package nz.presley.sloppatv;

import android.service.dreams.DreamService;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** Android DreamService lifecycle only; rendering remains in the native GLES layer. */
public final class SloppaDreamService extends DreamService implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("sloppatv");
    }

    private SurfaceView surfaceView;

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        setInteractive(false);
        setFullscreen(true);
        setScreenBright(false);
        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        setContentView(surfaceView);
    }

    @Override
    public void onDetachedFromWindow() {
        nativeStopDream();
        if (surfaceView != null) {
            surfaceView.getHolder().removeCallback(this);
            surfaceView = null;
        }
        super.onDetachedFromWindow();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        nativeStartDream(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // The native renderer queries the current EGL surface dimensions.
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeStopDream();
    }

    private static native void nativeStartDream(Surface surface);
    private static native void nativeStopDream();
}
