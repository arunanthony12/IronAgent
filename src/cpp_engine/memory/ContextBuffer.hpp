#pragma once

#include <string_view>
#include <deque>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <stdexcept>

extern "C" {
    #include "allocator.h"
}

namespace coreagent {

enum class MessageRole { SYSTEM, USER, ASSISTANT, TOOL };

struct Message {
    MessageRole      role;
    std::string_view content;
    std::string_view tool_name;
    uint64_t         timestamp_ms;
};

// ContextBuffer v1.1
//
// Two arenas, two different lifetime shapes:
//
//  - retained_ / standby_ : hold Message content. Messages are evicted
//    FIFO (oldest first) from the front of the deque, but ca_arena_t can
//    only reclaim LIFO (via save/restore). To reclaim space from FIFO
//    eviction we do periodic *compaction*: copy the still-live messages
//    into standby_, swap retained_/standby_, and ca_arena_reset() the old
//    one. This is a semispace copy, same idea as a copying GC.
//
//  - scratch_ : holds the transient buildPrompt() output. We save a mark
//    at construction and ca_arena_restore() to it at the start of every
//    buildPrompt() call, so each call reuses the same space instead of
//    growing unbounded -- there is at most one "live" prompt at a time,
//    which matches how the hot loop actually consumes it.
class ContextBuffer {
public:
    // arena_capacity applies to BOTH retained_ and standby_ (they must be
    // equal size for the ping-pong swap to be safe). scratch_capacity is
    // separate and sized for one prompt's worth of text.
    ContextBuffer(size_t arena_capacity, size_t scratch_capacity, size_t max_tokens = 4096)
        : max_tokens_(max_tokens), current_tokens_(0) {
        retained_ = ca_arena_create(arena_capacity);
        standby_  = ca_arena_create(arena_capacity);
        scratch_  = ca_arena_create(scratch_capacity);
        if (!retained_ || !standby_ || !scratch_) {
            throw std::bad_alloc();
        }
        scratch_mark_ = ca_arena_save(scratch_);
    }

    ~ContextBuffer() {
        if (retained_) ca_arena_destroy(retained_);
        if (standby_)  ca_arena_destroy(standby_);
        if (scratch_)  ca_arena_destroy(scratch_);
    }

    // Not copyable (owns raw arenas); movable if you need it later.
    ContextBuffer(const ContextBuffer&) = delete;
    ContextBuffer& operator=(const ContextBuffer&) = delete;

    void addSystem   (std::string_view content) { push(MessageRole::SYSTEM, content); }
    void addUser     (std::string_view content) { push(MessageRole::USER, content); }
    void addAssistant(std::string_view content) { push(MessageRole::ASSISTANT, content); }

    void addToolResult(std::string_view tool_name, std::string_view content) {
        Message msg;
        msg.role         = MessageRole::TOOL;
        msg.content      = copyToArena(retained_, content);
        msg.tool_name    = copyToArena(retained_, tool_name);
        msg.timestamp_ms = nowMs();
        messages_.push_back(msg);
        current_tokens_ += estimateTokens(msg.content);
        trim();
    }

    // Builds into scratch_, rolling back to scratch_mark_ first so repeated
    // calls don't accumulate. Returned view is valid until the NEXT
    // buildPrompt() call (or destruction) -- caller should consume it
    // before calling again, which matches the Think->Plan->Act loop shape.
    std::string_view buildPrompt() {
        ca_arena_restore(scratch_, scratch_mark_);

        size_t total_len = 0;
        for (const auto& m : messages_) {
            total_len += m.content.size() + (m.role == MessageRole::TOOL ? 5 : 1) + 1;
        }

        char* out = static_cast<char*>(ca_arena_alloc(scratch_, total_len));
        if (!out) {
            // Scratch too small for this prompt -- caller's arena sizing
            // is wrong; surface it loudly rather than silently truncate.
            throw std::runtime_error("ContextBuffer: scratch arena too small for buildPrompt()");
        }

        char* cursor = out;
        for (const auto& m : messages_) {
            *cursor++ = '[';
            if (m.role == MessageRole::TOOL) {
                std::memcpy(cursor, "TOOL:", 5);
                cursor += 5;
            }
            std::memcpy(cursor, m.content.data(), m.content.size());
            cursor += m.content.size();
            *cursor++ = '\n';
        }

        return std::string_view(out, static_cast<size_t>(cursor - out));
    }

    const std::deque<Message>& messages()     const { return messages_; }
    size_t                     messageCount() const { return messages_.size(); }
    size_t                     tokenCount()   const { return current_tokens_; }

    void clear() {
        messages_.clear();
        current_tokens_ = 0;
        ca_arena_reset(retained_);
    }

    // Exposed for tuning/tests -- how full the retained arena is.
    size_t retainedBytesUsed() const { return ca_arena_used(retained_); }

    // Human-readable status line -- kept for compatibility with call
    // sites (Agent::run(), pybind bindings) written against the
    // pre-v1.1 API. Prefer messageCount()/tokenCount()/retainedBytesUsed()
    // directly for anything programmatic.
    void printSummary() const {
        printf("[ContextBuffer] messages=%zu tokens=%zu retained_bytes=%zu/%zu\n",
               messages_.size(), current_tokens_,
               ca_arena_used(retained_), retained_->capacity);
    }

private:
    ca_arena_t* retained_;
    ca_arena_t* standby_;
    ca_arena_t* scratch_;
    ca_arena_mark_t scratch_mark_;

    std::deque<Message> messages_;
    size_t max_tokens_;
    size_t current_tokens_;

    std::string_view copyToArena(ca_arena_t* arena, std::string_view text) {
        if (text.empty()) return {};
        char* ptr = static_cast<char*>(ca_arena_alloc(arena, text.size()));
        if (!ptr) {
            // Was previously an unchecked memcpy into a possibly-null
            // pointer. Compact and retry once before giving up -- a full
            // retained arena is exactly what compact() exists to fix.
            compact();
            ptr = static_cast<char*>(ca_arena_alloc(arena, text.size()));
            if (!ptr) {
                throw std::runtime_error("ContextBuffer: retained arena OOM after compaction");
            }
        }
        std::memcpy(ptr, text.data(), text.size());
        return std::string_view(ptr, text.size());
    }

    void push(MessageRole role, std::string_view content) {
        Message msg;
        msg.role         = role;
        msg.content      = copyToArena(retained_, content);
        msg.timestamp_ms = nowMs();
        messages_.push_back(msg);
        current_tokens_ += estimateTokens(msg.content);
        trim();
    }

    size_t estimateTokens(std::string_view text) const { return (text.size() + 3) / 4; }

    // Evicts oldest non-SYSTEM messages until back under max_tokens_.
    // This only shrinks the logical deque -- see compact() for actually
    // reclaiming the arena bytes those messages occupied.
    void trim() {
        while (current_tokens_ > max_tokens_ && messages_.size() > 1) {
            for (auto it = messages_.begin(); it != messages_.end(); ++it) {
                if (it->role != MessageRole::SYSTEM) {
                    current_tokens_ -= estimateTokens(it->content);
                    messages_.erase(it);
                    break;
                }
            }
        }
        // Once eviction has freed up a meaningful fraction of logical
        // tokens, the arena itself is still holding the dead bytes.
        // Compact opportunistically rather than waiting for OOM, so the
        // OOM path in copyToArena() is a safety net, not the normal case.
        if (ca_arena_used(retained_) > (retained_capacity() * 3) / 4) {
            compact();
        }
    }

    size_t retained_capacity() const { return retained_->capacity; }

    // Semispace copy: copy every still-live message's bytes into standby_,
    // rewriting Message::content/tool_name to point into standby_, then
    // swap retained_/standby_ and reset what was retained_ (now unused).
    void compact() {
        ca_arena_reset(standby_);
        for (auto& m : messages_) {
            if (!m.content.empty()) {
                m.content = copyToArena(standby_, m.content);
            }
            if (!m.tool_name.empty()) {
                m.tool_name = copyToArena(standby_, m.tool_name);
            }
        }
        std::swap(retained_, standby_);
        ca_arena_reset(standby_); // old retained_, now empty of live refs
    }

    uint64_t nowMs() const {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
};

} // namespace coreagent