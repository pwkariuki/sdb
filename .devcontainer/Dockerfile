FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    openssh-server \
    rsync \
    ninja-build \
    libdwarf-dev \
    libelf-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# CLion needs SSH to connect as a remote toolchain
RUN mkdir /var/run/sshd
RUN echo 'root:password' | chpasswd
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed -i 's/#StrictModes yes/StrictModes no/' /etc/ssh/sshd_config

EXPOSE 22

CMD ["/usr/sbin/sshd", "-D"]
