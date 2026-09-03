#!/usr/bin/env python3
"""Non-destructive integration checks against a real Jellyfin server.

Credentials are loaded from environment variables or the gitignored .env.local.
The script does not mutate user state, refresh metadata, mark items, or delete media.
PlaybackInfo negotiation is allowed because it is required to verify client/profile behavior.
"""

from __future__ import annotations

import argparse
import json
import os
import ssl
import sys
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEVICE_ID = "sloppatv-server-e2e"
CLIENT = "sloppaTV Server E2E"
VERSION = "0.1.0"


def load_local_env() -> None:
    path = ROOT / ".env.local"
    if not path.exists():
        return
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        os.environ.setdefault(key.strip(), value.strip())


def authorization(token: str = "", device_id: str = DEVICE_ID) -> str:
    value = (
        f'MediaBrowser Client="{CLIENT}",Version="{VERSION}",'
        f'DeviceId="{device_id}",Device="Glass"'
    )
    if token:
        value += f',Token="{token}"'
    return value


class Jellyfin:
    def __init__(self, server: str, insecure: bool = False, device_id: str = DEVICE_ID) -> None:
        self.server = server.rstrip("/")
        self.device_id = device_id
        self.token = ""
        self.user_id = ""
        self.username = ""
        self.context = ssl._create_unverified_context() if insecure else ssl.create_default_context()

    def request(self, method: str, path: str, body: Any | None = None) -> Any:
        headers = {
            "Accept": "application/json",
            "Content-Type": "application/json",
            "Authorization": authorization(self.token, self.device_id),
            "User-Agent": f"sloppaTV-server-e2e/{VERSION}",
        }
        if self.token:
            headers["X-Emby-Token"] = self.token
        payload = None if body is None else json.dumps(body).encode("utf-8")
        request = urllib.request.Request(self.server + path, data=payload, headers=headers, method=method)
        with urllib.request.urlopen(request, context=self.context, timeout=20) as response:
            content = response.read()
            if not content:
                return None
            return json.loads(content)

    def get(self, path: str) -> Any:
        return self.request("GET", path)

    def login(self, username: str, password: str) -> None:
        result = self.request(
            "POST",
            "/Users/AuthenticateByName",
            {"Username": username, "Pw": password},
        )
        self.token = result["AccessToken"]
        self.user_id = result["User"]["Id"]
        self.username = result["User"]["Name"]

    def logout(self) -> None:
        if not self.token:
            return
        try:
            self.request("POST", "/Sessions/Logout")
        finally:
            self.token = ""
            self.user_id = ""
            self.username = ""


def query(values: dict[str, Any]) -> str:
    return urllib.parse.urlencode(values, doseq=True)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def verify_bitrate_negotiation(client: Jellyfin, items: list[dict[str, Any]]) -> dict[str, Any] | None:
    candidate: tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]] | None = None
    for item in items:
        for source in item.get("MediaSources") or []:
            streams = source.get("MediaStreams") or []
            video = next((stream for stream in streams if stream.get("Type") == "Video"), None)
            audio = next((stream for stream in streams if stream.get("Type") == "Audio"), None)
            source_bitrate = int(source.get("Bitrate") or 0)
            if video is None or audio is None or source_bitrate <= 3_000_000:
                continue
            if (
                str(source.get("Container") or "").split(",", 1)[0].lower() in {"mkv", "mp4"}
                and str(video.get("Codec") or "").lower() in {"h264", "hevc"}
                and str(audio.get("Codec") or "").lower() in {"aac", "ac3", "eac3"}
            ):
                candidate = (item, source, video, audio)
                break
        if candidate is not None:
            break
    if candidate is None:
        return None

    item, source, video, audio = candidate
    video_codec = str(video.get("Codec") or "").lower()
    audio_codec = str(audio.get("Codec") or "").lower()
    high_cap = 120_000_000
    low_cap = min(2_000_000, max(1_000_000, int(source.get("Bitrate") or 0) // 4))

    def negotiate(max_bitrate: int) -> dict[str, Any]:
        profile = {
            "Name": "sloppaTV Bitrate E2E",
            "MaxStaticBitrate": high_cap,
            "MaxStreamingBitrate": max_bitrate,
            "DirectPlayProfiles": [
                {
                    "Container": "mkv,matroska,mp4,m4v,mov,ts,mpegts,webm",
                    "Type": "Video",
                    "VideoCodec": video_codec,
                    "AudioCodec": audio_codec,
                }
            ],
            "TranscodingProfiles": [
                {
                    "Container": "ts",
                    "Type": "Video",
                    "VideoCodec": "h264",
                    "AudioCodec": "aac,ac3,eac3,mp3",
                    "Protocol": "hls",
                    "Context": "Streaming",
                    "MaxAudioChannels": "8",
                }
            ],
            "CodecProfiles": [],
            "SubtitleProfiles": [],
        }
        params = {
            "UserId": client.user_id,
            "MediaSourceId": source["Id"],
            "SubtitleStreamIndex": -1,
            "IsPlayback": "true",
            "AutoOpenLiveStream": "true",
            "MaxStreamingBitrate": max_bitrate,
        }
        playback = client.request(
            "POST",
            f"/Items/{item['Id']}/PlaybackInfo?" + query(params),
            {
                "UserId": client.user_id,
                "MediaSourceId": source["Id"],
                "DeviceProfile": profile,
                "SubtitleStreamIndex": -1,
                "MaxAudioChannels": 8,
                "EnableDirectPlay": True,
                "EnableDirectStream": True,
                "EnableTranscoding": True,
                "AllowVideoStreamCopy": True,
                "AllowAudioStreamCopy": True,
                "AutoOpenLiveStream": True,
            },
        )
        return playback.get("MediaSources", [])[0]

    high = negotiate(high_cap)
    low = negotiate(low_cap)
    low_url = str(low.get("TranscodingUrl") or "")
    low_query = urllib.parse.parse_qs(urllib.parse.urlparse(low_url).query)
    low_reasons = [reason for value in low_query.get("TranscodeReasons", []) for reason in value.split(",") if reason]
    require(high.get("SupportsDirectPlay") is True, "High-bitrate probe did not preserve DirectPlay")
    require(not high.get("TranscodingUrl"), "High-bitrate probe unexpectedly returned a server stream")
    require(low.get("SupportsDirectPlay") is False, "Low-bitrate probe did not reject DirectPlay")
    require(bool(low_url), "Low-bitrate probe did not return a constrained server stream")
    require("ContainerBitrateExceedsLimit" in low_reasons, f"Low-bitrate probe reasons were unexpected: {low_reasons}")
    return {
        "item": item.get("Name"),
        "sourceBitrate": int(source.get("Bitrate") or 0),
        "highCap": high_cap,
        "highCapDirectPlay": True,
        "lowCap": low_cap,
        "lowCapDirectPlay": False,
        "lowCapTranscodeReasons": low_reasons,
    }


def verify_direct_stream_negotiation(client: Jellyfin, items: list[dict[str, Any]]) -> dict[str, Any] | None:
    hls_video_codecs = {"h264", "hevc", "vp9", "av1"}
    hls_audio_codecs = {"aac", "ac3", "eac3", "mp3"}
    candidate: tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]] | None = None
    for item in items:
        for source in item.get("MediaSources") or []:
            streams = source.get("MediaStreams") or []
            video = next((stream for stream in streams if stream.get("Type") == "Video"), None)
            audio = next((stream for stream in streams if stream.get("Type") == "Audio"), None)
            if video is None or audio is None:
                continue
            video_codec = str(video.get("Codec") or "").lower()
            audio_codec = str(audio.get("Codec") or "").lower()
            container = str(source.get("Container") or "").split(",", 1)[0].lower()
            if container == "mkv" and video_codec in hls_video_codecs and audio_codec in hls_audio_codecs:
                candidate = (item, source, video, audio)
                break
        if candidate is not None:
            break
    if candidate is None:
        return None

    item, source, video, audio = candidate
    video_codec = str(video.get("Codec") or "").lower()
    audio_codec = str(audio.get("Codec") or "").lower()
    profile = {
        "Name": "sloppaTV DirectStream E2E",
        "MaxStaticBitrate": 120_000_000,
        "MaxStreamingBitrate": 120_000_000,
        # Intentionally exclude MKV so Jellyfin must expose the remux/server-stream path.
        "DirectPlayProfiles": [
            {
                "Container": "mp4,m4v,mov",
                "Type": "Video",
                "VideoCodec": video_codec,
                "AudioCodec": audio_codec,
            }
        ],
        "TranscodingProfiles": [
            {
                "Container": "ts",
                "Type": "Video",
                "VideoCodec": video_codec,
                "AudioCodec": audio_codec,
                "Protocol": "hls",
                "Context": "Streaming",
                "MaxAudioChannels": "8",
            }
        ],
        "CodecProfiles": [],
        "SubtitleProfiles": [],
    }
    params = {
        "UserId": client.user_id,
        "MediaSourceId": source["Id"],
        "SubtitleStreamIndex": -1,
        "IsPlayback": "true",
        "AutoOpenLiveStream": "true",
        "MaxStreamingBitrate": 120_000_000,
    }
    playback = client.request(
        "POST",
        f"/Items/{item['Id']}/PlaybackInfo?" + query(params),
        {
            "UserId": client.user_id,
            "MediaSourceId": source["Id"],
            "DeviceProfile": profile,
            "SubtitleStreamIndex": -1,
            "MaxAudioChannels": 8,
            "EnableDirectPlay": True,
            "EnableDirectStream": True,
            "EnableTranscoding": True,
            "AllowVideoStreamCopy": True,
            "AllowAudioStreamCopy": True,
            "AutoOpenLiveStream": True,
        },
    )
    playback_source = playback.get("MediaSources", [])[0]
    transcoding_url = str(playback_source.get("TranscodingUrl") or "")
    require(not playback_source.get("SupportsDirectPlay"), "DirectStream probe unexpectedly remained DirectPlay")
    require(playback_source.get("SupportsTranscoding") is True, "DirectStream probe did not offer a server-stream path")
    require(bool(transcoding_url), "DirectStream probe returned no server-stream URL")
    transcode_query = urllib.parse.parse_qs(urllib.parse.urlparse(transcoding_url).query)
    reasons = [reason for value in transcode_query.get("TranscodeReasons", []) for reason in value.split(",") if reason]
    direct_stream_reasons = {
        "AudioCodecNotSupported",
        "AudioBitrateNotSupported",
        "AudioChannelsNotSupported",
        "AudioProfileNotSupported",
        "AudioSampleRateNotSupported",
        "SecondaryAudioNotSupported",
        "AudioBitDepthNotSupported",
        "AudioIsExternal",
        "ContainerNotSupported",
        "VideoCodecTagNotSupported",
    }
    require(bool(reasons), "DirectStream probe did not return TranscodeReasons")
    require(all(reason in direct_stream_reasons for reason in reasons), f"Probe required full transcoding: {reasons}")
    return {
        "item": item.get("Name"),
        "container": source.get("Container"),
        "videoCodec": video_codec,
        "audioCodec": audio_codec,
        "serverSupportsDirectStreamFlag": bool(playback_source.get("SupportsDirectStream")),
        "transcodeReasons": reasons,
        "semanticDirectStream": True,
    }


def authorize_quick_connect_code(
    server: str,
    insecure: bool,
    username: str,
    password: str,
    code: str,
) -> dict[str, Any]:
    authorizer = Jellyfin(server, insecure, f"{DEVICE_ID}-qc-ui-authorizer")
    try:
        authorizer.login(username, password)
        authorized = authorizer.request("POST", "/QuickConnect/Authorize?" + query({"code": code}))
        require(authorized is True, "Quick Connect authorization was rejected")
        return {"authorized": True, "code": code, "user": authorizer.username}
    finally:
        authorizer.logout()


def verify_quick_connect(server: str, insecure: bool, username: str, password: str) -> dict[str, Any]:
    authorizer = Jellyfin(server, insecure, f"{DEVICE_ID}-qc-authorizer")
    target = Jellyfin(server, insecure, f"{DEVICE_ID}-qc-target")
    try:
        authorizer.login(username, password)
        require(target.get("/QuickConnect/Enabled") is True, "Quick Connect is disabled on the server")
        initiated = target.request("POST", "/QuickConnect/Initiate")
        secret = str(initiated.get("Secret") or "")
        code = str(initiated.get("Code") or "")
        require(bool(secret and code), "Quick Connect initiate response was incomplete")

        initial = target.get("/QuickConnect/Connect?" + query({"secret": secret}))
        require(initial.get("Authenticated") is False, "Quick Connect request was unexpectedly pre-authorized")
        authorized = authorizer.request("POST", "/QuickConnect/Authorize?" + query({"code": code}))
        require(authorized is True, "Quick Connect authorization was rejected")
        connected = target.get("/QuickConnect/Connect?" + query({"secret": secret}))
        require(connected.get("Authenticated") is True, "Quick Connect poll did not observe authorization")

        authentication = target.request(
            "POST",
            "/Users/AuthenticateWithQuickConnect",
            {"Secret": secret},
        )
        target.token = str(authentication.get("AccessToken") or "")
        target.user_id = str((authentication.get("User") or {}).get("Id") or "")
        target.username = str((authentication.get("User") or {}).get("Name") or "")
        require(bool(target.token and target.user_id), "Quick Connect did not issue an authenticated session")
        require(target.username == authorizer.username, "Quick Connect authenticated the wrong user")
        return {
            "initiallyAuthorized": False,
            "authorized": True,
            "authenticatedUser": target.username,
            "tokenIssued": True,
        }
    finally:
        target.logout()
        authorizer.logout()


def main() -> int:
    load_local_env()
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", default=os.getenv("JELLYFIN_LOCAL_SERVER", ""))
    parser.add_argument("--username", default=os.getenv("JELLYFIN_LOCAL_USERNAME", ""))
    parser.add_argument("--password", default=os.getenv("JELLYFIN_LOCAL_PASSWORD", ""))
    parser.add_argument("--insecure", action="store_true", help="Disable TLS verification for a local test endpoint")
    parser.add_argument("--scan-limit", type=int, default=1000)
    parser.add_argument(
        "--quick-connect",
        action="store_true",
        help="Exercise the transient Quick Connect authorize/authenticate round-trip and log out both test sessions",
    )
    parser.add_argument(
        "--authorize-code",
        default="",
        help="Authorize an existing Quick Connect code for TV UI acceptance, then exit",
    )
    args = parser.parse_args()

    require(bool(args.server), "Set JELLYFIN_LOCAL_SERVER or pass --server")
    require(bool(args.username), "Set JELLYFIN_LOCAL_USERNAME or pass --username")

    if args.authorize_code:
        result = authorize_quick_connect_code(
            args.server,
            args.insecure,
            args.username,
            args.password,
            args.authorize_code,
        )
        print(json.dumps(result, indent=2))
        return 0

    client = Jellyfin(args.server, args.insecure)
    public = client.get("/System/Info/Public")
    require(bool(public.get("Version")), "Server did not return a version")
    client.login(args.username, args.password)

    views = client.get(f"/Users/{client.user_id}/Views").get("Items", [])
    video_views = [view for view in views if view.get("CollectionType") in {"movies", "tvshows", "mixed", "boxsets"}]
    require(video_views, "No scoped video libraries were returned")

    resume = client.get(
        f"/Users/{client.user_id}/Items/Resume?"
        + query({"Limit": 30, "MediaTypes": "Video", "ExcludeItemTypes": "AudioBook"})
    ).get("Items", [])
    next_up = client.get(
        "/Shows/NextUp?"
        + query({"UserId": client.user_id, "Limit": 30, "EnableResumable": "false"})
    ).get("Items", [])

    search = client.get(
        f"/Users/{client.user_id}/Items?"
        + query(
            {
                "Recursive": "true",
                "SearchTerm": "Friends",
                "IncludeItemTypes": "Movie,Series,Episode",
                "Limit": 20,
            }
        )
    ).get("Items", [])
    require(search, "Real-server search returned no results for the acceptance query 'Friends'")

    browsed: dict[str, int] = {}
    for view in video_views:
        collection_type = view.get("CollectionType")
        if collection_type == "boxsets":
            continue
        items = client.get(
            f"/Users/{client.user_id}/Items?"
            + query(
                {
                    "ParentId": view["Id"],
                    "Recursive": "true",
                    "IncludeItemTypes": "Movie,Series,Episode,Folder,BoxSet",
                    "Limit": 60,
                }
            )
        ).get("Items", [])
        require(items, f"Library {view.get('Name', view['Id'])!r} returned no browse items")
        name = view.get("Name", view["Id"])
        browsed[name] = len(items)

    movie_view = next((view for view in video_views if view.get("CollectionType") == "movies"), None)
    show_view = next((view for view in video_views if view.get("CollectionType") == "tvshows"), None)
    collections = client.get(
        f"/Users/{client.user_id}/Items?"
        + query(
            {
                "Recursive": "true",
                "IncludeItemTypes": "BoxSet",
                "SortBy": "SortName",
                "SortOrder": "Ascending",
                "Limit": 60,
            }
        )
    ).get("Items", [])
    require(collections, "Collections browse returned no box sets")

    genre_count = 0
    letter_a_count = 0
    favorite_movie_count = 0
    if movie_view is not None:
        genres = client.get(
            "/Genres?"
            + query(
                {
                    "UserId": client.user_id,
                    "ParentId": movie_view["Id"],
                    "Recursive": "true",
                    "IncludeItemTypes": "Movie",
                    "SortBy": "SortName",
                    "SortOrder": "Ascending",
                    "Limit": 100,
                }
            )
        ).get("Items", [])
        require(genres, "Movie genre browse returned no genres")
        genre_count = len(genres)
        letter_a = client.get(
            f"/Users/{client.user_id}/Items?"
            + query(
                {
                    "ParentId": movie_view["Id"],
                    "Recursive": "true",
                    "IncludeItemTypes": "Movie",
                    "NameStartsWith": "A",
                    "Limit": 60,
                }
            )
        ).get("Items", [])
        require(letter_a, "Movie A-Z browse returned no titles starting with A")
        letter_a_count = len(letter_a)
        favorites = client.get(
            f"/Users/{client.user_id}/Items?"
            + query(
                {
                    "ParentId": movie_view["Id"],
                    "Recursive": "true",
                    "IncludeItemTypes": "Movie",
                    "Filters": "IsFavorite",
                    "Limit": 60,
                }
            )
        ).get("Items", [])
        favorite_movie_count = len(favorites)

    series_hierarchy: dict[str, Any] | None = None
    if show_view is not None:
        series_items = client.get(
            f"/Users/{client.user_id}/Items?"
            + query(
                {
                    "ParentId": show_view["Id"],
                    "Recursive": "true",
                    "IncludeItemTypes": "Series",
                    "SortBy": "SortName",
                    "SortOrder": "Ascending",
                    "Limit": 60,
                }
            )
        ).get("Items", [])
        series = series_items[0] if series_items else None
        require(series is not None, "Shows library filtered browse returned no series")
        seasons = client.get(
            f"/Shows/{series['Id']}/Seasons?"
            + query({"UserId": client.user_id, "EnableTotalRecordCount": "false"})
        ).get("Items", [])
        require(seasons, f"Series {series.get('Name')!r} returned no seasons")
        season = seasons[0]
        episodes = client.get(
            f"/Shows/{series['Id']}/Episodes?"
            + query(
                {
                    "UserId": client.user_id,
                    "SeasonId": season["Id"],
                    "EnableTotalRecordCount": "false",
                }
            )
        ).get("Items", [])
        require(episodes, f"Season {season.get('Name')!r} returned no episodes")
        series_hierarchy = {
            "series": series.get("Name"),
            "seasonCount": len(seasons),
            "sampleSeason": season.get("Name"),
            "sampleSeasonEpisodeCount": len(episodes),
        }

    scan = client.get(
        "/Items?"
        + query(
            {
                "UserId": client.user_id,
                "Recursive": "true",
                "IncludeItemTypes": "Movie,Series,Episode",
                "Fields": "MediaSources,MediaStreams,Trickplay",
                "EnableImages": "true",
                "EnableImageTypes": "Primary,Backdrop,Logo,Thumb",
                "ImageTypeLimit": 3,
                "Limit": max(1, args.scan_limit),
            }
        )
    ).get("Items", [])
    require(scan, "Library scan returned no media items")

    own_logos = 0
    parent_logos = 0
    own_backdrops = 0
    parent_backdrops = 0
    trickplay = 0
    hdr: list[dict[str, Any]] = []
    ass_candidate: tuple[dict[str, Any], dict[str, Any], dict[str, Any]] | None = None
    for item in scan:
        image_tags = item.get("ImageTags") or {}
        own_logos += bool(image_tags.get("Logo"))
        parent_logos += bool(item.get("ParentLogoImageTag"))
        own_backdrops += bool(item.get("BackdropImageTags"))
        parent_backdrops += bool(item.get("ParentBackdropImageTags"))
        trickplay += bool(item.get("Trickplay"))
        for source in item.get("MediaSources") or []:
            for stream in source.get("MediaStreams") or []:
                if (
                    ass_candidate is None
                    and stream.get("Type") == "Subtitle"
                    and str(stream.get("Codec", "")).lower() in {"ass", "ssa"}
                ):
                    ass_candidate = (item, source, stream)
                video_range = stream.get("VideoRangeType")
                if stream.get("Type") == "Video" and video_range not in (None, "", "SDR"):
                    hdr.append(
                        {
                            "name": item.get("Name"),
                            "codec": stream.get("Codec"),
                            "range": video_range,
                            "bitDepth": stream.get("BitDepth"),
                            "profile": stream.get("Profile"),
                            "level": stream.get("Level"),
                        }
                    )
                    break

    ass_probe: dict[str, Any] | None = None
    if ass_candidate is not None:
        item, source, subtitle = ass_candidate
        codec = str(subtitle.get("Codec", "")).lower()
        profile = {
            "Name": "sloppaTV Server E2E",
            "MaxStaticBitrate": 120_000_000,
            "MaxStreamingBitrate": 120_000_000,
            "DirectPlayProfiles": [
                {
                    "Container": "mkv,matroska,mp4,m4v,mov,ts,mpegts,webm",
                    "Type": "Video",
                    "VideoCodec": "h264,hevc,vp8,vp9,av1,mpeg2video",
                    "AudioCodec": "aac,mp3,ac3,eac3,flac,opus,vorbis",
                }
            ],
            "TranscodingProfiles": [
                {
                    "Container": "ts",
                    "Type": "Video",
                    "VideoCodec": "h264",
                    "AudioCodec": "aac,ac3,eac3,mp3",
                    "Protocol": "hls",
                    "Context": "Streaming",
                    "MaxAudioChannels": "8",
                }
            ],
            "CodecProfiles": [],
            "SubtitleProfiles": [
                {"Format": "srt", "Method": "External"},
                {"Format": "subrip", "Method": "External"},
                {"Format": "vtt", "Method": "External"},
                {"Format": "webvtt", "Method": "External"},
                {"Format": "ass", "Method": "External"},
                {"Format": "ssa", "Method": "External"},
            ],
        }
        subtitle_index = int(subtitle["Index"])
        playback = client.request(
            "POST",
            f"/Items/{item['Id']}/PlaybackInfo?"
            + query(
                {
                    "UserId": client.user_id,
                    "MediaSourceId": source["Id"],
                    "SubtitleStreamIndex": subtitle_index,
                    "IsPlayback": "true",
                    "AutoOpenLiveStream": "true",
                    "MaxStreamingBitrate": 120_000_000,
                }
            ),
            {
                "UserId": client.user_id,
                "MediaSourceId": source["Id"],
                "DeviceProfile": profile,
                "SubtitleStreamIndex": subtitle_index,
                "MaxAudioChannels": 8,
                "EnableDirectPlay": True,
                "EnableDirectStream": True,
                "EnableTranscoding": True,
                "AllowVideoStreamCopy": True,
                "AllowAudioStreamCopy": True,
                "AutoOpenLiveStream": True,
            },
        )
        playback_source = playback.get("MediaSources", [])[0]
        selected_stream = next(
            (
                stream
                for stream in playback_source.get("MediaStreams") or []
                if stream.get("Index") == subtitle_index
            ),
            {},
        )
        delivery_url = str(selected_stream.get("DeliveryUrl") or "")
        require(playback_source.get("SupportsDirectPlay") is True, "ASS/SSA probe was not offered DirectPlay")
        require(selected_stream.get("DeliveryMethod") == "External", "ASS/SSA probe did not use External delivery")
        require(bool(delivery_url), "ASS/SSA probe did not return a DeliveryUrl")
        ass_probe = {
            "item": item.get("Name"),
            "codec": codec,
            "supportsDirectPlay": True,
            "deliveryMethod": selected_stream.get("DeliveryMethod"),
            # Never print DeliveryUrl itself: it can contain an API key.
            "hasDeliveryUrl": True,
        }

    bitrate_probe = verify_bitrate_negotiation(client, scan)
    direct_stream_probe = verify_direct_stream_negotiation(client, scan)
    quick_connect = (
        verify_quick_connect(args.server, args.insecure, args.username, args.password)
        if args.quick_connect
        else None
    )

    result = {
        "server": {
            "name": public.get("ServerName"),
            "version": public.get("Version"),
        },
        "user": client.username,
        "views": [{"name": view.get("Name"), "type": view.get("CollectionType")} for view in video_views],
        "browseCounts": browsed,
        "resumeCount": len(resume),
        "nextUpCount": len(next_up),
        "friendsSearchCount": len(search),
        "collectionsCount": len(collections),
        "movieGenreCount": genre_count,
        "movieLetterACount": letter_a_count,
        "favoriteMovieCount": favorite_movie_count,
        "seriesHierarchy": series_hierarchy,
        "sampledItems": len(scan),
        "artwork": {
            "ownLogo": own_logos,
            "parentLogo": parent_logos,
            "ownBackdrop": own_backdrops,
            "parentBackdrop": parent_backdrops,
        },
        "trickplayItems": trickplay,
        "hdrItems": len(hdr),
        "hdrExamples": hdr[:5],
        "assSubtitleProbe": ass_probe,
        "bitrateProbe": bitrate_probe,
        "directStreamProbe": direct_stream_probe,
        "quickConnectProbe": quick_connect,
    }
    client.logout()
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, urllib.error.URLError) as error:
        print(f"server-e2e: {error}", file=sys.stderr)
        raise SystemExit(1)
