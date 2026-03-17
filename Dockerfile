FROM ubuntu:latest

LABEL maintainer="cpunchline@foxmail.com"
LABEL version="0.0.1"
LABEL description="This is custom Docker Image for the C/C++ Services by cpunchline."
ARG DEBIAN_FRONTEND=noninteractive

WORKDIR /home/ubuntu

RUN  apt-get update && apt-get upgrade
RUN  apt-get -y install build-essential cmake ccache pkg-config ninja-build gdb
RUN  apt-get -y install llvm clangd clang-format clang-tidy
RUN  apt-get -y install git curl wget python-is-python3
RUN  apt-get -y install vim tree zip rar unrar cloc cppcheck
RUN  apt-get -y install protobuf-compiler python3-protobuf

RUN  apt-get -y install libsqlite3-dev libcjson-dev libgtest-dev libgmock-dev liburing-dev

# docker build -t ubuntu:cpunchline .
