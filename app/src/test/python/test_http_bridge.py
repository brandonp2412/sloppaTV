from __future__ import annotations

import base64
import contextlib
import http.server
import pathlib
import shutil
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[4]
HTTP_BRIDGE = ROOT / "app" / "src" / "main" / "java" / "app" / "sloppatv" / "HttpBridge.java"


class _Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path == "/slow":
            self.send_response(200)
            self.end_headers()
            time.sleep(0.5)
            with contextlib.suppress(BrokenPipeError):
                self.wfile.write(b"late")
            return
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/final")
            self.end_headers()
            return
        if self.path == "/oversized":
            self.send_response(200)
            self.send_header("Content-Length", str(64 * 1024 * 1024 + 1))
            self.end_headers()
            return
        if self.path == "/final":
            self.send_response(200)
            self.send_header("Set-Cookie", "first=one; Path=/")
            self.send_header("Set-Cookie", "second=two; Path=/")
            self.end_headers()
            self.wfile.write(b"redirected")
            return
        self.send_response(404)
        self.end_headers()

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        payload = b"POST|" + self.headers.get("X-Test", "").encode() + b"|" + body
        self.send_response(201)
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, format: str, *args: object) -> None:
        pass


class HttpBridgeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._temp = tempfile.TemporaryDirectory()
        cls.classes = pathlib.Path(cls._temp.name)
        cls.harness = cls.classes / "HttpBridgeHarness.java"
        cls.harness.write_text(
            """
package app.sloppatv;

import java.nio.charset.StandardCharsets;
import java.util.Base64;

public final class HttpBridgeHarness {
    private static String encode(String value) {
        return Base64.getEncoder().encodeToString(value.getBytes(StandardCharsets.UTF_8));
    }

    public static void main(String[] args) {
        if (args[0].equals("CANCEL") || args[0].equals("CANCEL_REUSE")) {
            final HttpBridge.Result[] result = new HttpBridge.Result[1];
            HttpBridge.register(42);
            Thread request = new Thread(() -> result[0] = HttpBridge.perform(
                "GET", args[1], null, new byte[0], 10_000, 42
            ));
            request.start();
            try {
                Thread.sleep(50);
                HttpBridge.cancel(42);
                request.join(2_000);
            } catch (InterruptedException exception) {
                throw new RuntimeException(exception);
            }
            if (request.isAlive()) throw new RuntimeException("request did not cancel");
            if (args[0].equals("CANCEL_REUSE")) {
                HttpBridge.register(42);
                result[0] = HttpBridge.perform("GET", args[2], null, new byte[0], 10_000, 42);
            }
            System.out.println(result[0].status);
            System.out.println(Base64.getEncoder().encodeToString(result[0].body));
            System.out.println(encode(result[0].error));
            System.out.println(encode(result[0].setCookie));
            return;
        }
        if (args[0].equals("PRECANCEL")) {
            HttpBridge.register(43);
            HttpBridge.cancel(43);
            HttpBridge.Result result = HttpBridge.perform("GET", args[1], null, new byte[0], 10_000, 43);
            System.out.println(result.status);
            System.out.println(Base64.getEncoder().encodeToString(result.body));
            System.out.println(encode(result.error));
            System.out.println(encode(result.setCookie));
            return;
        }
        if (args[0].equals("UNREGISTER")) {
            HttpBridge.register(44);
            HttpBridge.unregister(44);
            HttpBridge.cancel(44);
            HttpBridge.Result result = HttpBridge.perform("GET", args[1], null, new byte[0], 10_000, 44);
            System.out.println(result.status);
            System.out.println(Base64.getEncoder().encodeToString(result.body));
            System.out.println(encode(result.error));
            System.out.println(encode(result.setCookie));
            return;
        }
        byte[] body = args.length > 2 ? args[2].getBytes(StandardCharsets.UTF_8) : new byte[0];
        HttpBridge.Result result = args.length > 3
            ? HttpBridge.perform(
                args[0],
                args[1],
                new String[] {"X-Test", "bridge"},
                body,
                Long.parseLong(args[3])
            )
            : HttpBridge.perform(
                args[0],
                args[1],
                new String[] {"X-Test", "bridge"},
                body
            );
        System.out.println(result.status);
        System.out.println(Base64.getEncoder().encodeToString(result.body));
        System.out.println(encode(result.error));
        System.out.println(encode(result.setCookie));
    }
}
""".strip()
        )
        javac = shutil.which("javac")
        if javac is None:
            raise RuntimeError("javac is required for HttpBridge host tests")
        subprocess.run(
            [javac, "-d", str(cls.classes), str(HTTP_BRIDGE), str(cls.harness)],
            check=True,
            cwd=ROOT,
        )

        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
        cls.server_thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.server_thread.start()
        cls.base_url = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.server.shutdown()
        cls.server.server_close()
        cls.server_thread.join(timeout=2)
        cls._temp.cleanup()

    def _request(
        self,
        method: str,
        url: str,
        body: str = "",
        timeout_ms: int | None = None,
    ) -> tuple[int, bytes, str, str]:
        command = [
            "java",
            "-cp",
            str(self.classes),
            "app.sloppatv.HttpBridgeHarness",
            method,
            url,
            body,
        ]
        if timeout_ms is not None:
            command.append(str(timeout_ms))
        completed = subprocess.run(
            command,
            check=True,
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        lines = completed.stdout.splitlines()
        self.assertEqual(len(lines), 4)
        return (
            int(lines[0]),
            base64.b64decode(lines[1]),
            base64.b64decode(lines[2]).decode(),
            base64.b64decode(lines[3]).decode(),
        )

    def test_follows_redirects_and_collects_cookies(self) -> None:
        status, body, error, cookies = self._request("GET", self.base_url + "/redirect")
        self.assertEqual(status, 200)
        self.assertEqual(body, b"redirected")
        self.assertEqual(error, "")
        self.assertIn("first=one; Path=/", cookies)
        self.assertIn("second=two; Path=/", cookies)

    def test_sends_headers_and_request_body(self) -> None:
        status, body, error, cookies = self._request("POST", self.base_url + "/echo", "payload")
        self.assertEqual(status, 201)
        self.assertEqual(body, b"POST|bridge|payload")
        self.assertEqual(error, "")
        self.assertEqual(cookies, "")

    def test_enforces_overall_request_deadline(self) -> None:
        started = time.monotonic()
        status, body, error, cookies = self._request(
            "GET",
            self.base_url + "/slow",
            timeout_ms=100,
        )
        self.assertLess(time.monotonic() - started, 1.0)
        self.assertEqual(status, 0)
        self.assertEqual(body, b"")
        self.assertEqual(error, "java.util.concurrent.TimeoutException")
        self.assertEqual(cookies, "")

    def test_cancels_an_active_request(self) -> None:
        started = time.monotonic()
        status, body, error, cookies = self._request("CANCEL", self.base_url + "/slow")
        self.assertLess(time.monotonic() - started, 1.0)
        self.assertEqual(status, 0)
        self.assertEqual(body, b"")
        self.assertNotEqual(error, "")
        self.assertEqual(cookies, "")

    def test_cancelled_request_id_does_not_poison_reuse(self) -> None:
        status, body, error, cookies = self._request("CANCEL_REUSE", self.base_url + "/slow", self.base_url + "/final")
        self.assertEqual(status, 200)
        self.assertEqual(body, b"redirected")
        self.assertEqual(error, "")
        self.assertIn("first=one; Path=/", cookies)
        self.assertIn("second=two; Path=/", cookies)

    def test_cancel_before_connection_registration_is_honoured(self) -> None:
        started = time.monotonic()
        status, body, error, cookies = self._request("PRECANCEL", self.base_url + "/slow")
        self.assertLess(time.monotonic() - started, 0.5)
        self.assertEqual(status, 0)
        self.assertEqual(body, b"")
        self.assertEqual(error, "Request cancelled")
        self.assertEqual(cookies, "")

    def test_unregister_removes_request_state(self) -> None:
        status, body, error, cookies = self._request("UNREGISTER", self.base_url + "/final")
        self.assertEqual(status, 200)
        self.assertEqual(body, b"redirected")
        self.assertEqual(error, "")
        self.assertIn("second=two; Path=/", cookies)

    def test_rejects_oversized_response_before_buffering_body(self) -> None:
        status, body, error, cookies = self._request("GET", self.base_url + "/oversized")
        self.assertEqual(status, 0)
        self.assertEqual(body, b"")
        self.assertIn("HTTP response exceeds 64 MiB limit", error)
        self.assertEqual(cookies, "")

    def test_returns_transport_errors_as_result(self) -> None:
        status, body, error, cookies = self._request("GET", "not-a-url")
        self.assertEqual(status, 0)
        self.assertEqual(body, b"")
        self.assertIn("MalformedURLException", error)
        self.assertEqual(cookies, "")


if __name__ == "__main__":
    unittest.main()
