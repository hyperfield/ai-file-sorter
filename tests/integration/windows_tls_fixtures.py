"""Ephemeral signed-CRL and TLS CONNECT fixtures; no system certificate imports."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import select
import socket
import ssl
import subprocess
import tempfile
import threading
import time

BLOCK_BYTES = 16384
SOCKET_TIMEOUT_SECONDS = 5
TUNNEL_TIMEOUT_SECONDS = 30
TUNNEL_POLL_SECONDS = 0.1
OPENSSL_TIMEOUT_SECONDS = 30


class QuietServer(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):
        # Clients rejecting TLS close sockets without sending HTTP.
        pass


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        lab = self.server.lab
        if self.server.kind == "crl":
            found = self.path == "/signed.crl"
            lab.crl_requests.append(self.path)
            if not found:
                self.send_error(404)
                return
            data = (lab.root / "signed.crl").read_bytes()
        else:
            data = b"verified fixture\n"
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", lab.origins["revoked"])
        else:
            self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def do_HEAD(self):
        self.do_GET()

    def do_CONNECT(self):
        host, port = self.path.rsplit(":", 1)
        lab = self.server.lab
        if host not in ("localhost", "127.0.0.1") or int(port) not in lab.origin_ports:
            self.send_error(403)
            return
        with socket.create_connection((host, int(port)), timeout=SOCKET_TIMEOUT_SECONDS) as upstream:
            self.send_response(200, "Connection established")
            self.end_headers()
            self.close_connection = True
            lab.tunnels.append(self.path)
            self.forward(upstream)

    def forward(self, upstream):
        peer = self.connection
        peer.settimeout(SOCKET_TIMEOUT_SECONDS)
        upstream.settimeout(SOCKET_TIMEOUT_SECONDS)
        deadline = time.monotonic() + TUNNEL_TIMEOUT_SECONDS
        while time.monotonic() < deadline:
            ready, _, _ = select.select([peer, upstream], [], [], TUNNEL_POLL_SECONDS)
            if peer.pending() and peer not in ready:
                ready.append(peer)
            for source in ready:
                data = source.recv(BLOCK_BYTES)
                if not data:
                    return
                (upstream if source is peer else peer).sendall(data)


class SignedCrlLab:
    """Own disposable CA keys, a signed CRL, origins and functioning TLS proxies."""

    def __init__(self, openssl):
        self.openssl = openssl
        self.temporary = tempfile.TemporaryDirectory(prefix="aifs-schannel-")
        self.root = Path(self.temporary.name)
        self.servers = []
        self.crl_requests = []
        self.tunnels = []

    def run(self, *arguments):
        return subprocess.run([self.openssl, *arguments], cwd=self.root, check=True,
                              capture_output=True, text=True, timeout=OPENSSL_TIMEOUT_SECONDS)

    def start_server(self, kind, certificate=None):
        server = QuietServer(("127.0.0.1", 0), Handler)
        server.kind, server.lab = kind, self
        if certificate:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(self.root / (certificate + ".pem"),
                                    self.root / (certificate + ".key"))
            server.socket = context.wrap_socket(server.socket, server_side=True)
        self.servers.append(server)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        return server.server_port

    def prepare(self):
        crl_port = self.start_server("crl")
        self.run("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
                 "-subj", "/CN=AIFS ephemeral revocation fixture", "-keyout", "root.key", "-out", "root.pem",
                 "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign")
        for name, data in {"index.txt": "", "index.txt.attr": "unique_subject = no\n",
                           "serial": "1000\n", "crlnumber": "1000\n"}.items():
            (self.root / name).write_text(data)
        (self.root / "issued").mkdir()
        config = f"""[ca]
default_ca = fixture
[fixture]
dir = {self.root.as_posix()}
database = $dir/index.txt
new_certs_dir = $dir/issued
certificate = $dir/root.pem
private_key = $dir/root.key
serial = $dir/serial
crlnumber = $dir/crlnumber
default_md = sha256
default_days = 2
default_crl_days = 2
policy = policy
unique_subject = no
[policy]
commonName = supplied
"""
        for name in ("good", "revoked", "unavailable"):
            endpoint = "unavailable.crl" if name == "unavailable" else "signed.crl"
            config += f"""[{name}]
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost
crlDistributionPoints=URI:http://127.0.0.1:{crl_port}/{endpoint}
"""
        (self.root / "ca.cnf").write_text(config)
        self.issue_certificates()
        self.origins = {name: "https://localhost:" + str(self.start_server("origin", name))
                        for name in ("good", "revoked", "unavailable")}
        self.proxies = {name: "https://localhost:" + str(self.start_server("proxy", name))
                        for name in ("good", "revoked", "unavailable")}
        self.origin_ports = {int(url.rsplit(":", 1)[1]) for url in self.origins.values()}

    def issue_certificates(self):
        for name in ("good", "revoked", "unavailable"):
            self.run("req", "-new", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost",
                     "-keyout", name + ".key", "-out", name + ".csr")
            self.run("ca", "-batch", "-notext", "-config", "ca.cnf", "-in", name + ".csr",
                     "-out", name + ".pem", "-extensions", name)
        self.run("ca", "-config", "ca.cnf", "-revoke", "revoked.pem", "-crl_reason", "keyCompromise")
        self.run("ca", "-config", "ca.cnf", "-gencrl", "-out", "signed-crl.pem")
        self.crl_verification = self.run("crl", "-in", "signed-crl.pem", "-verify",
                                         "-CAfile", "root.pem", "-noout")
        self.run("crl", "-in", "signed-crl.pem", "-outform", "DER", "-out", "signed.crl")

    def close(self):
        for server in self.servers:
            server.shutdown()
            server.server_close()
        self.temporary.cleanup()
