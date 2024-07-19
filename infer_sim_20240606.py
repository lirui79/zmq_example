'''
Author: yangyaozong yangyaozong@smartcore-core.cn
Date: 2024-05-16 14:42:27
LastEditors: yangyaozong yangyaozong@smartcore-core.cn
LastEditTime: 2024-06-05 17:22:18
FilePath: /sift_expir/mq/infer_sim.py
Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
'''
import zmq
import os
import json
import time
import queue
import threading
import sys

msgs_q = queue.Queue(32)

def zmq_server():
    context = zmq.Context()
    socket = context.socket(zmq.REP)
    socket.bind("tcp://*:8003")
    while True:
        msg = socket.recv_string()
        msgs_q.put(msg)
        socket.send_string("0")

def handle_thread():
    print("start handle decode data")
    while True:
        msg = msgs_q.get()
        # print(msg)
        task = dict(json.loads(msg))
        os.remove(task['data'][0])

if __name__ == "__main__":
    print("-------------")
    for _ in range(int(sys.argv[1])):
        threading.Thread(target=handle_thread).start()

    zmq_server()