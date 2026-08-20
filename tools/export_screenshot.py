#!/usr/bin/env python3
import argparse
import binascii
import struct
import time
import zlib

import serial

COMMAND = b"POMO1 SCREENSHOT\n"
HEADER_MAGIC = b"POMO1"
CHUNK_MAGIC = b"PX"
DONE_MAGIC = b"DONE"
BYTES_PER_PIXEL = 2
DIMENSIONS_FORMAT = "<HH"
CHUNK_HEADER_FORMAT = "<2sHHHH"


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(
        ">I", binascii.crc32(kind + data) & 0xFFFFFFFF
    )


def write_png(path, width, height, rgb565):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            offset = (y * width + x) * 2
            value = rgb565[offset] | rgb565[offset + 1] << 8
            rows.extend((
                ((value >> 11) & 31) * 255 // 31,
                ((value >> 5) & 63) * 255 // 63,
                (value & 31) * 255 // 31,
            ))
    data = b"\x89PNG\r\n\x1a\n"
    data += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += png_chunk(b"IDAT", zlib.compress(rows, 9))
    data += png_chunk(b"IEND", b"")
    with open(path, "wb") as output:
        output.write(data)


def read_exact(connection, buffer, size, deadline):
    while len(buffer) < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("device screenshot timed out")
        connection.timeout = remaining
        data = connection.read(size - len(buffer))
        if not data:
            raise TimeoutError("device screenshot timed out")
        buffer.extend(data)
    result = bytes(buffer[:size])
    del buffer[:size]
    return result


def main():
    parser = argparse.ArgumentParser(description="Export the Pomodoro display as PNG")
    parser.add_argument("port")
    parser.add_argument("output")
    args = parser.parse_args()

    with serial.Serial(args.port, 115200, timeout=1, write_timeout=5) as connection:
        connection.reset_input_buffer()
        deadline = time.monotonic() + 180
        connection.write(COMMAND)
        connection.flush()
        buffer = bytearray()
        while True:
            buffer.extend(read_exact(connection, bytearray(), 1, deadline))
            marker = buffer.find(HEADER_MAGIC)
            if marker >= 0:
                del buffer[:marker]
                break
            if len(buffer) >= len(HEADER_MAGIC):
                del buffer[:-(len(HEADER_MAGIC) - 1)]
        header = read_exact(
            connection,
            buffer,
            len(HEADER_MAGIC) + struct.calcsize(DIMENSIONS_FORMAT),
            deadline,
        )
        width, height = struct.unpack(
            DIMENSIONS_FORMAT, header[len(HEADER_MAGIC):]
        )
        pixels = bytearray(width * height * BYTES_PER_PIXEL)
        written = 0
        while written < width * height:
            chunk = read_exact(
                connection, buffer, struct.calcsize(CHUNK_HEADER_FORMAT), deadline
            )
            chunk_magic, x, y, chunk_width, chunk_height = struct.unpack(
                CHUNK_HEADER_FORMAT, chunk
            )
            if chunk_magic != CHUNK_MAGIC:
                raise RuntimeError("corrupt screenshot stream")
            if (chunk_width == 0 or chunk_height == 0 or
                    x + chunk_width > width or y + chunk_height > height):
                raise RuntimeError(
                    f"invalid screenshot chunk {x},{y} {chunk_width}x{chunk_height}"
                )
            data = read_exact(
                connection,
                buffer,
                chunk_width * chunk_height * BYTES_PER_PIXEL,
                deadline,
            )
            for row in range(chunk_height):
                source = row * chunk_width * BYTES_PER_PIXEL
                destination = ((y + row) * width + x) * BYTES_PER_PIXEL
                row_bytes = chunk_width * BYTES_PER_PIXEL
                pixels[destination:destination + row_bytes] = data[source:source + row_bytes]
            written += chunk_width * chunk_height
            print(f"\r{written * 100 // (width * height)}%", end="", flush=True)
        if read_exact(connection, buffer, len(DONE_MAGIC), deadline) != DONE_MAGIC:
            raise RuntimeError("screenshot did not complete")
        write_png(args.output, width, height, pixels)
        print(f"\r{args.output}")


if __name__ == "__main__":
    main()
