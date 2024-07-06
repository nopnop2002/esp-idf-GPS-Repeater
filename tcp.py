#!/usr/bin/python
#-*- encoding: utf-8 -*-
import argparse
import socket
import signal

def handler(signal, frame):
	global running
	print('CNTL-C')
	running = False

if __name__ == '__main__':
	signal.signal(signal.SIGINT, handler)
	running = True

	parser = argparse.ArgumentParser()
	parser.add_argument('--host', help="TCP Server", default="esp32-server.local")
	parser.add_argument('--port', type=int, help="TCP Port", default=5000)
	args = parser.parse_args()
	print("args.host={}".format(args.host))
	print("args.port={}".format(args.port))

	client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	client.connect((args.host, args.port))
	client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

	while running:
		msg = client.recv(1024)
		#print("type(msg)={}".format(type(msg)))
		if (type(msg) is bytes):
			msg=msg.decode('utf-8')
		print(msg.strip())

	#print("socket close")
	client.close()

