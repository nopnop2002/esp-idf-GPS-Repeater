#!/usr/bin/python
#-*- encoding: utf-8 -*-
import socket
import signal

global TaskRunning

def handler(signal, frame):
	global TaskRunning
	print('CNTL-C')
	TaskRunning = False


host = "esp32-server.local" # mDNS hostname
port = 5000

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

client.connect((host, port))

client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

TaskRunning = True
signal.signal(signal.SIGINT, handler)

while(TaskRunning):
	msg = client.recv(1024)
	#print("type(msg)={}".format(type(msg)))
	if (type(msg) is bytes):
		msg=msg.decode('utf-8')
		print(msg.strip())

client.close()

