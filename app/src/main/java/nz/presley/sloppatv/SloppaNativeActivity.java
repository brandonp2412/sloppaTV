package nz.presley.sloppatv;

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
import android.os.Looper;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Minimal Android platform bridge for APIs NativeActivity does not expose to native callbacks.
 * All application, navigation, rendering and playback behavior remains in C++.
 */
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

    private EditText nativeTextInput;

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

    public SloppaPlayerBridge createPlayerBridge() {
        return new SloppaPlayerBridge(this);
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

    /**
     * Uses the TV's configured IME (normally Gboard on Android TV) instead of
     * forcing users through the native fallback keyboard.
     */
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
                            post(() -> removeNativeTextInput(false));
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
    }

    private Paint createUiFontPaint() {
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        paint.setColor(Color.WHITE);
        paint.setTextAlign(Paint.Align.LEFT);
        paint.setTypeface(Typeface.create(Typeface.SANS_SERIF, Typeface.NORMAL));
        paint.setTextSize(48.0f);
        return paint;
    }

    /** Returns a high-resolution antialiased ASCII atlas using Android's proportional system sans font. */
    public Bitmap createFontAtlas() {
        final int columns = 16;
        final int rows = 6;
        final int cellWidth = 64;
        final int cellHeight = 64;
        final float leftPadding = 5.0f;
        Bitmap bitmap = Bitmap.createBitmap(columns * cellWidth, rows * cellHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT);
        Paint paint = createUiFontPaint();
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

    /** Per-glyph advances for the atlas above, in source bitmap pixels. */
    public float[] createFontAdvances() {
        Paint paint = createUiFontPaint();
        float[] advances = new float[95];
        for (int index = 0; index < advances.length; ++index) {
            advances[index] = paint.measureText(String.valueOf((char) (32 + index)));
        }
        return advances;
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
    private static native void nativeOnNewIntent(String action, String data, String query);
    private static native void nativeOnMediaSessionCommand(int command, long positionMs);
    private static native void nativeOnSystemTextInputChanged(int mode, String text);
    private static native void nativeOnSystemTextInputDone(int mode, String text);
    private static native void nativeOnSystemTextInputCancelled(int mode, String text);
}
