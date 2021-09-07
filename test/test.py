# -*- coding: UTF-8 -*-

import pdb
import socket
from fabric import Connection

start_server_cmd = 'tmux new -s pytest_server -d "bash -c /tju_tcp/test/receiver > /tju_tcp/test/server.log 2>&1"'
start_client_cmd = 'tmux new -s pytest_client -d "bash -c /tju_tcp/test/test_client > /tju_tcp/test/client.log 2>&1"'
stop_server_cmd = 'tmux kill-session -t pytest_server'
stop_client_cmd = 'tmux kill-session -t pytest_client'

def main():
    
    if socket.gethostname() == "server":
        print("只能在client端运行自动测试")
        exit()
        
    print("====================================================================")
    print("============================开始测试================================")
    print("====================================================================")
    with Connection(host="10.0.0.3", user='root', connect_kwargs={'password':''}) as conn:

        print("[自动测试] 编译提交源码")
        print("> cd /tju_tcp && make")
        rst = conn.run("cd /tju_tcp && make", timeout=5)
        if (rst.failed):
            print('[自动测试] 编译提交源码错误 停止测试')
            print('{"scores": {"establish_connection": 0}}')
            exit()
        print("")
        

        print("[自动测试] 编译测试源码")
        print("> cd /tju_tcp/test && make")
        rst = conn.run("cd /tju_tcp/test && make", timeout=5)
        if (rst.failed):
            print('[自动测试] 编译测试源码错误 停止测试')
            print('{"scores": {"establish_connection": 0}}')
            exit()    
        print("")


        print("[自动测试] 开启服务端和客户端 将输出重定向到文件")
        conn.run(start_server_cmd)
        conn.local(start_client_cmd)
        print("")


        print("[自动测试] 等待8s测试结束 三次握手应该在8s内完成")
        conn.run('sleep 8')
        print("")


        try:
            conn.local('tmux has-session -t pytest_client', hide=True)
            conn.local(stop_client_cmd)
        except Exception as e:
            pass 
        try:
            conn.run('tmux has-session -t pytest_server', hide=True)
            conn.run(stop_server_cmd)
        except Exception as e: 
            pass 

        cat_log_server_cmd = 'cat /tju_tcp/test/server.log'
        cat_log_client_cmd = 'cat /tju_tcp/test/client.log'

        print("[自动测试] 打印文件里面的日志")
        print("====================================================================")
        server_log = conn.run(cat_log_server_cmd)
        client_log = conn.local(cat_log_client_cmd)
        print("====================================================================")
        print("")

        print("[自动测试] 进行评分")
        if "{{TEST SUCCESS}}" in server_log.stdout:
            print('{"scores": {"establish_connection": 100}}')
        elif "{{TEST FAILED}}" in server_log.stdout:
            score_count = server_log.stdout.count("{{GET SCORE}}")
            final_score = score_count*50
            print('{"scores": {"establish_connection": %s}}'%final_score)
        else:
            print('[自动测试] 测试结果解析出错')
            print('{"scores": {"establish_connection": 0}}')

            

if __name__ == "__main__":
    main()
