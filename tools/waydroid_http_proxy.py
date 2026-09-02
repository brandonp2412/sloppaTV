#!/usr/bin/env python3
"""Tiny forward proxy used by the Waydroid acceptance environment.

Waydroid on Glass is configured to use the host gateway at 192.168.240.1:8888.
This proxy intentionally tunnels HTTPS with CONNECT rather than intercepting TLS, so
Android clients still validate the destination server's real certificate.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import urllib.parse


async def relay(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    try:
        while data := await reader.read(64 * 1024):
            writer.write(data)
            await writer.drain()
    except (ConnectionError, asyncio.CancelledError):
        pass
    finally:
        with contextlib.suppress(Exception):
            writer.close()
            await writer.wait_closed()


async def tunnel(
    client_reader: asyncio.StreamReader,
    client_writer: asyncio.StreamWriter,
    host: str,
    port: int,
) -> None:
    try:
        upstream_reader, upstream_writer = await asyncio.open_connection(host, port)
    except OSError:
        client_writer.write(b"HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n")
        await client_writer.drain()
        client_writer.close()
        await client_writer.wait_closed()
        return

    client_writer.write(b"HTTP/1.1 200 Connection Established\r\n\r\n")
    await client_writer.drain()
    await asyncio.gather(
        relay(client_reader, upstream_writer),
        relay(upstream_reader, client_writer),
    )


async def forward_http(
    client_reader: asyncio.StreamReader,
    client_writer: asyncio.StreamWriter,
    method: str,
    target: str,
    version: str,
    header_lines: list[bytes],
) -> None:
    parsed = urllib.parse.urlsplit(target)
    if not parsed.hostname:
        client_writer.write(b"HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n")
        await client_writer.drain()
        client_writer.close()
        await client_writer.wait_closed()
        return

    port = parsed.port or (443 if parsed.scheme == "https" else 80)
    try:
        upstream_reader, upstream_writer = await asyncio.open_connection(parsed.hostname, port)
    except OSError:
        client_writer.write(b"HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n")
        await client_writer.drain()
        client_writer.close()
        await client_writer.wait_closed()
        return

    path = urllib.parse.urlunsplit(("", "", parsed.path or "/", parsed.query, ""))
    upstream_writer.write(f"{method} {path} {version}\r\n".encode("ascii"))
    for line in header_lines:
        lower = line.lower()
        if lower.startswith(b"proxy-connection:"):
            continue
        upstream_writer.write(line)
    upstream_writer.write(b"\r\n")
    await upstream_writer.drain()

    await asyncio.gather(
        relay(client_reader, upstream_writer),
        relay(upstream_reader, client_writer),
    )


async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    try:
        request_line = await asyncio.wait_for(reader.readline(), timeout=10)
        if not request_line:
            writer.close()
            await writer.wait_closed()
            return
        parts = request_line.decode("latin-1").rstrip("\r\n").split(" ", 2)
        if len(parts) != 3:
            raise ValueError("invalid request line")
        method, target, version = parts

        headers: list[bytes] = []
        while True:
            line = await asyncio.wait_for(reader.readline(), timeout=10)
            if line in {b"\r\n", b"\n", b""}:
                break
            headers.append(line)

        if method.upper() == "CONNECT":
            host, separator, port_text = target.rpartition(":")
            if not separator or not host:
                raise ValueError("invalid CONNECT target")
            await tunnel(reader, writer, host, int(port_text))
        else:
            await forward_http(reader, writer, method, target, version, headers)
    except (ValueError, asyncio.TimeoutError):
        with contextlib.suppress(Exception):
            writer.write(b"HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n")
            await writer.drain()
            writer.close()
            await writer.wait_closed()


async def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8888)
    args = parser.parse_args()

    server = await asyncio.start_server(handle_client, args.host, args.port)
    sockets = ", ".join(str(sock.getsockname()) for sock in server.sockets or [])
    print(f"Waydroid proxy listening on {sockets}", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
