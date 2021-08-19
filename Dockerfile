FROM ubuntu:latest

RUN apt-get update \
    && apt install -y build-essential \
    && apt install -y python3.8