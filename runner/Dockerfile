FROM ubuntu:26.04

# Prevent interactive prompts from blocking apt during build passes
ENV DEBIAN_FRONTEND=noninteractive

# Install only the essentials: GCC, Clang, CMake, Ninja, and Git (Git is required for actions/checkout)
RUN apt-get update && apt-get install -y \
    build-essential \
    clang \
    cmake \
    ninja-build \
    git \
    nodejs \
    clang-format \
    clang-tidy \
    valgrind \
    ccache \
    libssl-dev \
    libsqlite3-dev \
    libboost-all-dev \
    g++ \
    g++-16 \
    cmake-format \
    && rm -rf /var/lib/apt/lists/*

# Set default working directory path inside container filesystem
WORKDIR /workspace

