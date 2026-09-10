package app.sloppatv;

import android.annotation.SuppressLint;
import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.session.MediaSession;
import android.os.Build;
import android.os.Bundle;
import android.os.Looper;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public final class SloppaNativeActivity extends NativeActivity {
    static {
        // NativeActivity dlopens the library for android_main, but Java native
        // callbacks also need the library associated with this app class loader.
        System.loadLibrary("sloppatv");
    }

    private static final int MEDIA_COMMAND_PLAY = 1;
    private static final int MEDIA_COMMAND_PAUSE = 2;
    private static final int MEDIA_COMMAND_STOP = 3;
    private static final int MEDIA_COMMAND_SEEK = 4;
    private static final int MEDIA_COMMAND_NEXT = 5;
    private static final int MEDIA_COMMAND_PREVIOUS = 6;
    private static final ExecutorService HTTP_EXECUTOR = Executors.newFixedThreadPool(6, runnable -> {
        Thread thread = new Thread(runnable, "sloppa-http");
        thread.setDaemon(true);
        return thread;
    });

    public static final class HttpResult {
        public final int status;
        public final byte[] body;
        public final String error;

        HttpResult(int status, byte[] body, String error) {
            this.status = status;
            this.body = body;
            this.error = error;
        }
    }

    private EditText nativeTextInput;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                this::dispatchBackToNative
            );
        }
    }

    private void dispatchBackToNative() {
        nativeOnBackPressed();
    }

    @Override
    @SuppressLint("GestureBackNavigation")
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        dispatchBackToNative();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                return;
            }
        }
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        );
    }

    public void setPlaybackKeepScreenOn(boolean enabled) {
        runOnUiThread(() -> {
            if (enabled) {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            } else {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public HttpResult performHttpRequestBridge(String method, String url, String[] headerPairs, byte[] requestBody) {
        Future<HttpResult> future = null;
        try {
            future = HTTP_EXECUTOR.submit(
                () -> performHttpRequest(method, url, headerPairs, requestBody)
            );
            return future.get(45, TimeUnit.SECONDS);
        } catch (Exception error) {
            if (future != null) future.cancel(true);
            if (error instanceof InterruptedException) Thread.currentThread().interrupt();
            Throwable cause = error.getCause() != null ? error.getCause() : error;
            return new HttpResult(0, new byte[0], cause.toString());
        }
    }

    private static HttpResult performHttpRequest(String method, String url, String[] headerPairs, byte[] requestBody) {
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setConnectTimeout(10_000);
            connection.setReadTimeout(30_000);
            connection.setInstanceFollowRedirects(true);
            connection.setRequestMethod(method);
            if (headerPairs != null) {
                for (int index = 0; index + 1 < headerPairs.length; index += 2) {
                    connection.setRequestProperty(headerPairs[index], headerPairs[index + 1]);
                }
            }
            if (requestBody != null && requestBody.length > 0) {
                connection.setDoOutput(true);
                try (OutputStream output = connection.getOutputStream()) {
                    output.write(requestBody);
                }
            }

            int status = connection.getResponseCode();
            InputStream stream = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            byte[] responseBody = new byte[0];
            if (stream != null) {
                try (InputStream input = stream; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                    byte[] buffer = new byte[16 * 1024];
                    int count;
                    while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
                    responseBody = output.toByteArray();
                }
            }
            return new HttpResult(status, responseBody, "");
        } catch (Exception error) {
            return new HttpResult(0, new byte[0], error.toString());
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        nativeOnActivityResult(requestCode, resultCode, data);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        if (intent == null) return;
        String action = intent.getAction();
        String data = intent.getDataString();
        String query = intent.getStringExtra("query");
        nativeOnNewIntent(action, data, query);
    }

    public MediaSession createMediaSessionBridge() {
        if (Looper.myLooper() == Looper.getMainLooper()) return createMediaSessionOnMainThread();
        AtomicReference<MediaSession> result = new AtomicReference<>();
        CountDownLatch ready = new CountDownLatch(1);
        runOnUiThread(() -> {
            try {
                result.set(createMediaSessionOnMainThread());
            } finally {
                ready.countDown();
            }
        });
        try {
            if (!ready.await(3, TimeUnit.SECONDS)) return null;
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt();
            return null;
        }
        return result.get();
    }

    private MediaSession createMediaSessionOnMainThread() {
        try {
            MediaSession session = new MediaSession(this, "sloppaTV");
            session.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS | MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS);
            session.setCallback(createMediaSessionCallback());
            return session;
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    public boolean showTextInput(String initialText, String hint, int mode, boolean password) {
        InputMethodManager inputManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (inputManager == null) return false;
        final String startingText = initialText == null ? "" : initialText;
        final String inputHint = hint == null ? "" : hint;
        runOnUiThread(() -> {
            removeNativeTextInput(false);
            EditText input = new EditText(this) {
                @Override
                public boolean onKeyPreIme(int keyCode, KeyEvent event) {
                    if (keyCode == KeyEvent.KEYCODE_BACK) {
                        if (event.getAction() == KeyEvent.ACTION_UP) {
                            nativeOnSystemTextInputCancelled(mode, getText().toString());
                            post(() -> removeNativeTextInput(true));
                        }
                        return true;
                    }
                    return super.onKeyPreIme(keyCode, event);
                }
            };
            nativeTextInput = input;
            input.setSingleLine(true);
            input.setHint(inputHint);
            input.setText(startingText);
            input.setSelection(startingText.length());
            int imeAction = mode == 1
                ? EditorInfo.IME_ACTION_SEARCH
                : ((mode == 10 || mode == 11) ? EditorInfo.IME_ACTION_NEXT : EditorInfo.IME_ACTION_DONE);
            input.setImeOptions(imeAction);
            int inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
            if (password) inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
            else if (mode == 10) inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
            input.setInputType(inputType);
            input.setBackgroundColor(Color.TRANSPARENT);
            input.setTextColor(Color.TRANSPARENT);
            input.setHintTextColor(Color.TRANSPARENT);
            input.setCursorVisible(false);
            input.setAlpha(0.01f);
            input.addTextChangedListener(new TextWatcher() {
                @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
                @Override public void onTextChanged(CharSequence s, int start, int before, int count) {
                    nativeOnSystemTextInputChanged(mode, s.toString());
                }
                @Override public void afterTextChanged(Editable s) {}
            });
            input.setOnEditorActionListener((view, actionId, event) -> {
                if (actionId == EditorInfo.IME_ACTION_SEARCH
                    || actionId == EditorInfo.IME_ACTION_DONE
                    || actionId == EditorInfo.IME_ACTION_GO
                    || actionId == EditorInfo.IME_ACTION_NEXT) {
                    String value = input.getText().toString();
                    nativeOnSystemTextInputDone(mode, value);
                    removeNativeTextInput(true);
                    return true;
                }
                return false;
            });
            FrameLayout.LayoutParams layout = new FrameLayout.LayoutParams(2, 2);
            addContentView(input, layout);
            input.requestFocus();
            input.post(() -> inputManager.showSoftInput(input, 0));
        });
        return true;
    }

    public void hideTextInput() {
        runOnUiThread(() -> removeNativeTextInput(true));
    }

    private void removeNativeTextInput(boolean hideKeyboard) {
        EditText input = nativeTextInput;
        nativeTextInput = null;
        if (input == null) return;
        if (hideKeyboard) {
            InputMethodManager manager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            if (manager != null) manager.hideSoftInputFromWindow(input.getWindowToken(), 0);
        }
        if (input.getParent() instanceof ViewGroup) {
            ((ViewGroup) input.getParent()).removeView(input);
        }
        hideSystemBars();
    }

    private Paint createUiFontPaint() {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        paint.setColor(Color.WHITE);
        paint.setTextAlign(Paint.Align.LEFT);
        paint.setTypeface(Typeface.create(Typeface.SANS_SERIF, Typeface.NORMAL));
        paint.setTextSize(48.0f);
        return paint;
    }

    private Bitmap createFontAtlas(Paint.Style style, float strokeWidth) {
        final int columns = 16;
        final int rows = 6;
        final int cellWidth = 64;
        final int cellHeight = 64;
        final float leftPadding = 5.0f;
        Bitmap bitmap = Bitmap.createBitmap(columns * cellWidth, rows * cellHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT);
        Paint paint = createUiFontPaint();
        paint.setStyle(style);
        paint.setStrokeWidth(strokeWidth);
        paint.setStrokeJoin(Paint.Join.ROUND);
        Paint.FontMetrics metrics = paint.getFontMetrics();
        for (int index = 0; index < 95; ++index) {
            int column = index % columns;
            int row = index / columns;
            float left = column * cellWidth + leftPadding;
            float top = row * cellHeight;
            float baseline = top + (cellHeight - metrics.bottom - metrics.top) * 0.5f;
            canvas.drawText(String.valueOf((char) (32 + index)), left, baseline, paint);
        }
        return bitmap;
    }

    public Bitmap createFontAtlas() {
        return createFontAtlas(Paint.Style.FILL, 0.0f);
    }

    public Bitmap createFontOutlineAtlas() {
        return createFontAtlas(Paint.Style.STROKE, 5.0f);
    }

    public float[] createFontAdvances() {
        Paint paint = createUiFontPaint();
        float[] advances = new float[95];
        for (int index = 0; index < advances.length; ++index) {
            advances[index] = paint.measureText(String.valueOf((char) (32 + index)));
        }
        return advances;
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
    private static native void nativeOnNewIntent(String action, String data, String query);
    private static native void nativeOnBackPressed();
    private static native void nativeOnMediaSessionCommand(int command, long positionMs);
    private static native void nativeOnSystemTextInputChanged(int mode, String text);
    private static native void nativeOnSystemTextInputDone(int mode, String text);
    private static native void nativeOnSystemTextInputCancelled(int mode, String text);
}
