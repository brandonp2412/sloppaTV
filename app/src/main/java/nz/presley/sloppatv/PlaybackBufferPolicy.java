package nz.presley.sloppatv;

/** Pure buffer-duration policy shared by Media3 construction and host tests. */
final class PlaybackBufferPolicy {
    static final int BUFFER_AUTO = 0;
    static final int BUFFER_LARGE = 1;
    static final int BUFFER_EXTRA_LARGE = 2;

    private PlaybackBufferPolicy() {}

    /**
     * Returns Media3 setBufferDurationsMs arguments as
     * minBuffer, maxBuffer, bufferForPlayback and bufferForPlaybackAfterRebuffer.
     * Auto returns null so Media3 keeps its platform defaults.
     */
    static int[] durationsMs(int preset) {
        if (preset == BUFFER_LARGE) {
            return new int[] {50_000, 120_000, 2_500, 5_000};
        }
        if (preset == BUFFER_EXTRA_LARGE) {
            return new int[] {80_000, 240_000, 5_000, 10_000};
        }
        return null;
    }
}
