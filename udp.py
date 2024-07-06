#!/usr/bin/python
#-*- encoding: utf-8 -*-
import argparse
import select, socket
import signal

def handler(signal, frame):
	global running
	print('handler')
	running = False

if __name__ == '__main__':
	signal.signal(signal.SIGINT, handler)
	running = True

	parser = argparse.ArgumentParser()
	parser.add_argument('--port', type=int, help="UDP Port", default=9000)
	args = parser.parse_args()
	print("args.port={}".format(args.port))

	client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	client.bind(('<broadcast>', args.port))
	client.setblocking(0)

	while running:
		result = select.select([client],[],[])
		msg = result[0][0].recv(1024)
		#print("type(msg)={}".format(type(msg)))
		if (type(msg) is bytes):
			msg=msg.decode('utf-8')
		print(msg.strip())

	#print("socket close")
	client.close()
