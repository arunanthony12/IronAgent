from setuptools import setup, find_packages
import os

setup(
    name             = "coreagent",
    version          = "0.1.0",
    author           = "CoreAgent Contributors",
    description      = "Bare-Metal AI Agent Runtime — C + C++ + Python",
    long_description = open("README.md").read() if os.path.exists("README.md") else "",
    packages         = find_packages(where="python"),
    package_dir      = {"": "python"},
    python_requires  = ">=3.8",
    install_requires = [
        "requests>=2.28.0",
    ],
    extras_require = {
        "dev": ["pytest", "pytest-cov"],
    },
    entry_points = {
        "console_scripts": [
            "coreagent=coreagent.cli:main",
        ],
    },
    classifiers = [
        "Programming Language :: Python :: 3",
        "Programming Language :: C",
        "Programming Language :: C++",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "License :: OSI Approved :: MIT License",
        "Operating System :: POSIX :: Linux",
    ],
)