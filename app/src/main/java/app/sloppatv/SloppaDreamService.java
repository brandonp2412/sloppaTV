package app.sloppatv;

import android.service.dreams.DreamService;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.json.JSONObject;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

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
        nativeStartDream(holder.getSurface(), uses24HourClock());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeStopDream();
    }

    private boolean uses24HourClock() {
        try {
            File session = new File(getFilesDir(), "session.json");
            if (!session.isFile()) return false;
            String raw = new String(Files.readAllBytes(session.toPath()), StandardCharsets.UTF_8);
            JSONObject settings = new JSONObject(raw).optJSONObject("settings");
            return settings != null && settings.optBoolean("clock24Hour", false);
        } catch (Exception ignored) {
            return false;
        }
    }

    private static native void nativeStartDream(Surface surface, boolean clock24Hour);
    private static native void nativeStopDream();
}
