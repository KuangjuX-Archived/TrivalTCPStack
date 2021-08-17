FROM ubuntu:latest

RUN apt-get update \
    && apt install build-essential \
    && apt install python3.8 \
    && python build.py