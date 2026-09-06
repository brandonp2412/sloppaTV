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
ELEPHANTS_DREAM = movie(
    "movie-elephants-dream",
    "Elephants Dream",
    year=2006,
    runtime_seconds=658,
    overview="Two travellers move through a vast mechanical world whose strange machines and shifting spaces blur the line between guidance, control and imagination.",
    rating=7.6,
    genres=["Animation", "Science Fiction", "Fantasy"],
    official_rating="PG",
)
SPRING = movie(
    "movie-spring",
    "Spring",
    year=2019,
    runtime_seconds=464,
    overview="A shepherd girl and her dog climb into the mountains to confront ancient spirits and restore the seasonal cycle in a quiet fantasy landscape.",
    rating=8.5,
    genres=["Animation", "Fantasy", "Family"],
    official_rating="PG",
    position_seconds=132,
)
COFFEE_RUN = movie(
    "movie-coffee-run",
    "Coffee Run",
    year=2020,
    runtime_seconds=185,
    overview="A caffeine-fuelled sprint through memories of a relationship turns an ordinary coffee run into a fast-moving tour of joy, loss and recovery.",
    rating=8.0,
    genres=["Animation", "Comedy", "Drama"],
    official_rating="PG",
)
SPRITE_FRIGHT = movie(
    "movie-sprite-fright",
    "Sprite Fright",
    year=2021,
    runtime_seconds=630,
    overview="A group of noisy teenagers enters an isolated forest and discovers that its tiny mushroom inhabitants are considerably less harmless than they look.",
    rating=8.4,
    genres=["Animation", "Comedy", "Horror"],
    official_rating="PG-13",
    favorite=True,
)
GLASS_HALF = movie(
    "movie-glass-half",
    "Glass Half",
    year=2015,
    runtime_seconds=193,
    overview="Two art critics with very different temperaments inspect a gallery and turn a simple conversation about taste into an escalating comic argument.",
    rating=7.9,
    genres=["Animation", "Comedy", "Short"],
    official_rating="G",
)
DAILY_DWEEBS = movie(
    "movie-daily-dweebs",
    "The Daily Dweebs",
    year=2017,
    runtime_seconds=60,
    overview="An enthusiastic dog barrels through an emotional rollercoaster of everyday mishaps in a compact character-animation showcase.",
    rating=8.2,
    genres=["Animation", "Comedy", "Family"],
    official_rating="G",
)
MOVIES = [
    BIG_BUCK_BUNNY,
    SINTEL,
    TEARS_OF_STEEL,
    ELEPHANTS_DREAM,
    SPRING,
    COFFEE_RUN,
    SPRITE_FRIGHT,
    GLASS_HALF,
    DAILY_DWEEBS,
]

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


def anthology_series(item_id: str, name: str, overview: str, year: int, *, favorite: bool = False) -> dict[str, object]:
    return {
        "Id": item_id,
        "Name": name,
        "Type": "Series",
        "Overview": overview,
        "ProductionYear": year,
        "OfficialRating": "PG",
        "CommunityRating": 8.4,
        "Genres": ["Animation", "Short", "Open Movie"],
        "ImageTags": image_tags(item_id),
        "BackdropImageTags": [f"{item_id}-backdrop"],
        "UserData": user_data(favorite=favorite),
    }


def anthology_season(series: dict[str, object]) -> dict[str, object]:
    item_id = f"season-{series['Id']}-1"
    return {
        "Id": item_id,
        "Name": "Season 1",
        "Type": "Season",
        "SeriesId": series["Id"],
        "SeriesName": series["Name"],
        "IndexNumber": 1,
        "ProductionYear": series["ProductionYear"],
        "ImageTags": image_tags(item_id, thumb=False),
        "BackdropImageTags": [f"{series['Id']}-backdrop"],
        "UserData": user_data(),
    }


def anthology_episode(series: dict[str, object], season: dict[str, object], source: dict[str, object], index: int) -> dict[str, object]:
    item_id = f"episode-{series['Id']}-{index}"
    return {
        "Id": item_id,
        "Name": source["Name"],
        "Type": "Episode",
        "SeriesId": series["Id"],
        "SeriesName": series["Name"],
        "SeriesPrimaryImageTag": f"{series['Id']}-primary",
        "SeasonName": season["Name"],
        "IndexNumber": index,
        "ParentIndexNumber": 1,
        "Overview": source["Overview"],
        "ProductionYear": source["ProductionYear"],
        "OfficialRating": source["OfficialRating"],
        "CommunityRating": source["CommunityRating"],
        "Genres": source["Genres"],
        "RunTimeTicks": source["RunTimeTicks"],
        "Container": "mp4",
        "ImageTags": image_tags(item_id),
        "ParentBackdropImageTags": [f"{series['Id']}-backdrop"],
        "ParentBackdropItemId": series["Id"],
        "UserData": user_data(played=index == 1, position_seconds=48 if index == 2 else 0),
        "MediaSources": [media_source(item_id)],
    }


OPEN_CLASSICS = anthology_series(
    "series-open-classics",
    "Open Movie Classics",
    "A collection of landmark Blender Foundation open movies, from surreal mechanical worlds to giant rabbits and fantasy quests.",
    2006,
    favorite=True,
)
OPEN_WORLDS = anthology_series(
    "series-open-worlds",
    "Open Worlds",
    "Creative Commons shorts built around distinctive worlds: science fiction cities, mountain spirits and deeply unfriendly forests.",
    2012,
)
BLENDER_SHORTS = anthology_series(
    "series-blender-shorts",
    "Blender Shorts",
    "Short, self-contained Blender Studio stories focused on expressive characters, visual comedy and compact animation experiments.",
    2015,
)
MODERN_OPEN_MOVIES = anthology_series(
    "series-modern-open-movies",
    "Modern Open Movies",
    "A newer selection of openly licensed Blender films spanning stylized horror, warm character comedy and cinematic fantasy.",
    2019,
)

OPEN_CLASSICS_SEASON = anthology_season(OPEN_CLASSICS)
OPEN_WORLDS_SEASON = anthology_season(OPEN_WORLDS)
BLENDER_SHORTS_SEASON = anthology_season(BLENDER_SHORTS)
MODERN_OPEN_MOVIES_SEASON = anthology_season(MODERN_OPEN_MOVIES)

OPEN_CLASSICS_EPISODES = [
    anthology_episode(OPEN_CLASSICS, OPEN_CLASSICS_SEASON, source, index)
    for index, source in enumerate(MOVIES, start=1)
]
OPEN_WORLDS_EPISODES = [
    anthology_episode(OPEN_WORLDS, OPEN_WORLDS_SEASON, TEARS_OF_STEEL, 1),
    anthology_episode(OPEN_WORLDS, OPEN_WORLDS_SEASON, SPRING, 2),
    anthology_episode(OPEN_WORLDS, OPEN_WORLDS_SEASON, SPRITE_FRIGHT, 3),
]
BLENDER_SHORTS_EPISODES = [
    anthology_episode(BLENDER_SHORTS, BLENDER_SHORTS_SEASON, GLASS_HALF, 1),
    anthology_episode(BLENDER_SHORTS, BLENDER_SHORTS_SEASON, COFFEE_RUN, 2),
    anthology_episode(BLENDER_SHORTS, BLENDER_SHORTS_SEASON, DAILY_DWEEBS, 3),
]
MODERN_OPEN_MOVIES_EPISODES = [
    anthology_episode(MODERN_OPEN_MOVIES, MODERN_OPEN_MOVIES_SEASON, SPRING, 1),
    anthology_episode(MODERN_OPEN_MOVIES, MODERN_OPEN_MOVIES_SEASON, COFFEE_RUN, 2),
    anthology_episode(MODERN_OPEN_MOVIES, MODERN_OPEN_MOVIES_SEASON, SPRITE_FRIGHT, 3),
]

SERIES = [OPEN_CLASSICS, CAMINANDES, OPEN_WORLDS, BLENDER_SHORTS, MODERN_OPEN_MOVIES]
SEASONS = [SEASON_ONE, OPEN_CLASSICS_SEASON, OPEN_WORLDS_SEASON, BLENDER_SHORTS_SEASON, MODERN_OPEN_MOVIES_SEASON]
ALL_EPISODES = [*EPISODES, *OPEN_CLASSICS_EPISODES, *OPEN_WORLDS_EPISODES, *BLENDER_SHORTS_EPISODES, *MODERN_OPEN_MOVIES_EPISODES]
SEASONS_BY_SERIES = {
    CAMINANDES["Id"]: [SEASON_ONE],
    OPEN_CLASSICS["Id"]: [OPEN_CLASSICS_SEASON],
    OPEN_WORLDS["Id"]: [OPEN_WORLDS_SEASON],
    BLENDER_SHORTS["Id"]: [BLENDER_SHORTS_SEASON],
    MODERN_OPEN_MOVIES["Id"]: [MODERN_OPEN_MOVIES_SEASON],
}
EPISODES_BY_SERIES = {
    CAMINANDES["Id"]: EPISODES,
    OPEN_CLASSICS["Id"]: OPEN_CLASSICS_EPISODES,
    OPEN_WORLDS["Id"]: OPEN_WORLDS_EPISODES,
    BLENDER_SHORTS["Id"]: BLENDER_SHORTS_EPISODES,
    MODERN_OPEN_MOVIES["Id"]: MODERN_OPEN_MOVIES_EPISODES,
}

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

ALL_ITEMS = {str(value["Id"]): value for value in [*MOVIES, *SERIES, *SEASONS, *ALL_EPISODES, *VIEWS]}

PRIMARY_ART = {
    "movies": "movies-library.jpg",
    "shows": "shows-library.jpg",
    BIG_BUCK_BUNNY["Id"]: "big-buck-bunny-poster.jpg",
    SINTEL["Id"]: "sintel-poster.jpg",
    TEARS_OF_STEEL["Id"]: "tears-of-steel-poster.png",
    ELEPHANTS_DREAM["Id"]: "elephants-dream-poster.jpg",
    SPRING["Id"]: "spring-poster.jpg",
    COFFEE_RUN["Id"]: "coffee-run-poster.png",
    SPRITE_FRIGHT["Id"]: "sprite-fright-poster.jpg",
    GLASS_HALF["Id"]: "glass-half-poster.jpg",
    DAILY_DWEEBS["Id"]: "daily-dweebs-poster.png",
    CAMINANDES["Id"]: "caminandes-poster.jpg",
    SEASON_ONE["Id"]: "caminandes-season-1.jpg",
    EPISODES[0]["Id"]: "caminandes-llama-drama.jpg",
    EPISODES[1]["Id"]: "caminandes-gran-dillama.jpg",
    EPISODES[2]["Id"]: "caminandes-llamigos.jpg",
    OPEN_CLASSICS["Id"]: "elephants-dream-poster.jpg",
    OPEN_WORLDS["Id"]: "spring-poster.jpg",
    BLENDER_SHORTS["Id"]: "daily-dweebs-poster.png",
    MODERN_OPEN_MOVIES["Id"]: "sprite-fright-poster.jpg",
    OPEN_CLASSICS_SEASON["Id"]: "elephants-dream-poster.jpg",
    OPEN_WORLDS_SEASON["Id"]: "spring-poster.jpg",
    BLENDER_SHORTS_SEASON["Id"]: "daily-dweebs-poster.png",
    MODERN_OPEN_MOVIES_SEASON["Id"]: "sprite-fright-poster.jpg",
    OPEN_CLASSICS_EPISODES[0]["Id"]: "elephants-dream-poster.jpg",
    OPEN_CLASSICS_EPISODES[1]["Id"]: "big-buck-bunny-poster.jpg",
    OPEN_CLASSICS_EPISODES[2]["Id"]: "sintel-poster.jpg",
    OPEN_WORLDS_EPISODES[0]["Id"]: "tears-of-steel-poster.png",
    OPEN_WORLDS_EPISODES[1]["Id"]: "spring-poster.jpg",
    OPEN_WORLDS_EPISODES[2]["Id"]: "sprite-fright-poster.jpg",
    BLENDER_SHORTS_EPISODES[0]["Id"]: "glass-half-poster.jpg",
    BLENDER_SHORTS_EPISODES[1]["Id"]: "coffee-run-poster.png",
    BLENDER_SHORTS_EPISODES[2]["Id"]: "daily-dweebs-poster.png",
    MODERN_OPEN_MOVIES_EPISODES[0]["Id"]: "spring-poster.jpg",
    MODERN_OPEN_MOVIES_EPISODES[1]["Id"]: "coffee-run-poster.png",
    MODERN_OPEN_MOVIES_EPISODES[2]["Id"]: "sprite-fright-poster.jpg",
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
    ELEPHANTS_DREAM["Id"]: "elephants-dream-backdrop.jpg",
    SPRING["Id"]: "spring-backdrop.jpg",
    COFFEE_RUN["Id"]: "coffee-run-backdrop.png",
    SPRITE_FRIGHT["Id"]: "sprite-fright-backdrop.jpg",
    GLASS_HALF["Id"]: "glass-half-backdrop.png",
    DAILY_DWEEBS["Id"]: "daily-dweebs-backdrop.jpg",
    CAMINANDES["Id"]: "caminandes-backdrop.png",
    EPISODES[0]["Id"]: "caminandes-llama-drama.jpg",
    EPISODES[1]["Id"]: "caminandes-gran-dillama.jpg",
    EPISODES[2]["Id"]: "caminandes-llamigos.jpg",
    OPEN_CLASSICS["Id"]: "elephants-dream-backdrop.jpg",
    OPEN_WORLDS["Id"]: "spring-backdrop.jpg",
    BLENDER_SHORTS["Id"]: "glass-half-backdrop.png",
    MODERN_OPEN_MOVIES["Id"]: "sprite-fright-backdrop.jpg",
    OPEN_CLASSICS_EPISODES[0]["Id"]: "elephants-dream-backdrop.jpg",
    OPEN_CLASSICS_EPISODES[1]["Id"]: "big-buck-bunny-backdrop.png",
    OPEN_CLASSICS_EPISODES[2]["Id"]: "sintel-backdrop.png",
    OPEN_WORLDS_EPISODES[0]["Id"]: "tears-of-steel-backdrop.jpg",
    OPEN_WORLDS_EPISODES[1]["Id"]: "spring-backdrop.jpg",
    OPEN_WORLDS_EPISODES[2]["Id"]: "sprite-fright-backdrop.jpg",
    BLENDER_SHORTS_EPISODES[0]["Id"]: "glass-half-backdrop.png",
    BLENDER_SHORTS_EPISODES[1]["Id"]: "coffee-run-backdrop.png",
    BLENDER_SHORTS_EPISODES[2]["Id"]: "daily-dweebs-backdrop.jpg",
    MODERN_OPEN_MOVIES_EPISODES[0]["Id"]: "spring-backdrop.jpg",
    MODERN_OPEN_MOVIES_EPISODES[1]["Id"]: "coffee-run-backdrop.png",
    MODERN_OPEN_MOVIES_EPISODES[2]["Id"]: "sprite-fright-backdrop.jpg",
}
BACKDROP_ART = {
    BIG_BUCK_BUNNY["Id"]: "big-buck-bunny-backdrop.png",
    SINTEL["Id"]: "sintel-backdrop.png",
    TEARS_OF_STEEL["Id"]: "tears-of-steel-backdrop.jpg",
    ELEPHANTS_DREAM["Id"]: "elephants-dream-backdrop.jpg",
    SPRING["Id"]: "spring-backdrop.jpg",
    COFFEE_RUN["Id"]: "coffee-run-backdrop.png",
    SPRITE_FRIGHT["Id"]: "sprite-fright-backdrop.jpg",
    GLASS_HALF["Id"]: "glass-half-backdrop.png",
    DAILY_DWEEBS["Id"]: "daily-dweebs-backdrop.jpg",
    CAMINANDES["Id"]: "caminandes-backdrop.png",
    SEASON_ONE["Id"]: "caminandes-backdrop.png",
    OPEN_CLASSICS["Id"]: "elephants-dream-backdrop.jpg",
    OPEN_WORLDS["Id"]: "spring-backdrop.jpg",
    BLENDER_SHORTS["Id"]: "glass-half-backdrop.png",
    MODERN_OPEN_MOVIES["Id"]: "sprite-fright-backdrop.jpg",
    OPEN_CLASSICS_SEASON["Id"]: "elephants-dream-backdrop.jpg",
    OPEN_WORLDS_SEASON["Id"]: "spring-backdrop.jpg",
    BLENDER_SHORTS_SEASON["Id"]: "glass-half-backdrop.png",
    MODERN_OPEN_MOVIES_SEASON["Id"]: "sprite-fright-backdrop.jpg",
}
for episode_item, source_movie in zip(OPEN_CLASSICS_EPISODES, MOVIES, strict=True):
    PRIMARY_ART[episode_item["Id"]] = PRIMARY_ART[source_movie["Id"]]
    THUMB_ART[episode_item["Id"]] = THUMB_ART[source_movie["Id"]]
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
    values = [*MOVIES, *SERIES, *ALL_EPISODES]
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
            spring_resume = deepcopy(SPRING)
            spring_resume["UserData"]["PlaybackPositionTicks"] = ticks(132)
            coffee_resume = deepcopy(COFFEE_RUN)
            coffee_resume["UserData"]["PlaybackPositionTicks"] = ticks(54)
            sprite_resume = deepcopy(SPRITE_FRIGHT)
            sprite_resume["UserData"]["PlaybackPositionTicks"] = ticks(276)
            self.send_json(list_payload([bunny_resume, EPISODES[1], spring_resume, coffee_resume, sprite_resume, BLENDER_SHORTS_EPISODES[1]]))
            return

        if path == "/Shows/NextUp":
            series_id = query_value(query, "SeriesId")
            if series_id:
                series_episodes = EPISODES_BY_SERIES.get(series_id, [])
                next_items = series_episodes[1:2] or series_episodes[:1]
            else:
                next_items = [episodes[1] if len(episodes) > 1 else episodes[0] for episodes in EPISODES_BY_SERIES.values() if episodes]
            self.send_json(list_payload(next_items))
            return

        seasons = re.fullmatch(r"/Shows/([^/]+)/Seasons", path)
        if seasons:
            self.send_json(list_payload(SEASONS_BY_SERIES.get(seasons.group(1), [])))
            return

        episodes = re.fullmatch(r"/Shows/([^/]+)/Episodes", path)
        if episodes:
            self.send_json(list_payload(EPISODES_BY_SERIES.get(episodes.group(1), [])))
            return

        if path.endswith("/Items/Latest"):
            parent_id = query_value(query, "ParentId")
            values = MOVIES if parent_id == "movies" else SERIES if parent_id == "shows" else [*MOVIES, *SERIES]
            self.send_json(deepcopy(values))
            return

        similar = re.fullmatch(r"/Items/([^/]+)/Similar", path)
        if similar:
            item_id = similar.group(1)
            if item_id in {series["Id"] for series in SERIES} or item_id.startswith("episode-"):
                values = MOVIES[:6]
            else:
                values = [value for value in [SINTEL, TEARS_OF_STEEL, CAMINANDES, ELEPHANTS_DREAM, SPRING, SPRITE_FRIGHT, BIG_BUCK_BUNNY] if value["Id"] != item_id]
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
                    values = SERIES
                elif person_id:
                    values = [BIG_BUCK_BUNNY, CAMINANDES, EPISODES[1], SPRING, OPEN_WORLDS, OPEN_WORLDS_EPISODES[1]]
                elif filters == "IsFavorite":
                    values = [BIG_BUCK_BUNNY, CAMINANDES, SPRITE_FRIGHT, OPEN_CLASSICS]
                elif include_types == "Movie,Series":
                    values = [BIG_BUCK_BUNNY, CAMINANDES, SINTEL, OPEN_CLASSICS, TEARS_OF_STEEL, OPEN_WORLDS, SPRING, BLENDER_SHORTS]
                else:
                    values = [*MOVIES, *SERIES, *ALL_EPISODES]
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
