#!/usr/bin/env python3

import argparse
import json
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


class RetryFixtureHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    failed_views_once = False
    fail_views_once = True
    server_version = "10.11.11"

    def log_message(self, fmt: str, *args: object) -> None:
        print(f"{self.command} {self.path} - {fmt % args}", flush=True)

    def send_json(self, payload: object, status: int = 200) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlsplit(self.path).path
        if path == "/System/Info/Public":
            self.send_json({
                "Id": "sloppatv-retry-fixture",
                "ServerName": "sloppaTV Retry Fixture",
                "Version": type(self).server_version,
                "ProductName": "Jellyfin Server",
                "OperatingSystem": "Linux",
            })
            return

        if path == "/Users/retry-user/Views":
            if type(self).fail_views_once and not type(self).failed_views_once:
                type(self).failed_views_once = True
                print("INTENTIONAL_ABORT /Users/retry-user/Views", flush=True)
                try:
                    self.connection.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                self.connection.close()
                return
            self.send_json({
                "Items": [{
                    "Id": "retry-library",
                    "Name": "Retry Verified",
                    "Type": "CollectionFolder",
                    "CollectionType": "movies",
                }]
            })
            return

        if path.startswith("/Users/retry-user/Items") or path == "/Shows/NextUp":
            self.send_json({"Items": []})
            return

        self.send_json({"Items": []})


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18096)
    parser.add_argument("--version", default="10.11.11")
    parser.add_argument("--no-fail-views", action="store_true")
    parser.add_argument("--duration-seconds", type=float, default=0.0)
    args = parser.parse_args()
    RetryFixtureHandler.failed_views_once = False
    RetryFixtureHandler.fail_views_once = not args.no_fail_views
    RetryFixtureHandler.server_version = args.version
    server = ThreadingHTTPServer((args.bind, args.port), RetryFixtureHandler)
    if args.duration_seconds > 0:
        timer = threading.Timer(args.duration_seconds, server.shutdown)
        timer.daemon = True
        timer.start()
    print(f"READY http://{args.bind}:{args.port} version={args.version}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
