#!/usr/bin/env python3
"""A deterministic, unauthenticated-to-start Jellyfin fixture for CI screenshots."""
from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def item(item_id: str, name: str, item_type: str = "Movie") -> dict[str, object]:
    return {
        "Id": item_id,
        "Name": name,
        "Type": item_type,
        "Overview": f"A featured {item_type.lower()} in the sloppaTV screenshot catalog.",
        "ProductionYear": 2026,
        "CommunityRating": 8.4,
        "Genres": ["Adventure", "Drama"],
        "ImageTags": {},
        "UserData": {"IsFavorite": True},
    }


MOVIES = [item("movie-1", "Big Buck Bunny"), item("movie-2", "Caminandes"), item("movie-3", "Sintel")]
VIEWS = [dict(item("movies", "Movies", "CollectionFolder"), CollectionType="movies")]


class Handler(BaseHTTPRequestHandler):
    server_version = "sloppaTVScreenshotFixture/1.0"

    def log_message(self, fmt: str, *args: object) -> None:
        print(fmt % args, flush=True)

    def send_json(self, payload: object) -> None:
        data = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self) -> None:  # noqa: N802
        if self.path.startswith("/Users/AuthenticateByName"):
            self.send_json({"AccessToken": "fixture-token", "User": {"Id": "fixture-user", "Name": "Viewer"}, "ServerId": "fixture-server"})
            return
        self.send_json({})

    def do_GET(self) -> None:  # noqa: N802
        path = self.path.split("?", 1)[0]
        if path == "/System/Info/Public":
            self.send_json({
                "Id": "fixture-server",
                "ServerName": "sloppaTV Demo Library",
                "Version": "10.10.0",
                "ProductName": "Jellyfin",
                "OperatingSystem": "Linux",
            })
        elif path.endswith("/Views"):
            self.send_json({"Items": VIEWS, "TotalRecordCount": len(VIEWS)})
        elif path.endswith("/Items/Resume") or path == "/Shows/NextUp":
            self.send_json({"Items": MOVIES[:2], "TotalRecordCount": 2})
        elif path.endswith("/Items/Latest"):
            self.send_json(MOVIES)
        elif "/Items/" in path and path.rsplit("/", 1)[-1].startswith("movie-"):
            selected = next((value for value in MOVIES if value["Id"] == path.rsplit("/", 1)[-1]), MOVIES[0])
            self.send_json(selected)
        elif path.endswith("/Items") or path == "/Items":
            self.send_json({"Items": MOVIES, "TotalRecordCount": len(MOVIES)})
        elif path.endswith("/Similar"):
            self.send_json({"Items": MOVIES[1:], "TotalRecordCount": 2})
        else:
            self.send_json({"Items": [], "TotalRecordCount": 0})


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18096)
    args = parser.parse_args()
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
