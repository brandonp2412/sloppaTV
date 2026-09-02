package nz.presley.sloppatv;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;

import androidx.media3.common.AudioAttributes;
import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.common.TrackSelectionOverride;
import androidx.media3.common.Tracks;
import androidx.media3.common.VideoSize;
import androidx.media3.common.text.CueGroup;
import androidx.media3.datasource.DefaultDataSource;
import androidx.media3.datasource.HttpDataSource;
import androidx.media3.exoplayer.DefaultLoadControl;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory;
import androidx.media3.ui.SubtitleView;

import java.util.Collections;

import io.github.peerless2012.ass.media.AssHandler;
import io.github.peerless2012.ass.media.AssHandlerConfig;
import io.github.peerless2012.ass.media.factory.AssRenderersFactory;
import io.github.peerless2012.ass.media.parser.AssSubtitleParserFactory;
import io.github.peerless2012.ass.media.type.AssRenderType;
import io.github.peerless2012.ass.media.widget.AssSubtitleView;

/**
 * Minimal Media3 bridge. The C++ application owns policy/state; this class exists only
 * because ExoPlayer and its buffer/audio output primitives are Java Android APIs.
 */
@SuppressWarnings("UnsafeOptInUsageError")
public final class SloppaPlayerBridge {
    private static final String TAG = "sloppaTV/player";

    public static final int STATE_IDLE = 0;
    public static final int STATE_PREPARING = 1;
    public static final int STATE_PLAYING = 2;
    public static final int STATE_PAUSED = 3;
    public static final int STATE_ERROR = 4;

    public static final int BUFFER_AUTO = 0;
    public static final int BUFFER_LARGE = 1;
    public static final int BUFFER_EXTRA_LARGE = 2;

    private final SloppaNativeActivity activity;
    private final Context context;
    private final HandlerThread playerThread;
    private final Handler handler;

    private volatile ExoPlayer player;
    private volatile AssSubtitleView assSubtitleView;
    private volatile AssHandler assHandler;
    private volatile SubtitleView textSubtitleView;
    private volatile boolean embeddedAudioSelectionApplied;
    private volatile boolean embeddedSubtitleSelectionApplied;
    private volatile boolean loggedTextSubtitleCue;
    private volatile int state = STATE_IDLE;
    private volatile String error = "";
    private volatile long positionMs;
    private volatile long durationMs;
    private volatile int videoWidth;
    private volatile int videoHeight;
    private volatile float playbackSpeed = 1.0f;
    private volatile boolean released;

    public SloppaPlayerBridge(SloppaNativeActivity activity) {
        this.activity = activity;
        this.context = activity.getApplicationContext();
        playerThread = new HandlerThread("sloppaTV-exoplayer");
        playerThread.start();
        handler = new Handler(playerThread.getLooper());
    }

    public void start(
        String url,
        Surface surface,
        long startPositionMs,
        int bufferPreset,
        int embeddedAudioOrdinal,
        int embeddedSubtitleOrdinal,
        String subtitleUrl,
        String subtitleCodec,
        String subtitleLanguage
    ) {
        if (released || url == null || url.isEmpty() || surface == null) {
            state = STATE_ERROR;
            error = "Missing playback surface or URL";
            return;
        }
        state = STATE_PREPARING;
        error = "";
        handler.post(() -> createPlayer(
            url,
            surface,
            Math.max(0L, startPositionMs),
            bufferPreset,
            embeddedAudioOrdinal,
            embeddedSubtitleOrdinal,
            subtitleUrl,
            subtitleCodec,
            subtitleLanguage
        ));
    }

    private void createPlayer(
        String url,
        Surface surface,
        long startPositionMs,
        int bufferPreset,
        int embeddedAudioOrdinal,
        int embeddedSubtitleOrdinal,
        String subtitleUrl,
        String subtitleCodec,
        String subtitleLanguage
    ) {
        if (released) return;
        releasePlayerOnly();
        try {
            DefaultLoadControl loadControl;
            if (bufferPreset == BUFFER_LARGE) {
                loadControl = new DefaultLoadControl.Builder()
                    .setBufferDurationsMs(50_000, 120_000, 2_500, 5_000)
                    .build();
            } else if (bufferPreset == BUFFER_EXTRA_LARGE) {
                loadControl = new DefaultLoadControl.Builder()
                    .setBufferDurationsMs(80_000, 240_000, 5_000, 10_000)
                    .build();
            } else {
                loadControl = new DefaultLoadControl();
            }

            DefaultRenderersFactory defaultRenderersFactory = new DefaultRenderersFactory(context)
                .setEnableDecoderFallback(true)
                .setExtensionRendererMode(DefaultRenderersFactory.EXTENSION_RENDERER_MODE_ON);

            boolean embeddedSubtitle = embeddedSubtitleOrdinal >= 0;
            boolean useLibass = (embeddedSubtitle || (subtitleUrl != null && !subtitleUrl.isEmpty()))
                && ("ass".equalsIgnoreCase(subtitleCodec) || "ssa".equalsIgnoreCase(subtitleCodec));
            boolean useEmbeddedText = embeddedSubtitle && isTextSubtitleCodec(subtitleCodec);
            embeddedAudioSelectionApplied = false;
            embeddedSubtitleSelectionApplied = false;
            loggedTextSubtitleCue = false;
            ExoPlayer.Builder playerBuilder = new ExoPlayer.Builder(context)
                .setLoadControl(loadControl);
            AssHandler localAssHandler = null;
            if (useLibass) {
                // Canvas is intentionally preferred over the libass OpenGL overlay: some
                // Android TV GLES stacks reject the extension's framebuffer operations.
                localAssHandler = new AssHandler(AssRenderType.OVERLAY_CANVAS, new AssHandlerConfig());
                AssSubtitleParserFactory parserFactory = new AssSubtitleParserFactory(localAssHandler);
                DefaultMediaSourceFactory mediaSourceFactory = new DefaultMediaSourceFactory(
                    new DefaultDataSource.Factory(context)
                );
                mediaSourceFactory.setSubtitleParserFactory(parserFactory);
                playerBuilder
                    .setMediaSourceFactory(mediaSourceFactory)
                    .setRenderersFactory(new AssRenderersFactory(localAssHandler, defaultRenderersFactory));
            } else {
                playerBuilder.setRenderersFactory(defaultRenderersFactory);
            }
            ExoPlayer created = playerBuilder.build();
            created.setAudioAttributes(
                new AudioAttributes.Builder()
                    .setUsage(C.USAGE_MEDIA)
                    .setContentType(C.AUDIO_CONTENT_TYPE_MOVIE)
                    .build(),
                true
            );
            created.setHandleAudioBecomingNoisy(true);
            created.addListener(new Player.Listener() {
                @Override
                public void onPlaybackStateChanged(int playbackState) {
                    if (player != created) return;
                    updateTelemetry(created);
                    Log.i(TAG, "Media3 state=" + playbackStateName(playbackState)
                        + " playWhenReady=" + created.getPlayWhenReady()
                        + " positionMs=" + created.getCurrentPosition()
                        + " bufferedMs=" + created.getBufferedPosition());
                    if (playbackState == Player.STATE_READY) {
                        state = created.getPlayWhenReady() ? STATE_PLAYING : STATE_PAUSED;
                    } else if (playbackState == Player.STATE_BUFFERING) {
                        state = STATE_PREPARING;
                    } else if (playbackState == Player.STATE_ENDED) {
                        positionMs = Math.max(positionMs, durationMs);
                        state = STATE_PAUSED;
                    }
                }

                @Override
                public void onPlayWhenReadyChanged(boolean playWhenReady, int reason) {
                    if (player != created) return;
                    updateTelemetry(created);
                    if (created.getPlaybackState() == Player.STATE_READY) {
                        state = playWhenReady ? STATE_PLAYING : STATE_PAUSED;
                    }
                }

                @Override
                public void onVideoSizeChanged(VideoSize videoSize) {
                    if (player != created) return;
                    videoWidth = videoSize.width;
                    videoHeight = videoSize.height;
                    Log.i(TAG, "Media3 video size=" + videoSize.width + "x" + videoSize.height);
                }

                @Override
                public void onRenderedFirstFrame() {
                    if (player != created) return;
                    Log.i(TAG, "Media3 rendered first frame at positionMs=" + created.getCurrentPosition());
                }

                @Override
                public void onTracksChanged(Tracks tracks) {
                    if (player != created) return;
                    if (embeddedAudioOrdinal >= 0 && !embeddedAudioSelectionApplied) {
                        embeddedAudioSelectionApplied = applyEmbeddedTrackSelection(
                            created,
                            tracks,
                            C.TRACK_TYPE_AUDIO,
                            embeddedAudioOrdinal,
                            "audio"
                        );
                    }
                    if (embeddedSubtitle && !embeddedSubtitleSelectionApplied) {
                        embeddedSubtitleSelectionApplied = applyEmbeddedTrackSelection(
                            created,
                            tracks,
                            C.TRACK_TYPE_TEXT,
                            embeddedSubtitleOrdinal,
                            "subtitle"
                        );
                    }
                }

                @Override
                public void onCues(CueGroup cueGroup) {
                    if (player != created) return;
                    SubtitleView subtitleView = textSubtitleView;
                    if (subtitleView == null) return;
                    if (!cueGroup.cues.isEmpty() && !loggedTextSubtitleCue) {
                        loggedTextSubtitleCue = true;
                        Log.i(TAG, "Media3 received embedded text subtitle cue count=" + cueGroup.cues.size());
                    }
                    activity.runOnUiThread(() -> {
                        if (textSubtitleView == subtitleView) subtitleView.setCues(cueGroup.cues);
                    });
                }

                @Override
                public void onPlayerError(PlaybackException playbackException) {
                    if (player != created) return;
                    error = playbackException.getMessage() == null
                        ? "ExoPlayer playback failed"
                        : playbackException.getMessage();
                    Throwable cause = playbackException;
                    while (cause != null) {
                        if (cause instanceof HttpDataSource.InvalidResponseCodeException) {
                            HttpDataSource.InvalidResponseCodeException httpError =
                                (HttpDataSource.InvalidResponseCodeException) cause;
                            Uri failedUri = httpError.dataSpec.uri;
                            Log.e(TAG, "Media3 HTTP " + httpError.responseCode
                                + " path=" + (failedUri == null ? "?" : failedUri.getPath()));
                            break;
                        }
                        cause = cause.getCause();
                    }
                    Log.e(TAG, "Media3 playback error", playbackException);
                    state = STATE_ERROR;
                }
            });
            player = created;
            created.setVideoSurface(surface);
            if (embeddedSubtitle) {
                // Prevent Media3's default subtitle choice from flashing before the exact
                // Jellyfin-selected embedded stream is overridden in onTracksChanged().
                created.setTrackSelectionParameters(
                    created.getTrackSelectionParameters().buildUpon()
                        .setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true)
                        .build()
                );
            }

            MediaItem.Builder mediaItem = new MediaItem.Builder().setUri(url);
            if (useEmbeddedText) {
                SubtitleView subtitleView = new SubtitleView(activity);
                textSubtitleView = subtitleView;
                activity.attachSubtitleOverlay(subtitleView);
            }
            if (useLibass) {
                if (subtitleUrl != null && !subtitleUrl.isEmpty()) {
                    MediaItem.SubtitleConfiguration subtitle = new MediaItem.SubtitleConfiguration.Builder(Uri.parse(subtitleUrl))
                        .setMimeType(MimeTypes.TEXT_SSA)
                        .setLanguage(subtitleLanguage == null || subtitleLanguage.isEmpty() ? null : subtitleLanguage)
                        .setSelectionFlags(C.SELECTION_FLAG_DEFAULT)
                        .build();
                    mediaItem.setSubtitleConfigurations(Collections.singletonList(subtitle));
                }
                localAssHandler.init(created);
                AssSubtitleView subtitleView = new AssSubtitleView(activity, localAssHandler);
                assHandler = localAssHandler;
                assSubtitleView = subtitleView;
                activity.attachSubtitleOverlay(subtitleView);
            }
            created.setMediaItem(mediaItem.build());
            if (startPositionMs > 0) created.seekTo(startPositionMs);
            created.prepare();
            created.play();
            scheduleTelemetry(created);
        } catch (RuntimeException exception) {
            error = exception.getMessage() == null ? "Unable to start ExoPlayer" : exception.getMessage();
            state = STATE_ERROR;
            releasePlayerOnly();
        }
    }

    private void scheduleTelemetry(ExoPlayer expectedPlayer) {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (released || player != expectedPlayer) return;
                updateTelemetry(expectedPlayer);
                handler.postDelayed(this, 100L);
            }
        }, 100L);
    }


    private boolean applyEmbeddedTrackSelection(
        ExoPlayer expectedPlayer,
        Tracks tracks,
        int trackType,
        int targetOrdinal,
        String label
    ) {
        int ordinal = 0;
        for (Tracks.Group group : tracks.getGroups()) {
            if (group.getType() != trackType) continue;
            for (int trackIndex = 0; trackIndex < group.length; ++trackIndex) {
                if (ordinal++ != targetOrdinal) continue;
                if (!group.isTrackSupported(trackIndex)) {
                    Log.w(TAG, "Selected embedded " + label + " is unsupported ordinal=" + targetOrdinal);
                    return false;
                }
                expectedPlayer.setTrackSelectionParameters(
                    expectedPlayer.getTrackSelectionParameters().buildUpon()
                        .setTrackTypeDisabled(trackType, false)
                        .clearOverridesOfType(trackType)
                        .setOverrideForType(new TrackSelectionOverride(group.getMediaTrackGroup(), trackIndex))
                        .build()
                );
                Log.i(TAG, "Media3 selected embedded " + label + " ordinal=" + targetOrdinal
                    + " mime=" + group.getTrackFormat(trackIndex).sampleMimeType
                    + " language=" + group.getTrackFormat(trackIndex).language);
                return true;
            }
        }
        Log.w(TAG, "Embedded " + label + " ordinal not found=" + targetOrdinal);
        return false;
    }

    private static boolean isTextSubtitleCodec(String codec) {
        if (codec == null) return false;
        return "srt".equalsIgnoreCase(codec)
            || "subrip".equalsIgnoreCase(codec)
            || "vtt".equalsIgnoreCase(codec)
            || "webvtt".equalsIgnoreCase(codec);
    }

    private static String playbackStateName(int state) {
        if (state == Player.STATE_IDLE) return "IDLE";
        if (state == Player.STATE_BUFFERING) return "BUFFERING";
        if (state == Player.STATE_READY) return "READY";
        if (state == Player.STATE_ENDED) return "ENDED";
        return Integer.toString(state);
    }

    private void updateTelemetry(ExoPlayer expectedPlayer) {
        if (player != expectedPlayer) return;
        positionMs = Math.max(0L, expectedPlayer.getCurrentPosition());
        long duration = expectedPlayer.getDuration();
        durationMs = duration == C.TIME_UNSET ? 0L : Math.max(0L, duration);
        playbackSpeed = expectedPlayer.getPlaybackParameters().speed;
        VideoSize size = expectedPlayer.getVideoSize();
        videoWidth = size.width;
        videoHeight = size.height;
    }

    public void togglePause() {
        handler.post(() -> {
            ExoPlayer current = player;
            if (current == null) return;
            if (current.isPlaying()) current.pause();
            else current.play();
            updateTelemetry(current);
        });
    }

    public void pause() {
        handler.post(() -> {
            ExoPlayer current = player;
            if (current == null) return;
            current.pause();
            updateTelemetry(current);
        });
    }

    public void play() {
        handler.post(() -> {
            ExoPlayer current = player;
            if (current == null) return;
            current.play();
            updateTelemetry(current);
        });
    }

    public void seekTo(long targetMs) {
        final long bounded = Math.max(0L, targetMs);
        positionMs = bounded;
        handler.post(() -> {
            ExoPlayer current = player;
            if (current == null) return;
            current.seekTo(bounded);
            updateTelemetry(current);
        });
    }

    public void seekBy(long deltaMs) {
        seekTo(positionMs + deltaMs);
    }

    public void selectEmbeddedAudioOrdinal(int ordinal) {
        if (ordinal < 0) return;
        handler.post(() -> {
            ExoPlayer current = player;
            if (current == null) return;
            embeddedAudioSelectionApplied = applyEmbeddedTrackSelection(
                current,
                current.getCurrentTracks(),
                C.TRACK_TYPE_AUDIO,
                ordinal,
                "audio"
            );
        });
    }

    public void setPlaybackSpeed(float speed) {
        final float bounded = Math.max(0.25f, Math.min(2.0f, speed));
        playbackSpeed = bounded;
        handler.post(() -> {
            ExoPlayer current = player;
            if (current != null) current.setPlaybackSpeed(bounded);
        });
    }

    public int getState() { return state; }
    public String getError() { return error; }
    public long getPositionMs() { return positionMs; }
    public long getDurationMs() { return durationMs; }
    public int getVideoWidth() { return videoWidth; }
    public int getVideoHeight() { return videoHeight; }
    public float getPlaybackSpeed() { return playbackSpeed; }

    public void release() {
        if (released) return;
        released = true;
        state = STATE_IDLE;
        handler.post(() -> {
            releasePlayerOnly();
            // ExoPlayer release may enqueue analytics cleanup on this same looper.
            // Queue shutdown after those callbacks instead of marking the HandlerThread
            // dead while Media3 is still finishing release work.
            handler.post(playerThread::quitSafely);
        });
    }

    private void releasePlayerOnly() {
        AssSubtitleView subtitleView = assSubtitleView;
        assSubtitleView = null;
        assHandler = null;
        if (subtitleView != null) activity.detachSubtitleOverlay(subtitleView);
        SubtitleView localTextSubtitleView = textSubtitleView;
        textSubtitleView = null;
        if (localTextSubtitleView != null) activity.detachSubtitleOverlay(localTextSubtitleView);

        ExoPlayer current = player;
        player = null;
        if (current != null) {
            try {
                current.clearVideoSurface();
                current.release();
            } catch (RuntimeException ignored) {
            }
        }
    }
}
