#!/usr/bin/python
#-*- encoding: utf-8 -*-
# python3 -m pip install git+https://github.com/inmcm/micropyGPS.git
import argparse
import select, socket
import signal
import micropyGPS
import threading

def handler(signal, frame):
	global running
	print('handler')
	running = False

if __name__ == '__main__':
	signal.signal(signal.SIGINT, handler)
	running = True

	parser = argparse.ArgumentParser()
	parser.add_argument('--port', type=int, help="UDP Port", default=9000)
	parser.add_argument('--timezone', type=int, help="Timezone", default=0)
	args = parser.parse_args()
	print("args.port={}".format(args.port))
	print("args.timezone={}".format(args.timezone))

	client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	client.bind(('<broadcast>', args.port))
	client.setblocking(0)

	gps = micropyGPS.MicropyGPS(args.timezone, 'dd')

	stdout = True
	while running:
		result = select.select([client],[],[])
		msg = result[0][0].recv(1024)
		#print("type(msg)={}".format(type(msg)))
		if (type(msg) is bytes):
			msg=msg.decode('utf-8')
		sentence=msg.strip()
		if stdout:
			print(sentence)
		if (sentence[0] != '$'):
			continue
		for x in sentence:
			gps.update(x)

		if gps.clean_sentences > 20:
			stdout = False
			h = gps.timestamp[0] if gps.timestamp[0] < 24 else gps.timestamp[0] - 24
			print('%2d:%02d:%04.1f' % (h, gps.timestamp[1], gps.timestamp[2]))
			print('latitude and longitude: %2.8f, %2.8f' % (gps.latitude[0], gps.longitude[0]))
			print('altitude: %f' % gps.altitude)
			print('satellites_used: {}'.format(gps.satellites_used))
			print('satellite number: (elevation, azimuth, S/N ratio)')
			for k, v in gps.satellite_data.items():
				print('%d: %s' % (k, v))
			print('')

	#print("socket close")
	client.close()
