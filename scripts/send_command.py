#!/usr/bin/env python3
"""Send a commander command over a Beaver Protocol v1 serial connection."""

import argparse
import os
import select
import struct
import sys
import termios
import time
import tty


START_MARKER = 0xA5
PROTOCOL_VERSION = 1
COMMAND_MESSAGE_ID = 0x0100
ACK_MESSAGE_ID = 0x0101
HEADER_SIZE = 6
CRC_SIZE = 2
MAX_PAYLOAD_SIZE = 1024
Q31_SCALE = 1 << 31
Q31_MAX = (1 << 31) - 1
Q31_FULL_SCALE_RPM = 1000.0

MODES = {
	"off": 0,
	"voltage": 1,
	"torque": 2,
	"speed": 3,
	"position": 4,
	"calibration": 5,
}

ACK_STATUSES = {
	0: "accepted",
	1: "invalid payload length",
	2: "invalid mode",
}


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


def encode_frame(message_id, payload):
	header = struct.pack("<BBHH", START_MARKER, PROTOCOL_VERSION, message_id, len(payload))
	frame_without_crc = header + payload
	crc = crc16_ccitt_false(frame_without_crc[1:])
	return frame_without_crc + struct.pack("<H", crc)


def take_ack_status(buffer):
	while True:
		try:
			marker = buffer.index(START_MARKER)
		except ValueError:
			buffer.clear()
			return None

		if marker:
			del buffer[:marker]
		if len(buffer) < HEADER_SIZE:
			return None
		if buffer[1] != PROTOCOL_VERSION:
			del buffer[0]
			continue

		message_id, payload_length = struct.unpack_from("<HH", buffer, 2)
		if payload_length > MAX_PAYLOAD_SIZE:
			del buffer[0]
			continue

		frame_length = HEADER_SIZE + payload_length + CRC_SIZE
		if len(buffer) < frame_length:
			return None

		frame = bytes(buffer[:frame_length])
		del buffer[:frame_length]
		received_crc = struct.unpack_from("<H", frame, HEADER_SIZE + payload_length)[0]
		if received_crc != crc16_ccitt_false(frame[1:HEADER_SIZE + payload_length]):
			continue
		if message_id == ACK_MESSAGE_ID and payload_length == 1:
			return frame[HEADER_SIZE]


def parse_signed_16(value):
	try:
		parsed = int(value, 0)
	except ValueError as error:
		raise argparse.ArgumentTypeError("must be an integer") from error
	if parsed < -32768 or parsed > 32767:
		raise argparse.ArgumentTypeError("must be in the range -32768 to 32767")
	return parsed


def parse_rpm(value):
	try:
		return float(value)
	except ValueError as error:
		raise argparse.ArgumentTypeError("must be a number") from error


def configure_serial(fd, baud):
	speed = getattr(termios, f"B{baud}", None)
	if speed is None:
		raise ValueError(f"unsupported baud rate: {baud}")

	tty.setraw(fd)
	settings = termios.tcgetattr(fd)
	settings[2] |= termios.CLOCAL | termios.CREAD
	settings[4] = speed
	settings[5] = speed
	termios.tcsetattr(fd, termios.TCSANOW, settings)


def write_all(fd, data, timeout):
	deadline = time.monotonic() + timeout
	remaining = memoryview(data)
	while remaining:
		time_left = deadline - time.monotonic()
		if time_left <= 0:
			raise TimeoutError("timed out writing command frame")
		_, writable, _ = select.select([], [fd], [], time_left)
		if not writable:
			raise TimeoutError("timed out writing command frame")
		written = os.write(fd, remaining)
		remaining = remaining[written:]


def read_ack(fd, timeout):
	deadline = time.monotonic() + timeout
	buffer = bytearray()
	while True:
		status = take_ack_status(buffer)
		if status is not None:
			return status

		time_left = deadline - time.monotonic()
		if time_left <= 0:
			raise TimeoutError("timed out waiting for commander acknowledgment")
		readable, _, _ = select.select([fd], [], [], time_left)
		if not readable:
			raise TimeoutError("timed out waiting for commander acknowledgment")
		chunk = os.read(fd, 4096)
		if chunk:
			buffer.extend(chunk)


def positive_float(value):
	try:
		parsed = float(value)
	except ValueError as error:
		raise argparse.ArgumentTypeError("must be a number") from error
	if parsed <= 0:
		raise argparse.ArgumentTypeError("must be greater than zero")
	return parsed


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--port", required=True, help="serial device, for example /dev/ttyUSB0")
	parser.add_argument("--baud", type=int, default=115200, help="serial baud rate (default: %(default)s)")
	parser.add_argument("--mode", choices=MODES, required=True)
	parser.add_argument("--setpoint", type=parse_rpm, required=True,
			    help="mechanical speed in RPM; converted to Q31")
	parser.add_argument("--current", type=parse_signed_16, required=True,
			    help="opaque signed current field")
	parser.add_argument("--timeout", type=positive_float, default=2.0,
			    help="write and acknowledgment timeout in seconds (default: %(default)s)")
	args = parser.parse_args()

	setpoint_q31 = round(args.setpoint / Q31_FULL_SCALE_RPM * Q31_SCALE)
	if setpoint_q31 < -Q31_SCALE or setpoint_q31 > Q31_MAX:
		parser.error("converted setpoint must be in the Q31 range [-1, 1)")

	payload = struct.pack("<Bih", MODES[args.mode], setpoint_q31, args.current)
	frame = encode_frame(COMMAND_MESSAGE_ID, payload)
	fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
	original_settings = termios.tcgetattr(fd)
	try:
		configure_serial(fd, args.baud)
		write_all(fd, frame, args.timeout)
		status = read_ack(fd, args.timeout)
	finally:
		try:
			termios.tcsetattr(fd, termios.TCSADRAIN, original_settings)
		finally:
			os.close(fd)

	status_text = ACK_STATUSES.get(status, f"unknown status {status}")
	if status != 0:
		print(f"Command rejected: {status_text}", file=sys.stderr)
		return 1
	print("Command accepted")
	return 0


if __name__ == "__main__":
	try:
		sys.exit(main())
	except (OSError, TimeoutError, ValueError) as error:
		print(f"Unable to send command: {error}", file=sys.stderr)
		sys.exit(1)