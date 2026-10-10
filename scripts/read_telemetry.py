#!/usr/bin/env python3
"""Read and decode Beaver Protocol telemetry from a PTY."""

import argparse
import os
import select
import struct
import sys
import termios
import tty


START_MARKER = 0xA5
PROTOCOL_VERSION = 1
TELEMETRY_MESSAGE_ID = 0x0200
TELEMETRY_PAYLOAD_SIZE = 6
HEADER_SIZE = 6
CRC_SIZE = 2


def crc16_ccitt_false(data):
	crc = 0xFFFF
	for byte in data:
		crc ^= byte << 8
		for _ in range(8):
			if crc & 0x8000:
				crc = ((crc << 1) ^ 0x1021) & 0xFFFF
			else:
				crc = (crc << 1) & 0xFFFF
	return crc


def decode_frames(buffer):
	while True:
		try:
			marker = buffer.index(START_MARKER)
		except ValueError:
			buffer.clear()
			return

		if marker:
			del buffer[:marker]
		if len(buffer) < HEADER_SIZE:
			return
		if buffer[1] != PROTOCOL_VERSION:
			del buffer[0]
			continue

		message_id, payload_length = struct.unpack_from("<HH", buffer, 2)
		if message_id != TELEMETRY_MESSAGE_ID or payload_length != TELEMETRY_PAYLOAD_SIZE:
			del buffer[0]
			continue

		frame_length = HEADER_SIZE + payload_length + CRC_SIZE
		if len(buffer) < frame_length:
			return

		frame = buffer[:frame_length]
		del buffer[:frame_length]
		received_crc = struct.unpack_from("<H", frame, HEADER_SIZE + payload_length)[0]
		if received_crc != crc16_ccitt_false(frame[1:HEADER_SIZE + payload_length]):
			continue

		values = struct.unpack_from("<hhh", frame, HEADER_SIZE)
		print(f"temperature={values[0]} power={values[1]} energy={values[2]}", flush=True)


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--port", default="/dev/pts/4", help="PTY device (default: %(default)s)")
	args = parser.parse_args()

	fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
	original_settings = termios.tcgetattr(fd)
	try:
		tty.setraw(fd)
		print(f"Reading telemetry from {args.port}; press Ctrl+C to stop", file=sys.stderr)
		buffer = bytearray()
		while True:
			readable, _, _ = select.select([fd], [], [])
			if readable:
				chunk = os.read(fd, 4096)
				if chunk:
					buffer.extend(chunk)
					decode_frames(buffer)
	finally:
		try:
			termios.tcsetattr(fd, termios.TCSADRAIN, original_settings)
		finally:
			os.close(fd)


if __name__ == "__main__":
	try:
		main()
	except KeyboardInterrupt:
		pass
	except OSError as error:
		print(f"Unable to read PTY: {error}", file=sys.stderr)
		sys.exit(1)