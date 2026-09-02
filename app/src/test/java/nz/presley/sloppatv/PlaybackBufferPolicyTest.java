package nz.presley.sloppatv;

import java.util.Arrays;

public final class PlaybackBufferPolicyTest {
    private PlaybackBufferPolicyTest() {}

    public static void main(String[] args) {
        if (PlaybackBufferPolicy.durationsMs(PlaybackBufferPolicy.BUFFER_AUTO) != null) {
            throw new AssertionError("Auto must retain Media3 defaults");
        }
        assertDurations(
            PlaybackBufferPolicy.durationsMs(PlaybackBufferPolicy.BUFFER_LARGE),
            new int[] {50_000, 120_000, 2_500, 5_000}
        );
        assertDurations(
            PlaybackBufferPolicy.durationsMs(PlaybackBufferPolicy.BUFFER_EXTRA_LARGE),
            new int[] {80_000, 240_000, 5_000, 10_000}
        );
        if (PlaybackBufferPolicy.durationsMs(999) != null) {
            throw new AssertionError("Unknown presets must fall back to Media3 defaults");
        }
    }

    private static void assertDurations(int[] actual, int[] expected) {
        if (!Arrays.equals(actual, expected)) {
            throw new AssertionError("Expected " + Arrays.toString(expected) + " but got " + Arrays.toString(actual));
        }
    }
}
