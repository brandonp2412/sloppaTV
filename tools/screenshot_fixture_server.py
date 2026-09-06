#!/usr/bin/env python3
"""Deterministic Jellyfin fixture used by the Android TV visual screenshot suite."""
from __future__ import annotations

import argparse
import json
import mimetypes
import re
from copy import deepcopy
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

FIXTURE_ROOT = Path(__file__).resolve().parent / "screenshot-fixtures"
ARTWORK_ROOT = FIXTURE_ROOT / "artwork"
MEDIA_ROOT = FIXTURE_ROOT / "media"
TICKS_PER_SECOND = 10_000_000


def ticks(seconds: int) -> int:
    return seconds * TICKS_PER_SECOND


def user_data(*, favorite: bool = False, played: bool = False, position_seconds: int = 0) -> dict[str, object]:
    return {
        "IsFavorite": favorite,
        "Played": played,
        "PlaybackPositionTicks": ticks(position_seconds),
    }


def image_tags(item_id: str, *, thumb: bool = True) -> dict[str, str]:
    tags = {"Primary": f"{item_id}-primary"}
    if thumb:
        tags["Thumb"] = f"{item_id}-thumb"
    return tags


def media_source(item_id: str) -> dict[str, object]:
    return {
        "Id": f"{item_id}-source",
        "Container": "mp4",
        "SupportsDirectPlay": True,
        "SupportsDirectStream": True,
        "SupportsTranscoding": False,
        "DefaultAudioStreamIndex": -1,
        "DefaultSubtitleStreamIndex": -1,
        "MediaStreams": [
            {
                "Type": "Video",
                "Index": 0,
                "Codec": "h264",
                "Profile": "High",
                "Level": 31,
                "VideoRangeType": "SDR",
                "Width": 1280,
                "Height": 720,
                "BitDepth": 8,
                "RealFrameRate": 24.0,
            }
        ],
    }


def actor(person_id: str, name: str, role: str) -> dict[str, object]:
    return {
        "Id": person_id,
        "Name": name,
        "Type": "Actor",
        "Role": role,
        "PrimaryImageTag": f"{person_id}-primary",
    }


def movie(
    item_id: str,
    name: str,
    *,
    year: int,
    runtime_seconds: int,
    overview: str,
    rating: float,
    genres: list[str],
    official_rating: str,
    favorite: bool = False,
    played: bool = False,
    position_seconds: int = 0,
) -> dict[str, object]:
    return {
        "Id": item_id,
        "Name": name,
        "Type": "Movie",
        "Overview": overview,
        "ProductionYear": year,
        "OfficialRating": official_rating,
        "CommunityRating": rating,
        "Genres": genres,
        "RunTimeTicks": ticks(runtime_seconds),
        "Container": "mp4",
        "ImageTags": image_tags(item_id),
        "BackdropImageTags": [f"{item_id}-backdrop"],
        "People": [
            actor("person-ari", "Ari Vale", "Lead explorer"),
            actor("person-mina", "Mina Koro", "Forest neighbour"),
            actor("person-jon", "Jon Ember", "Inventor"),
        ],
        "UserData": user_data(favorite=favorite, played=played, position_seconds=position_seconds),
        "MediaSources": [media_source(item_id)],
    }


BIG_BUCK_BUNNY = movie(
    "movie-big-buck-bunny",
    "Big Buck Bunny",
    year=2008,
    runtime_seconds=634,
    overview="A gentle giant of the forest decides that three tiny troublemakers have pushed their luck too far, setting up a playful animated revenge story.",
    rating=8.1,
    genres=["Animation", "Comedy", "Family"],
    official_rating="PG",
    favorite=True,
)
SINTEL = movie(
    "movie-sintel",
    "Sintel",
    year=2010,
    runtime_seconds=888,
    overview="A lone traveller crosses a harsh fantasy landscape while searching for the dragon she once rescued, with the journey revealing more than she expects.",
    rating=8.3,
    genres=["Animation", "Fantasy", "Adventure"],
    official_rating="PG",
    played=True,
)
TEARS_OF_STEEL = movie(
    "movie-tears-of-steel",
    "Tears of Steel",
    year=2012,
    runtime_seconds=734,
    overview="Scientists and fighters reunite in a futuristic Amsterdam to confront destructive machines and an old mistake before the city is overwhelmed.",
    rating=7.8,
    genres=["Science Fiction", "Action", "Drama"],
    official_rating="PG-13",
)
MOVIES = [BIG_BUCK_BUNNY, SINTEL, TEARS_OF_STEEL]

CAMINANDES = {
    "Id": "series-caminandes",
    "Name": "Caminandes",
    "Type": "Series",
    "Overview": "Koro is a stubborn Patagonian llama whose simple plans repeatedly become elaborate slapstick problems involving fences, food, ice and other animals.",
    "ProductionYear": 2013,
    "OfficialRating": "G",
    "CommunityRating": 8.7,
    "Genres": ["Animation", "Comedy", "Family"],
    "ImageTags": image_tags("series-caminandes"),
    "BackdropImageTags": ["series-caminandes-backdrop"],
    "People": [
        actor("person-koro", "Koro", "Persistent llama"),
        actor("person-pichin", "Pichin", "Armadillo neighbour"),
        actor("person-oti", "Oti", "Penguin companion"),
    ],
    "UserData": user_data(favorite=True),
}
SEASON_ONE = {
    "Id": "season-caminandes-1",
    "Name": "Season 1",
    "Type": "Season",
    "SeriesId": CAMINANDES["Id"],
    "SeriesName": CAMINANDES["Name"],
    "IndexNumber": 1,
    "ProductionYear": 2013,
    "ImageTags": image_tags("season-caminandes-1", thumb=False),
    "BackdropImageTags": ["series-caminandes-backdrop"],
    "UserData": user_data(),
}


def episode(item_id: str, name: str, index: int, year: int, runtime_seconds: int, overview: str, *, position_seconds: int = 0, played: bool = False) -> dict[str, object]:
    return {
        "Id": item_id,
        "Name": name,
        "Type": "Episode",
        "SeriesId": CAMINANDES["Id"],
        "SeriesName": CAMINANDES["Name"],
        "SeriesPrimaryImageTag": "series-caminandes-primary",
        "SeasonName": SEASON_ONE["Name"],
        "IndexNumber": index,
        "ParentIndexNumber": 1,
        "Overview": overview,
        "ProductionYear": year,
        "OfficialRating": "G",
        "CommunityRating": 8.5 + index * 0.1,
        "Genres": ["Animation", "Comedy", "Family"],
        "RunTimeTicks": ticks(runtime_seconds),
        "Container": "mp4",
        "ImageTags": image_tags(item_id),
        "ParentBackdropImageTags": ["series-caminandes-backdrop"],
        "ParentBackdropItemId": CAMINANDES["Id"],
        "People": CAMINANDES["People"],
        "UserData": user_data(played=played, position_seconds=position_seconds),
        "MediaSources": [media_source(item_id)],
    }


EPISODES = [
    episode(
        "episode-llama-drama",
        "Llama Drama",
        1,
        2013,
        90,
        "Koro spots a tempting meal across the road and discovers that the shortest route can produce the longest string of problems.",
        played=True,
    ),
    episode(
        "episode-gran-dillama",
        "Gran Dillama",
        2,
        2013,
        146,
        "An electric fence stands between Koro and greener grass, so a simple snack becomes an increasingly inventive battle of persistence.",
        position_seconds=61,
    ),
    episode(
        "episode-llamigos",
        "Llamigos",
        3,
        2016,
        150,
        "A frozen landscape and a group of penguins turn Koro's latest expedition into a fast, slippery test of balance and friendship.",
    ),
]

VIEWS = [
    {
        "Id": "movies",
        "Name": "Open Movies",
        "Type": "CollectionFolder",
        "CollectionType": "movies",
        "ImageTags": image_tags("movies", thumb=False),
    },
    {
        "Id": "shows",
        "Name": "Open Series",
        "Type": "CollectionFolder",
        "CollectionType": "tvshows",
        "ImageTags": image_tags("shows", thumb=False),
    },
]

ALL_ITEMS = {str(value["Id"]): value for value in [*MOVIES, CAMINANDES, SEASON_ONE, *EPISODES, *VIEWS]}

PRIMARY_ART = {
    "movies": "movies-library.jpg",
    "shows": "shows-library.jpg",
    BIG_BUCK_BUNNY["Id"]: "big-buck-bunny-poster.jpg",
    SINTEL["Id"]: "sintel-poster.jpg",
    TEARS_OF_STEEL["Id"]: "tears-of-steel-poster.png",
    CAMINANDES["Id"]: "caminandes-poster.jpg",
    SEASON_ONE["Id"]: "caminandes-season-1.jpg",
    EPISODES[0]["Id"]: "caminandes-llama-drama.jpg",
    EPISODES[1]["Id"]: "caminandes-gran-dillama.jpg",
    EPISODES[2]["Id"]: "caminandes-llamigos.jpg",
    "person-ari": "fixture-user.jpg",
    "person-mina": "fixture-user.jpg",
    "person-jon": "fixture-user.jpg",
    "person-koro": "fixture-user.jpg",
    "person-pichin": "fixture-user.jpg",
    "person-oti": "fixture-user.jpg",
}
THUMB_ART = {
    BIG_BUCK_BUNNY["Id"]: "big-buck-bunny-backdrop.png",
    SINTEL["Id"]: "sintel-backdrop.png",
    TEARS_OF_STEEL["Id"]: "tears-of-steel-backdrop.jpg",
    CAMINANDES["Id"]: "caminandes-backdrop.png",
    EPISODES[0]["Id"]: "caminandes-llama-drama.jpg",
    EPISODES[1]["Id"]: "caminandes-gran-dillama.jpg",
    EPISODES[2]["Id"]: "caminandes-llamigos.jpg",
}
BACKDROP_ART = {
    BIG_BUCK_BUNNY["Id"]: "big-buck-bunny-backdrop.png",
    SINTEL["Id"]: "sintel-backdrop.png",
    TEARS_OF_STEEL["Id"]: "tears-of-steel-backdrop.jpg",
    CAMINANDES["Id"]: "caminandes-backdrop.png",
    SEASON_ONE["Id"]: "caminandes-backdrop.png",
}
VIDEO_FILE = MEDIA_ROOT / "big-buck-bunny-clip.mp4"


def list_payload(items: list[dict[str, object]]) -> dict[str, object]:
    return {"Items": deepcopy(items), "TotalRecordCount": len(items)}


def query_values(query: dict[str, list[str]], name: str) -> list[str]:
    return query.get(name, [])


def query_value(query: dict[str, list[str]], name: str) -> str:
    values = query_values(query, name)
    return values[0] if values else ""


def matching_search_items(term: str) -> list[dict[str, object]]:
    needle = term.casefold().strip()
    values = [*MOVIES, CAMINANDES, *EPISODES]
    if not needle:
        return values
    return [
        value
        for value in values
        if needle in str(value.get("Name", "")).casefold()
        or needle in str(value.get("SeriesName", "")).casefold()
    ]


class Handler(BaseHTTPRequestHandler):
    server_version = "sloppaTVScreenshotFixture/2.0"

    def log_message(self, fmt: str, *args: object) -> None:
        print(fmt % args, flush=True)

    def send_json(self, payload: object, status: int = 200) -> None:
        data = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def send_empty(self, status: int = 204) -> None:
        self.send_response(status)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def send_file(self, path: Path, *, content_type: str | None = None) -> None:
        if not path.is_file():
            self.send_error(404)
            return
        size = path.stat().st_size
        start = 0
        end = size - 1
        status = 200
        range_header = self.headers.get("Range", "")
        match = re.fullmatch(r"bytes=(\d*)-(\d*)", range_header.strip()) if range_header else None
        if match:
            if match.group(1):
                start = int(match.group(1))
            if match.group(2):
                end = min(int(match.group(2)), end)
            if start > end or start >= size:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            status = 206
        length = end - start + 1
        self.send_response(status)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Type", content_type or mimetypes.guess_type(path.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(length))
        if status == 206:
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.end_headers()
        if self.command == "HEAD":
            return
        with path.open("rb") as handle:
            handle.seek(start)
            remaining = length
            while remaining > 0:
                chunk = handle.read(min(64 * 1024, remaining))
                if not chunk:
                    break
                self.wfile.write(chunk)
                remaining -= len(chunk)

    def send_item_artwork(self, item_id: str, image_kind: str) -> None:
        if image_kind == "Primary":
            filename = PRIMARY_ART.get(item_id)
        elif image_kind == "Thumb":
            filename = THUMB_ART.get(item_id) or BACKDROP_ART.get(item_id)
        else:
            filename = BACKDROP_ART.get(item_id) or THUMB_ART.get(item_id)
        if not filename:
            self.send_error(404)
            return
        self.send_file(ARTWORK_ROOT / filename)

    def do_POST(self) -> None:  # noqa: N802
        split = urlsplit(self.path)
        path = split.path
        if path == "/Users/AuthenticateByName":
            self.send_json(
                {
                    "AccessToken": "fixture-token",
                    "User": {"Id": "fixture-user", "Name": "Viewer"},
                    "ServerId": "fixture-server",
                }
            )
            return
        playback = re.fullmatch(r"/Items/([^/]+)/PlaybackInfo", path)
        if playback:
            item_id = playback.group(1)
            self.send_json(
                {
                    "PlaySessionId": "fixture-play-session",
                    "MediaSources": [media_source(item_id)],
                }
            )
            return
        if path.startswith("/Sessions/Playing") or path.endswith("/Progress") or path.endswith("/Stopped"):
            self.send_empty()
            return
        self.send_json({})

    def do_HEAD(self) -> None:  # noqa: N802
        self.handle_read()

    def do_GET(self) -> None:  # noqa: N802
        self.handle_read()

    def handle_read(self) -> None:
        split = urlsplit(self.path)
        path = split.path
        query = parse_qs(split.query)

        if path == "/System/Info/Public":
            self.send_json(
                {
                    "Id": "fixture-server",
                    "ServerName": "sloppaTV Open Media Lab",
                    "Version": "10.10.0",
                    "ProductName": "Jellyfin",
                    "OperatingSystem": "Linux",
                }
            )
            return

        if path == "/Users/fixture-user/Images/Primary":
            self.send_file(ARTWORK_ROOT / "fixture-user.jpg")
            return

        stream = re.fullmatch(r"/Videos/([^/]+)/stream\.mp4", path)
        if stream:
            self.send_file(VIDEO_FILE, content_type="video/mp4")
            return

        image = re.fullmatch(r"/Items/([^/]+)/Images/(Primary|Thumb)", path)
        if image:
            self.send_item_artwork(image.group(1), image.group(2))
            return
        backdrop = re.fullmatch(r"/Items/([^/]+)/Images/Backdrop/0", path)
        if backdrop:
            self.send_item_artwork(backdrop.group(1), "Backdrop")
            return

        if path.endswith("/Views"):
            self.send_json(list_payload(VIEWS))
            return

        if path.endswith("/Items/Resume"):
            bunny_resume = deepcopy(BIG_BUCK_BUNNY)
            bunny_resume["UserData"]["PlaybackPositionTicks"] = ticks(238)
            self.send_json(list_payload([bunny_resume, EPISODES[1]]))
            return

        if path == "/Shows/NextUp":
            series_id = query_value(query, "SeriesId")
            next_items = [EPISODES[1]] if series_id == CAMINANDES["Id"] else [EPISODES[1], EPISODES[2]]
            self.send_json(list_payload(next_items))
            return

        seasons = re.fullmatch(r"/Shows/([^/]+)/Seasons", path)
        if seasons:
            values = [SEASON_ONE] if seasons.group(1) == CAMINANDES["Id"] else []
            self.send_json(list_payload(values))
            return

        episodes = re.fullmatch(r"/Shows/([^/]+)/Episodes", path)
        if episodes:
            values = EPISODES if episodes.group(1) == CAMINANDES["Id"] else []
            self.send_json(list_payload(values))
            return

        if path.endswith("/Items/Latest"):
            parent_id = query_value(query, "ParentId")
            values = MOVIES if parent_id == "movies" else [CAMINANDES] if parent_id == "shows" else [*MOVIES, CAMINANDES]
            self.send_json(deepcopy(values))
            return

        similar = re.fullmatch(r"/Items/([^/]+)/Similar", path)
        if similar:
            item_id = similar.group(1)
            if item_id == CAMINANDES["Id"] or item_id.startswith("episode-"):
                values = MOVIES
            else:
                values = [value for value in [SINTEL, TEARS_OF_STEEL, CAMINANDES, BIG_BUCK_BUNNY] if value["Id"] != item_id]
            self.send_json(list_payload(values))
            return

        user_item = re.fullmatch(r"/Users/[^/]+/Items/([^/]+)", path)
        if user_item:
            selected = ALL_ITEMS.get(user_item.group(1))
            if selected is None:
                self.send_error(404)
            else:
                self.send_json(deepcopy(selected))
            return

        if path.endswith("/Items") or path == "/Items":
            term = query_value(query, "SearchTerm")
            if term:
                values = matching_search_items(term)
            else:
                parent_id = query_value(query, "ParentId")
                person_id = query_value(query, "PersonIds")
                filters = query_value(query, "Filters")
                include_types = query_value(query, "IncludeItemTypes")
                if parent_id == "movies":
                    values = MOVIES
                elif parent_id == "shows":
                    values = [CAMINANDES]
                elif person_id:
                    values = [BIG_BUCK_BUNNY, CAMINANDES, EPISODES[1]]
                elif filters == "IsFavorite":
                    values = [BIG_BUCK_BUNNY, CAMINANDES]
                elif include_types == "Movie,Series":
                    values = [BIG_BUCK_BUNNY, CAMINANDES, SINTEL, TEARS_OF_STEEL]
                else:
                    values = [*MOVIES, CAMINANDES, *EPISODES]
            self.send_json(list_payload(values))
            return

        self.send_json({"Items": [], "TotalRecordCount": 0})


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18096)
    args = parser.parse_args()
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
