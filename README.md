# TrivialTCP
Trivial TCP implement in C

## Requirements
- GCC
- Dokcer

## Start
In project directory, we can execute following command:
   
```shell
docker build -t tcp .
```
So we have a docker image named tcp.    
   

As follow, we have to create my own network configuration:  


```shell
docker network create --subnet=10.0.0.0/16 mynetwork
```
And then we can run our container by static ip address:     

```shell
sudo docker run -it --name server --net mynetwork --ip 10.0.0.3 --hostname server tcp
sudo docker run -it --name client --net mynetwork --ip 10.0.0.2 --hostname client tcp
```
execute `make server` in server container and execute `make client` in client container, we can run the project easily.

## Differnes
We use `10.0.0.3` as server's ip address instead of `10.0.0.1` because this address is reported occupied.

## Other
Destroy all running containers:     

```shell
sudo docker container prune
```

