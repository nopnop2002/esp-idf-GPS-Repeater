#!/usr/bin/python
#-*- encoding: utf-8 -*-
import socket
import signal

global TaskRunning

def handler(signal, frame):
	global TaskRunning
	print('CNTL-C')
	TaskRunning = False


host = "192.168.10.131" # mDNS hostname
port = 5000

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

client.connect((host, port))

client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

TaskRunning = True
signal.signal(signal.SIGINT, handler)

while(TaskRunning):
	msg = client.recv(1024)
	print(msg.decode("utf-8"))

client.close()

