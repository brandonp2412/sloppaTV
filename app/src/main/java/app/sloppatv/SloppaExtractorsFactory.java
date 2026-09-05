package app.sloppatv;

import android.net.Uri;
import android.util.Log;

import androidx.media3.common.C;
import androidx.media3.extractor.DefaultExtractorsFactory;
import androidx.media3.extractor.Extractor;
import androidx.media3.extractor.ExtractorInput;
import androidx.media3.extractor.ExtractorOutput;
import androidx.media3.extractor.ForwardingExtractor;
import androidx.media3.extractor.ForwardingExtractorOutput;
import androidx.media3.extractor.ForwardingExtractorsFactory;
import androidx.media3.extractor.PositionHolder;
import androidx.media3.extractor.SeekMap;
import androidx.media3.extractor.SeekPoint;
import androidx.media3.extractor.TrackAwareSeekMap;
import androidx.media3.extractor.TrackOutput;
import androidx.media3.extractor.mkv.MatroskaExtractor;

import java.io.IOException;
import java.lang.reflect.Field;
import java.util.List;
import java.util.Map;

final class SloppaExtractorsFactory extends ForwardingExtractorsFactory {
    private static final String TAG = "sloppaTV/extractor";
    private volatile MatroskaSeekFixOutput.EstimatedSeekMap activeEstimatedSeekMap;

    SloppaExtractorsFactory() {
        super(new DefaultExtractorsFactory());
        Log.i(TAG, "Custom extractor factory created");
    }

    @Override
    public Extractor[] createExtractors() {
        return wrapMatroska(super.createExtractors());
    }

    @Override
    public Extractor[] createExtractors(Uri uri, Map<String, List<String>> responseHeaders) {
        return wrapMatroska(super.createExtractors(uri, responseHeaders));
    }

    boolean refineEstimatedSeek(long targetUs, long observedUs) {
        MatroskaSeekFixOutput.EstimatedSeekMap map = activeEstimatedSeekMap;
        return map != null && map.refine(targetUs, observedUs);
    }

    long estimatedSeekPosition(long targetUs) {
        MatroskaSeekFixOutput.EstimatedSeekMap map = activeEstimatedSeekMap;
        return map == null ? C.INDEX_UNSET : map.getSeekPoints(targetUs).first.position;
    }

    private Extractor[] wrapMatroska(Extractor[] extractors) {
        Log.i(TAG, "Extractor candidates=" + extractors.length);
        for (int index = 0; index < extractors.length; ++index) {
            Extractor underlying = extractors[index].getUnderlyingImplementation();
            if (underlying instanceof MatroskaExtractor) {
                Log.i(TAG, "Wrapping Matroska extractor class=" + extractors[index].getClass().getName());
                extractors[index] = new MatroskaSeekFixExtractor(extractors[index], this);
            }
        }
        return extractors;
    }

    private static final class MatroskaSeekFixExtractor extends ForwardingExtractor {
        private static final int CLUSTER_SCAN_CHUNK_BYTES = 64 * 1024;
        private static final int CLUSTER_SCAN_OVERLAP_BYTES = 64;
        private static final int MAX_CLUSTER_SCAN_BYTES = 32 * 1024 * 1024;
        private static final long RANDOM_ACCESS_PREROLL_US = 8_000_000L;
        private static final byte[] CLUSTER_ID = {(byte) 0x1F, (byte) 0x43, (byte) 0xB6, (byte) 0x75};

        private final SloppaExtractorsFactory owner;
        private final MatroskaExtractor matroskaExtractor;
        private MatroskaSeekFixOutput output;
        private boolean clusterResyncPending;
        private long alignedClusterPosition = C.INDEX_UNSET;
        private long seekTargetUs = C.TIME_UNSET;
        private int seekRefinementAttempts;
        private long timecodeScaleUs = C.TIME_UNSET;

        MatroskaSeekFixExtractor(Extractor extractor, SloppaExtractorsFactory owner) {
            super(extractor);
            this.owner = owner;
            this.matroskaExtractor = (MatroskaExtractor) extractor.getUnderlyingImplementation();
        }

        @Override
        public void init(ExtractorOutput output) {
            this.output = new MatroskaSeekFixOutput(output, owner);
            super.init(this.output);
        }

        @Override
        public int read(ExtractorInput input, PositionHolder seekPosition) throws IOException {
            if (output != null) output.setInputLength(input.getLength());
            if (clusterResyncPending) {
                ClusterMatch cluster = findNextCluster(input);
                if (cluster != null) {
                    long scaleUs = timecodeScaleUs();
                    long clusterTimeUs = scaleUs > 0 ? cluster.timecode * scaleUs : C.TIME_UNSET;
                    long desiredClusterUs = seekTargetUs > 0
                        ? Math.max(0L, seekTargetUs - RANDOM_ACCESS_PREROLL_US)
                        : C.TIME_UNSET;
                    if (desiredClusterUs >= 0
                        && clusterTimeUs > 0
                        && seekRefinementAttempts < 4
                        && Math.abs(clusterTimeUs - desiredClusterUs) > 2_000_000L
                        && owner.refineEstimatedSeek(desiredClusterUs, clusterTimeUs)) {
                        long refinedPosition = owner.estimatedSeekPosition(desiredClusterUs);
                        if (refinedPosition > 0 && refinedPosition != cluster.position) {
                            ++seekRefinementAttempts;
                            seekPosition.position = refinedPosition;
                            Log.i(TAG, "Refining Matroska cluster seek targetUs=" + seekTargetUs
                                + " prerollTargetUs=" + desiredClusterUs
                                + " clusterUs=" + clusterTimeUs
                                + " nextPosition=" + refinedPosition
                                + " attempt=" + seekRefinementAttempts);
                            return Extractor.RESULT_SEEK;
                        }
                    }
                    clusterResyncPending = false;
                    alignedClusterPosition = cluster.position;
                    seekPosition.position = cluster.position;
                    Log.i(TAG, "Aligned estimated Matroska seek to Cluster position=" + cluster.position
                        + " clusterUs=" + clusterTimeUs
                        + " targetUs=" + seekTargetUs
                        + " prerollUs=" + RANDOM_ACCESS_PREROLL_US);
                    return Extractor.RESULT_SEEK;
                }
                clusterResyncPending = false;
                Log.w(TAG, "No valid Matroska Cluster found near estimated seek position");
            }
            return super.read(input, seekPosition);
        }

        @Override
        public void seek(long position, long timeUs) {
            super.seek(position, timeUs);
            if (timeUs != seekTargetUs) {
                seekTargetUs = timeUs;
                seekRefinementAttempts = 0;
            }
            if (position > 0 && position == alignedClusterPosition) {
                alignedClusterPosition = C.INDEX_UNSET;
                clusterResyncPending = false;
            } else {
                clusterResyncPending = position > 0 && timeUs > 0;
            }
        }

        private long timecodeScaleUs() {
            if (timecodeScaleUs > 0) return timecodeScaleUs;
            try {
                Field field = MatroskaExtractor.class.getDeclaredField("timecodeScale");
                field.setAccessible(true);
                long scaleNs = field.getLong(matroskaExtractor);
                if (scaleNs > 0 && scaleNs != C.TIME_UNSET) timecodeScaleUs = Math.max(1L, scaleNs / 1000L);
            } catch (ReflectiveOperationException exception) {
                Log.w(TAG, "Unable to read Matroska timecode scale: " + exception.getMessage());
            }
            return timecodeScaleUs;
        }

        private static ClusterMatch findNextCluster(ExtractorInput input) throws IOException {
            long scanStart = input.getPosition();
            byte[] buffer = new byte[CLUSTER_SCAN_CHUNK_BYTES + CLUSTER_SCAN_OVERLAP_BYTES];
            int carry = 0;
            int scanned = 0;
            while (scanned < MAX_CLUSTER_SCAN_BYTES) {
                int request = Math.min(CLUSTER_SCAN_CHUNK_BYTES, MAX_CLUSTER_SCAN_BYTES - scanned);
                int count = input.read(buffer, carry, request);
                if (count == C.RESULT_END_OF_INPUT) return null;
                int total = carry + count;
                for (int index = 0; index <= total - 16; ++index) {
                    long timecode = validClusterTimecode(buffer, index, total);
                    if (timecode >= 0) {
                        return new ClusterMatch(scanStart + scanned - carry + index, timecode);
                    }
                }
                carry = Math.min(CLUSTER_SCAN_OVERLAP_BYTES, total);
                System.arraycopy(buffer, total - carry, buffer, 0, carry);
                scanned += count;
            }
            return null;
        }

        private static long validClusterTimecode(byte[] data, int offset, int limit) {
            if (offset + 8 >= limit
                || data[offset] != CLUSTER_ID[0]
                || data[offset + 1] != CLUSTER_ID[1]
                || data[offset + 2] != CLUSTER_ID[2]
                || data[offset + 3] != CLUSTER_ID[3]) {
                return -1;
            }
            int clusterSizeLength = vintLength(data[offset + 4]);
            if (clusterSizeLength == 0) return -1;
            int child = offset + 4 + clusterSizeLength;
            if (child + 3 >= limit || (data[child] & 0xFF) != 0xE7) return -1;
            int timecodeSizeLength = vintLength(data[child + 1]);
            if (timecodeSizeLength == 0) return -1;
            long timecodeSize = vintValue(data, child + 1, timecodeSizeLength);
            if (timecodeSize < 1 || timecodeSize > 8) return -1;
            int valueOffset = child + 1 + timecodeSizeLength;
            if (valueOffset + timecodeSize > limit) return -1;
            long timecode = 0;
            for (int index = 0; index < timecodeSize; ++index) {
                timecode = (timecode << 8) | (data[valueOffset + index] & 0xFFL);
            }
            return timecode;
        }

        private static int vintLength(byte firstByte) {
            int value = firstByte & 0xFF;
            if (value == 0) return 0;
            for (int length = 1; length <= 8; ++length) {
                if ((value & (0x80 >> (length - 1))) != 0) return length;
            }
            return 0;
        }

        private static long vintValue(byte[] data, int offset, int length) {
            long value = data[offset] & (0xFF >>> length);
            for (int index = 1; index < length; ++index) {
                value = (value << 8) | (data[offset + index] & 0xFFL);
            }
            return value;
        }

        private static final class ClusterMatch {
            final long position;
            final long timecode;

            ClusterMatch(long position, long timecode) {
                this.position = position;
                this.timecode = timecode;
            }
        }
    }

    private static final class MatroskaSeekFixOutput extends ForwardingExtractorOutput {
        private final SloppaExtractorsFactory owner;
        private int videoTrackId = C.INDEX_UNSET;
        private int fallbackAudioTrackId = C.INDEX_UNSET;
        private long inputLength = C.LENGTH_UNSET;
        private SeekMap pendingSeekMap;

        MatroskaSeekFixOutput(ExtractorOutput output, SloppaExtractorsFactory owner) {
            super(output);
            this.owner = owner;
        }

        void setInputLength(long inputLength) {
            if (inputLength > 0) this.inputLength = inputLength;
        }

        @Override
        public TrackOutput track(int id, int type) {
            Log.i(TAG, "Matroska track id=" + id + " type=" + type);
            TrackOutput output = super.track(id, type);
            if (type == C.TRACK_TYPE_VIDEO && videoTrackId == C.INDEX_UNSET) {
                videoTrackId = id;
                emitPendingSeekMap();
            } else if (type == C.TRACK_TYPE_AUDIO && fallbackAudioTrackId == C.INDEX_UNSET) {
                fallbackAudioTrackId = id;
            }
            return output;
        }

        @Override
        public void seekMap(SeekMap seekMap) {
            Log.i(TAG, "Matroska seekMap class=" + seekMap.getClass().getName()
                + " seekable=" + seekMap.isSeekable()
                + " trackAware=" + (seekMap instanceof TrackAwareSeekMap));
            if (seekMap.isSeekable()) {
                super.seekMap(seekMap);
                return;
            }
            if (seekMap instanceof TrackAwareSeekMap) {
                pendingSeekMap = seekMap;
                emitPendingSeekMap();
                return;
            }
            if (inputLength > 0 && seekMap.getDurationUs() > 0 && seekMap.getDurationUs() != C.TIME_UNSET) {
                EstimatedSeekMap estimated = new EstimatedSeekMap(seekMap, inputLength);
                owner.activeEstimatedSeekMap = estimated;
                super.seekMap(estimated);
                Log.i(TAG, "Repaired unindexed Matroska with estimated byte seeking length=" + inputLength
                    + " durationUs=" + seekMap.getDurationUs());
                return;
            }
            super.seekMap(seekMap);
        }

        @Override
        public void endTracks() {
            emitPendingSeekMap();
            if (pendingSeekMap != null) {
                super.seekMap(pendingSeekMap);
                pendingSeekMap = null;
            }
            super.endTracks();
        }

        private static final class EstimatedSeekMap implements SeekMap {
            private final long durationUs;
            private final long startPosition;
            private final long inputLength;
            private final long availableBytes;
            private volatile double scale = 1.0;

            EstimatedSeekMap(SeekMap original, long inputLength) {
                durationUs = original.getDurationUs();
                startPosition = Math.max(0L, original.getSeekPoints(0).first.position);
                this.inputLength = inputLength;
                availableBytes = Math.max(1L, inputLength - startPosition);
                Log.i(TAG, "Estimated Matroska map startPosition=" + startPosition
                    + " inputLength=" + inputLength + " durationUs=" + durationUs);
            }

            boolean refine(long targetUs, long observedUs) {
                if (targetUs <= 0 || observedUs <= 0) return false;
                double ratio = (double) targetUs / (double) observedUs;
                if (!Double.isFinite(ratio) || ratio < 0.5 || ratio > 2.0) return false;
                scale = Math.max(0.25, Math.min(4.0, scale * ratio));
                Log.i(TAG, "Refined estimated Matroska seek scale=" + scale
                    + " targetUs=" + targetUs + " observedUs=" + observedUs);
                return true;
            }

            @Override
            public boolean isSeekable() {
                return true;
            }

            @Override
            public long getDurationUs() {
                return durationUs;
            }

            @Override
            public SeekPoints getSeekPoints(long timeUs) {
                long boundedTimeUs = Math.max(0L, Math.min(timeUs, durationUs));
                double fraction = durationUs == 0 ? 0.0 : (double) boundedTimeUs / (double) durationUs;
                fraction = Math.max(0.0, Math.min(1.0, fraction * scale));
                long position = startPosition + (long) (availableBytes * fraction);
                position = Math.max(startPosition, position - (8L * 1024L * 1024L));
                position = Math.min(position, inputLength - 1);
                return new SeekPoints(new SeekPoint(boundedTimeUs, position));
            }

            @Override
            public boolean isEstimated() {
                return true;
            }
        }

        private void emitPendingSeekMap() {
            if (pendingSeekMap == null) return;
            int trackId = videoTrackId != C.INDEX_UNSET ? videoTrackId : C.INDEX_UNSET;
            if (trackId == C.INDEX_UNSET && videoTrackId == C.INDEX_UNSET && fallbackAudioTrackId != C.INDEX_UNSET) {
                return;
            }
            if (trackId == C.INDEX_UNSET) trackId = fallbackAudioTrackId;
            if (trackId == C.INDEX_UNSET) return;

            TrackAwareSeekMap trackAware = (TrackAwareSeekMap) pendingSeekMap;
            if (!trackAware.isSeekable(trackId)) return;
            SeekMap original = pendingSeekMap;
            final int selectedTrackId = trackId;
            super.seekMap(new SeekMap() {
                @Override
                public boolean isSeekable() {
                    return true;
                }

                @Override
                public long getDurationUs() {
                    return original.getDurationUs();
                }

                @Override
                public SeekPoints getSeekPoints(long timeUs) {
                    return trackAware.getSeekPoints(timeUs, selectedTrackId);
                }

                @Override
                public boolean isEstimated() {
                    return original.isEstimated();
                }
            });
            Log.i(TAG, "Repaired Matroska seek map with track-aware cues track=" + selectedTrackId);
            pendingSeekMap = null;
        }
    }
}
