FROM ubuntu:latest

WORKDIR /app

COPY . .

RUN apt-get update \
    && apt install build-essential \
    && apt install python3.8 \
    && cd /app \
    && python build.py