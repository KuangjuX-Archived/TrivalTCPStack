FROM ubuntu:latest

RUN apt-get update \
    && apt install build-essential
    && python build.py