package nz.presley.sloppatv;

import android.app.NativeActivity;
import android.content.Intent;

/**
 * Minimal Android platform bridge for APIs NativeActivity does not expose to native callbacks.
 * All application, navigation, rendering and playback behavior remains in C++.
 */
public final class SloppaNativeActivity extends NativeActivity {
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        nativeOnActivityResult(requestCode, resultCode, data);
    }

    private static native void nativeOnActivityResult(int requestCode, int resultCode, Intent data);
}
