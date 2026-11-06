#!/usr/bin/env python3
"""Exercise the production trust policy against isolated loopback TLS servers."""

import argparse
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import unittest


PROBE = None
OPENSSL = shutil.which("openssl")


class ResponseHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.server.redirect:
            self.send_response(302)
            self.send_header("Location", self.server.redirect)
        else:
            self.send_response(200)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, *_):
        pass


@contextmanager
def server(cert=None, key=None, redirect=None):
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), ResponseHandler)
    httpd.redirect = redirect
    scheme = "http"
    if cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        httpd.socket = context.wrap_socket(httpd.socket, server_side=True)
        scheme = "https"
    worker = threading.Thread(target=httpd.serve_forever, daemon=True)
    worker.start()
    try:
        yield f"{scheme}://localhost:{httpd.server_port}/"
    finally:
        httpd.shutdown()
        httpd.server_close()
        worker.join(timeout=5)


@unittest.skipUnless(OPENSSL, "requires openssl to generate temporary certificate fixtures")
class CurlTlsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="aifs-tls-")
        cls.root = Path(cls.directory.name)
        (cls.root / "ca.cnf").write_text(
            "[req]\ndistinguished_name=dn\nx509_extensions=ca\n"
            "[dn]\n[ca]\nbasicConstraints=critical,CA:TRUE\n"
            "keyUsage=critical,keyCertSign,cRLSign\n")
        cls.run_openssl(
            "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
            "-nodes", "-days", "2", "-subj", "/CN=AI File Sorter TLS test CA",
            "-config", "ca.cnf",
            "-keyout", "ca.key", "-out", "ca.pem")
        for serial, hostname in enumerate(("localhost", "wrong.example"), start=1):
            cls.run_openssl(
                "req", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
                "-subj", f"/CN={hostname}", "-keyout", f"{hostname}.key",
                "-out", f"{hostname}.csr")
            (cls.root / "extensions.txt").write_text(
                f"subjectAltName=DNS:{hostname}\nextendedKeyUsage=serverAuth\n"
                "basicConstraints=critical,CA:FALSE\n")
            cls.run_openssl(
                "x509", "-req", "-in", f"{hostname}.csr", "-CA", "ca.pem",
                "-CAkey", "ca.key", "-set_serial", str(serial), "-days", "2",
                "-extfile", "extensions.txt", "-out", f"{hostname}.pem")

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    @classmethod
    def run_openssl(cls, *args):
        subprocess.run([OPENSSL, *args], cwd=cls.root, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)

    def probe(self, url=None, ca_file=None):
        command = [str(PROBE)]
        if url:
            command += ["--url", url]
        if ca_file:
            command += ["--ca-file", str(ca_file)]
        return subprocess.run(command, capture_output=True, text=True, timeout=20)

    def tls_server(self, hostname="localhost"):
        return server(self.root / f"{hostname}.pem", self.root / f"{hostname}.key")

    def test_linked_runtime_supports_platform_policy(self):
        result = self.probe()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_plain_http_remains_available(self):
        with server() as url:
            result = self.probe(url)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("http_status=200", result.stdout)

    def test_untrusted_certificate_is_rejected(self):
        with self.tls_server() as url:
            result = self.probe(url)
        self.assertEqual(result.returncode, 60, result.stderr)

    def test_redirect_cannot_bypass_certificate_verification(self):
        with self.tls_server() as target, server(redirect=target) as url:
            result = self.probe(url)
        self.assertEqual(result.returncode, 60, result.stderr)

    @unittest.skipIf(sys.platform == "darwin", "macOS deliberately clears file-based trust overrides")
    def test_linux_preserves_administrator_configured_ca(self):
        with self.tls_server() as url:
            result = self.probe(url, self.root / "ca.pem")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("http_status=200", result.stdout)

    @unittest.skipIf(sys.platform == "darwin", "requires a fixture CA outside the Apple trust store")
    def test_linux_checks_hostname_even_with_a_trusted_issuer(self):
        with self.tls_server("wrong.example") as url:
            result = self.probe(url, self.root / "ca.pem")
        self.assertEqual(result.returncode, 60, result.stderr)
        self.assertIn("localhost", result.stderr)

    @unittest.skipUnless(sys.platform == "darwin", "Apple trust policy")
    def test_macos_does_not_add_file_anchors_to_native_trust(self):
        with self.tls_server() as url:
            result = self.probe(url, self.root / "ca.pem")
        self.assertEqual(result.returncode, 60, result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True, type=Path)
    arguments, remaining = parser.parse_known_args()
    PROBE = arguments.probe.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
