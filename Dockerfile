FROM ubuntu:latest

WORKDIR /app

COPY . .

RUN apt-get update \
    && apt install -y build-essential \
    && apt install -y python3.8 \
    && cd /app \
    && python3.8 build.py