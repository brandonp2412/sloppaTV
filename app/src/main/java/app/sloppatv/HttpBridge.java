package app.sloppatv;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

public final class HttpBridge {
    private static final ExecutorService EXECUTOR = Executors.newFixedThreadPool(6, runnable -> {
        Thread thread = new Thread(runnable, "sloppa-http");
        thread.setDaemon(true);
        return thread;
    });

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

    public static Result perform(String method, String url, String[] headerPairs, byte[] requestBody) {
        Future<Result> future = null;
        try {
            future = EXECUTOR.submit(() -> performRequest(method, url, headerPairs, requestBody));
            return future.get(45, TimeUnit.SECONDS);
        } catch (Exception error) {
            if (future != null) future.cancel(true);
            if (error instanceof InterruptedException) Thread.currentThread().interrupt();
            Throwable cause = error.getCause() != null ? error.getCause() : error;
            return new Result(0, new byte[0], cause.toString(), "");
        }
    }

    private static Result performRequest(String method, String url, String[] headerPairs, byte[] requestBody) {
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
            return new Result(status, responseBody, "", setCookies.toString());
        } catch (Exception error) {
            return new Result(0, new byte[0], error.toString(), "");
        } finally {
            if (connection != null) connection.disconnect();
        }
    }
}
