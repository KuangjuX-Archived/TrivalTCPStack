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

Before create the server/client containers, you should know the **absolute path** of this project in your computer. 
You can use the `pwd` command to get the path.

And then we can run our container by static ip address and bind this project in a double-way between your host and containers:      

**NOTICE!** Replace {project path on your host} by your path:    

```shell
sudo docker run -itd --name server --net mynetwork --ip 10.0.0.3 --hostname server -v {project path on your host}:/tjutcp tcp /bin/bash
sudo docker run -itd --name client --net mynetwork --ip 10.0.0.2 --hostname client -v {project path on your host}:/tjutcp tcp /bin/bash
```

The following commands could attach you in to each container:
````shell
 sudo docker exec -it client  /bin/bash
 sudo docker exec -it server  /bin/bash
````
execute `make server` in server container and execute `make client` in client container under the `/tjutcp` folder, we can run the project easily.

## Workflows:
- [x] Environment
- [x] Thread Pool
- [x] Channel
- [x] Connection Management
- [x] Timer
- [x] System API: bind, listen, accept, connect, recv, send, close
- [x] Reliable Transmission
- [ ] Flow Control
- [ ] Congestion Control

## Differnes
We use `10.0.0.3` as server's ip address instead of `10.0.0.1` because this address is reported occupied.

## Other
- Once create the containers by the params `-itd`, they keep running in the background.  
- Get more information on the Docker website.
Destroy all running containers:     

```shell
sudo docker container prune
```

- Use valgrind to check the status of memory leak:
```shell
valgrind --tool=memcheck (--leak-check=full) {exectuable file}
```

## References
- RFC 793
- RFC 1122
- RFC 6298
- [C-Thread-Pool](https://github.com/Pithikos/C-Thread-Pool)
- [chan](https://github.com/tylertreat/chan)