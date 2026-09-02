package nz.presley.sloppatv;

import android.app.NativeActivity;
import android.content.Intent;
import android.media.session.MediaSession;

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
