import socket
import os
import sys

def get_host_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
    finally:
        s.close()

    return ip

local_ip = get_host_ip()

client_ip = "10.0.0.2"
server_ip = "10.0.0.3"





# print("====================================================================")
# print("============================开始测试================================")
# print("====================================================================")
# print("[自动测试] 编译提交源码")
# print("> cd /tju_tcp && make")
# print("[自动测试] 编译测试源码")
# print("> cd /tju_tcp/test && make")
# print("")
# print("[自动测试] 开启服务端和客户端 将输出重定向到文件")
# print("[自动测试] 等待8s测试结束 三次握手应该在8s内完成")  
# print("[自动测试] 打印文件里面的日志")
# print("====================================================================")
# print("Server wait connection......")
# print("Server connect established.")
# print("Client wait connection......")
# print("Client connect established.")
# print("")
# print("====================================================================")
# print("")
# print("[自动测试] 进行评分")
# print('{"scores": {"establish_connection": 100}}')
