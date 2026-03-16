#
# Dockerfile for Arcus Hub OS Yocto (Scarthgap 5.0) builds
#
# Supports both Hub V2 (TI AM335x) and Hub V3 (NXP i.MX6) targets.
#
# Build:   docker build -t arcushubos .
# Run:     ./docker-build.sh hubv2-dev
#

FROM ubuntu:22.04

# Avoid interactive prompts during package install
ENV DEBIAN_FRONTEND=noninteractive

# Yocto Scarthgap host dependencies (from Yocto docs + project README)
RUN apt-get update && apt-get install -y \
    gawk wget git git-lfs diffstat unzip texinfo \
    gcc g++ gcc-multilib g++-multilib \
    build-essential chrpath socat cpio \
    python3 python3-pip python3-pexpect python3-subunit \
    xz-utils debianutils iputils-ping \
    libsdl1.2-dev xterm \
    lz4 zstd liblz4-tool patchelf \
    srecord \
    openjdk-11-jdk-headless \
    file locales sudo tmux \
    iproute2 ca-certificates curl \
    libegl1-mesa libsdl2-dev \
    pylint python3-git python3-jinja2 \
    && rm -rf /var/lib/apt/lists/*

# Yocto needs a UTF-8 locale
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Yocto refuses to run as root — create a build user
ARG UID=1000
ARG GID=1000
RUN groupadd -g ${GID} builder 2>/dev/null || true && \
    useradd -m -u ${UID} -g ${GID} -s /bin/bash builder && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Create sstate-cache directory at the path local.conf expects
RUN mkdir -p /build/sstate-cache && chown builder:builder /build/sstate-cache

# create_update_file scripts copy output to /tftpboot
RUN mkdir -p /tftpboot && chown builder:builder /tftpboot

# jlink is required by HOSTTOOLS for building trimmed OpenJDK runtime
RUN ln -sf /usr/lib/jvm/java-11-openjdk-amd64/bin/jlink /usr/local/bin/jlink

USER builder
WORKDIR /home/builder/arcushubos

# Git safe directory — the mounted repo will be owned by the host user
RUN git config --global --add safe.directory /home/builder/arcushubos

ENTRYPOINT ["/bin/bash", "-c"]
CMD ["bash"]
