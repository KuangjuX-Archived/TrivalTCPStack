FROM ubuntu:latest

ARG DEBIAN_FRONTED=noninteractive
ENV TZ=Europe/Moscow

RUN apt-get update \
    && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
    && apt install -y python3.8 \
    && apt install -y python3-pip \
    && pip install fabric \
    && apt install -y build-essential \
    && apt-get install -y valgrind \
    && apt-get install -y gdb \
    && apt install -y tmux \
    && apt install -y openssh-server \
    && apt install -y openssh-client \
    && apt install -y vim \
    && apt-get install python3 -y \
    && apt-get install python3-pip -y \
    && pip install tcconfig \
    && apt-get install tcpdump -y \
    && apt-get install sudo -y \
    && apt-get install net-tools -y \
    && apt-get install iproute2 -y \
    && pip install scapy \
    && pip install matplotlib 

EXPOSE 22