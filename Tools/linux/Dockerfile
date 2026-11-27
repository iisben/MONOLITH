# Ubuntu 22.04 (glibc 2.35) x86_64 build image for the MONOLITH Linux VST3 / Standalone. See Tools/build_linux_docker.sh.
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++-12 cmake ninja-build pkg-config ca-certificates file \
        libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
        libgl1-mesa-dev xvfb xauth fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*
ENV CC=gcc-12 CXX=g++-12
WORKDIR /src
