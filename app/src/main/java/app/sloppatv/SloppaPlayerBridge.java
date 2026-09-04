package app.sloppatv;

import android.content.Context;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;

import androidx.media3.common.AudioAttributes;
import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.common.TrackSelectionOverride;
import androidx.media3.common.Tracks;
import androidx.media3.common.VideoSize;
import androidx.media3.datasource.HttpDataSource;
import androidx.media3.exoplayer.DefaultLoadControl;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;

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

    private final Context context;
    private final HandlerThread playerThread;
    private final Handler handler;

    private volatile ExoPlayer player;
    private volatile boolean embeddedAudioSelectionApplied;
    private volatile int state = STATE_IDLE;
    private volatile String error = "";
    private volatile long positionMs;
    private volatile long durationMs;
    private volatile int videoWidth;
    private volatile int videoHeight;
    private volatile boolean released;

    public SloppaPlayerBridge(SloppaNativeActivity activity) {
        this.context = activity.getApplicationContext();
        playerThread = new HandlerThread("sloppaTV-exoplayer");
        playerThread.start();
        handler = new Handler(playerThread.getLooper());
    }

    private boolean isBenchmarkBuild() {
        try {
            String versionName = context.getPackageManager()
                .getPackageInfo(context.getPackageName(), 0)
                .versionName;
            return versionName != null && versionName.endsWith("-benchmark");
        } catch (PackageManager.NameNotFoundException ignored) {
            return false;
        }
    }

    public void start(
        String url,
        Surface surface,
        long startPositionMs,
        int minBufferMs,
        int maxBufferMs,
        int bufferForPlaybackMs,
        int bufferForPlaybackAfterRebufferMs,
        int embeddedAudioOrdinal
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
            minBufferMs,
            maxBufferMs,
            bufferForPlaybackMs,
            bufferForPlaybackAfterRebufferMs,
            embeddedAudioOrdinal
        ));
    }

    private void createPlayer(
        String url,
        Surface surface,
        long startPositionMs,
        int minBufferMs,
        int maxBufferMs,
        int bufferForPlaybackMs,
        int bufferForPlaybackAfterRebufferMs,
        int embeddedAudioOrdinal
    ) {
        if (released) return;
        releasePlayerOnly();
        try {
            boolean customBufferDurations = minBufferMs >= 0
                && maxBufferMs >= 0
                && bufferForPlaybackMs >= 0
                && bufferForPlaybackAfterRebufferMs >= 0;
            DefaultLoadControl loadControl = customBufferDurations
                ? new DefaultLoadControl.Builder()
                    .setBufferDurationsMs(
                        minBufferMs,
                        maxBufferMs,
                        bufferForPlaybackMs,
                        bufferForPlaybackAfterRebufferMs
                    )
                    .build()
                : new DefaultLoadControl();
            Log.i(TAG, "Media3 buffer config custom=" + customBufferDurations
                + " minMs=" + minBufferMs
                + " maxMs=" + maxBufferMs
                + " startMs=" + bufferForPlaybackMs
                + " rebufferMs=" + bufferForPlaybackAfterRebufferMs);

            DefaultRenderersFactory defaultRenderersFactory = new DefaultRenderersFactory(context)
                .setEnableDecoderFallback(true)
                .setExtensionRendererMode(DefaultRenderersFactory.EXTENSION_RENDERER_MODE_ON);

            embeddedAudioSelectionApplied = false;
            ExoPlayer.Builder playerBuilder = new ExoPlayer.Builder(context)
                .setLoadControl(loadControl)
                .setRenderersFactory(defaultRenderersFactory);
            ExoPlayer created = playerBuilder.build();
            if (isBenchmarkBuild()) {
                created.setVolume(0.0f);
                Log.i(TAG, "Benchmark build audio forced to volume=0.0");
            }
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
                        state = created.getPlayWhenReady() ? STATE_PREPARING : STATE_PAUSED;
                    } else if (playbackState == Player.STATE_ENDED) {
                        positionMs = Math.max(positionMs, durationMs);
                        state = STATE_PAUSED;
                    }
                }

                @Override
                public void onPlayWhenReadyChanged(boolean playWhenReady, int reason) {
                    if (player != created) return;
                    updateTelemetry(created);
                    Log.i(TAG, "Media3 playWhenReady=" + playWhenReady
                        + " positionMs=" + created.getCurrentPosition()
                        + " bufferedMs=" + created.getBufferedPosition());
                    int playbackState = created.getPlaybackState();
                    if (playbackState == Player.STATE_READY) {
                        state = playWhenReady ? STATE_PLAYING : STATE_PAUSED;
                    } else if (playbackState == Player.STATE_BUFFERING) {
                        state = playWhenReady ? STATE_PREPARING : STATE_PAUSED;
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
            created.setMediaItem(new MediaItem.Builder().setUri(url).build(), startPositionMs);
            Log.i(TAG, "Media3 initial position requested=" + startPositionMs);
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
        VideoSize size = expectedPlayer.getVideoSize();
        videoWidth = size.width;
        videoHeight = size.height;
    }

    public void togglePause() {
        handler.post(() -> {
            ExoPlayer current = player;
            if (current == null) return;
            if (current.getPlayWhenReady()) current.pause();
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


    public int getState() { return state; }
    public String getError() { return error; }
    public long getPositionMs() { return positionMs; }
    public long getDurationMs() { return durationMs; }
    public int getVideoWidth() { return videoWidth; }
    public int getVideoHeight() { return videoHeight; }

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
