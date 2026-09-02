package nz.presley.sloppatv;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.session.MediaSession;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

/**
 * Minimal Android platform bridge for APIs NativeActivity does not expose to native callbacks.
 * All application, navigation, rendering and playback behavior remains in C++.
 */
public final class SloppaNativeActivity extends NativeActivity {
    private static final int MEDIA_COMMAND_PLAY = 1;
    private static final int MEDIA_COMMAND_PAUSE = 2;
    private static final int MEDIA_COMMAND_STOP = 3;
    private static final int MEDIA_COMMAND_SEEK = 4;
    private static final int MEDIA_COMMAND_NEXT = 5;
    private static final int MEDIA_COMMAND_PREVIOUS = 6;

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        nativeOnActivityResult(requestCode, resultCode, data);
    }

    public SloppaPlayerBridge createPlayerBridge() {
        return new SloppaPlayerBridge(this);
    }

    public void attachSubtitleOverlay(View view) {
        if (view == null) return;
        runOnUiThread(() -> {
            if (view.getParent() != null) return;
            addContentView(
                view,
                new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
            );
            view.bringToFront();
        });
    }

    public void detachSubtitleOverlay(View view) {
        if (view == null) return;
        runOnUiThread(() -> {
            if (view.getParent() instanceof ViewGroup) {
                ((ViewGroup) view.getParent()).removeView(view);
            }
        });
    }

    /**
     * Returns {max output channels, direct-encoding bit mask}. This deliberately
     * describes the currently attached Android audio route rather than assuming
     * every codec the device can decode can also be sent to the TV/receiver.
     */
    public int[] queryAudioOutputCapabilities() {
        final int directAc3 = 1;
        final int directEac3 = 1 << 1;
        final int directDts = 1 << 2;
        final int directDtsHd = 1 << 3;
        final int directTrueHd = 1 << 4;

        int maxChannels = 2;
        int directMask = 0;
        AudioManager manager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (manager != null) {
            for (AudioDeviceInfo device : manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
                for (int channels : device.getChannelCounts()) {
                    maxChannels = Math.max(maxChannels, channels);
                }
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                    for (int encoding : device.getEncodings()) {
                        if (encoding == AudioFormat.ENCODING_AC3) directMask |= directAc3;
                        else if (encoding == AudioFormat.ENCODING_E_AC3 || encoding == AudioFormat.ENCODING_E_AC3_JOC) directMask |= directEac3;
                        else if (encoding == AudioFormat.ENCODING_DTS) directMask |= directDts;
                        else if (encoding == AudioFormat.ENCODING_DTS_HD) directMask |= directDtsHd;
                        else if (encoding == AudioFormat.ENCODING_DOLBY_TRUEHD) directMask |= directTrueHd;
                    }
                }
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            if (supportsDirectEncoding(AudioFormat.ENCODING_AC3, AudioFormat.CHANNEL_OUT_5POINT1)) directMask |= directAc3;
            if (supportsDirectEncoding(AudioFormat.ENCODING_E_AC3, AudioFormat.CHANNEL_OUT_5POINT1)) directMask |= directEac3;
            if (supportsDirectEncoding(AudioFormat.ENCODING_DTS, AudioFormat.CHANNEL_OUT_5POINT1)) directMask |= directDts;
            if (supportsDirectEncoding(AudioFormat.ENCODING_DTS_HD, AudioFormat.CHANNEL_OUT_7POINT1_SURROUND)) directMask |= directDtsHd;
            if (supportsDirectEncoding(AudioFormat.ENCODING_DOLBY_TRUEHD, AudioFormat.CHANNEL_OUT_7POINT1_SURROUND)) directMask |= directTrueHd;
        }

        if ((directMask & directTrueHd) != 0 || (directMask & directDtsHd) != 0) maxChannels = Math.max(maxChannels, 8);
        else if (directMask != 0) maxChannels = Math.max(maxChannels, 6);
        return new int[] { Math.max(2, Math.min(8, maxChannels)), directMask };
    }

    private static boolean supportsDirectEncoding(int encoding, int channelMask) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false;
        try {
            android.media.AudioAttributes attributes = new android.media.AudioAttributes.Builder()
                .setUsage(android.media.AudioAttributes.USAGE_MEDIA)
                .setContentType(android.media.AudioAttributes.CONTENT_TYPE_MOVIE)
                .build();
            AudioFormat format = new AudioFormat.Builder()
                .setEncoding(encoding)
                .setSampleRate(48_000)
                .setChannelMask(channelMask)
                .build();
            return AudioTrack.isDirectPlaybackSupported(format, attributes);
        } catch (RuntimeException ignored) {
            return false;
        }
    }

    public static MediaSession.Callback createMediaSessionCallback() {
        return new MediaSession.Callback() {
            @Override
            public void onPlay() {
                nativeOnMediaSessionCommand(MEDIA_COMMAND_PLAY, 0);
            }

            @Override
            public void onPause() {
                nativeOnMediaSessionCommand(MEDIA_COMMAND_PAUSE, 0);
            }

            @Override
            public void onStop() {
                nativeOnMediaSessionCommand(MEDIA_COMMAND_STOP, 0);
            }

            @Override
            public void onSeekTo(long positionMs) {
                nativeOnMediaSessionCommand(MEDIA_COMMAND_SEEK, positionMs);
            }

            @Override
            public void onSkipToNext() {
                nativeOnMediaSessionCommand(MEDIA_COMMAND_NEXT, 0);
            }

            @Override
            public void onSkipToPrevious() {
                nativeOnMediaSessionCommand(MEDIA_COMMAND_PREVIOUS, 0);
            }
        };
    }

    private static native void nativeOnActivityResult(int requestCode, int resultCode, Intent data);
    private static native void nativeOnMediaSessionCommand(int command, long positionMs);
}
