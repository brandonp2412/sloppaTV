package app.sloppatv;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;

public final class HttpBridge {
    private static final long REQUEST_TIMEOUT_MS = 45_000;
    private static final ScheduledExecutorService DEADLINE_EXECUTOR = Executors.newSingleThreadScheduledExecutor(
        runnable -> {
            Thread thread = new Thread(runnable, "sloppa-http-timeout");
            thread.setDaemon(true);
            return thread;
        }
    );
    private static final class RequestState {
        final AtomicBoolean cancelled = new AtomicBoolean(false);
        volatile HttpURLConnection connection;
    }

    private static final ConcurrentHashMap<Long, RequestState> ACTIVE_REQUESTS = new ConcurrentHashMap<>();

    public static final class Result {
        public final int status;
        public final byte[] body;
        public final String error;
        public final String setCookie;

        Result(int status, byte[] body, String error, String setCookie) {
            this.status = status;
            this.body = body;
            this.error = error;
            this.setCookie = setCookie;
        }
    }

    private HttpBridge() {}

    private static URL parseUrl(String value) throws MalformedURLException {
        try {
            URI uri = new URI(value);
            if (!uri.isAbsolute()) throw new MalformedURLException("no protocol: " + value);
            return uri.toURL();
        } catch (URISyntaxException error) {
            MalformedURLException malformed = new MalformedURLException(error.getMessage());
            malformed.initCause(error);
            throw malformed;
        }
    }

    public static Result perform(String method, String url, String[] headerPairs, byte[] requestBody) {
        return perform(method, url, headerPairs, requestBody, REQUEST_TIMEOUT_MS, 0);
    }

    static Result perform(
        String method,
        String url,
        String[] headerPairs,
        byte[] requestBody,
        long requestTimeoutMs
    ) {
        return perform(method, url, headerPairs, requestBody, requestTimeoutMs, 0);
    }

    static Result perform(
        String method,
        String url,
        String[] headerPairs,
        byte[] requestBody,
        long requestTimeoutMs,
        long requestId
    ) {
        HttpURLConnection connection = null;
        ScheduledFuture<?> deadline = null;
        AtomicBoolean timedOut = new AtomicBoolean(false);
        RequestState requestState =
            requestId == 0 ? null : ACTIVE_REQUESTS.computeIfAbsent(requestId, ignored -> new RequestState());
        Result result;
        try {
            connection = (HttpURLConnection) parseUrl(url).openConnection();
            if (requestState != null) {
                requestState.connection = connection;
                if (requestState.cancelled.get()) {
                    return new Result(0, new byte[0], "Request cancelled", "");
                }
            }
            HttpURLConnection activeConnection = connection;
            deadline = DEADLINE_EXECUTOR.schedule(
                () -> {
                    timedOut.set(true);
                    activeConnection.disconnect();
                },
                requestTimeoutMs,
                TimeUnit.MILLISECONDS
            );

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
            StringBuilder setCookies = new StringBuilder();
            for (int headerIndex = 0; ; ++headerIndex) {
                String key = connection.getHeaderFieldKey(headerIndex);
                String value = connection.getHeaderField(headerIndex);
                if (key == null && value == null) break;
                if (key != null && "Set-Cookie".equalsIgnoreCase(key) && value != null) {
                    if (setCookies.length() > 0) setCookies.append('\n');
                    setCookies.append(value);
                }
            }
            result = new Result(status, responseBody, "", setCookies.toString());
        } catch (Exception error) {
            result = new Result(0, new byte[0], error.toString(), "");
        } finally {
            if (deadline != null) deadline.cancel(false);
            if (requestState != null) {
                requestState.connection = null;
                ACTIVE_REQUESTS.remove(requestId, requestState);
            }
            if (connection != null) connection.disconnect();
        }

        if (timedOut.get()) {
            return new Result(0, new byte[0], new TimeoutException().toString(), "");
        }
        return result;
    }

    public static void register(long requestId) {
        if (requestId == 0) return;
        ACTIVE_REQUESTS.put(requestId, new RequestState());
    }

    public static void unregister(long requestId) {
        if (requestId == 0) return;
        ACTIVE_REQUESTS.remove(requestId);
    }

    public static void cancel(long requestId) {
        if (requestId == 0) return;
        RequestState requestState = ACTIVE_REQUESTS.get(requestId);
        if (requestState == null) return;
        requestState.cancelled.set(true);
        HttpURLConnection connection = requestState.connection;
        if (connection != null) connection.disconnect();
    }
}
