#!/usr/bin/env python3
"""Strict production Schannel policy, with per-handle test anchors only in the probe."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from windows_tls_fixtures import SignedCrlLab

PROBE = None
OPENSSL = shutil.which("openssl")
PROBE_TIMEOUT_SECONDS = 25


@unittest.skipUnless(sys.platform == "win32" and OPENSSL, "requires Windows and OpenSSL fixture generator")
class WindowsCurlTlsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lab = SignedCrlLab(OPENSSL)
        cls.addClassCleanup(cls.lab.close)
        cls.lab.prepare()

    def request(self, url, *, proxy=None, anchors=True, extra=(), executable=None, env=None):
        command = [str(executable or PROBE), "--url", url]
        if anchors:
            command += ["--test-ca-file", str(self.lab.root / "root.pem")]
        if proxy:
            command += ["--proxy", proxy]
        return subprocess.run(command + list(map(str, extra)), capture_output=True, text=True,
                              timeout=PROBE_TIMEOUT_SECONDS, env=env)

    def test_valid_certificate_and_head_with_a_verified_signed_crl(self):
        self.assertIn("verify OK", self.lab.crl_verification.stdout + self.lab.crl_verification.stderr)
        for extra in ((), ("--head", "1")):
            with self.subTest(extra=extra):
                result = self.request(self.lab.origins["good"], extra=extra)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("http_status=200", result.stdout)
        self.assertIn("/signed.crl", self.lab.crl_requests)

    def test_origin_revocation_hostname_and_redirect_are_enforced(self):
        targets = (self.lab.origins["revoked"], self.lab.origins["unavailable"],
                   self.lab.origins["good"].replace("localhost", "127.0.0.1"),
                   self.lab.origins["good"] + "/redirect")
        for target in targets:
            with self.subTest(target=target):
                result = self.request(target)
                self.assertEqual(result.returncode, 60, result.stderr)
        self.assertIn("/unavailable.crl", self.lab.crl_requests)

    def test_real_connect_preserves_verification_of_both_tls_layers(self):
        good = self.lab.origins["good"]
        proxy = self.lab.proxies["good"]
        result = self.request(good, proxy=proxy)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("http_status=200", result.stdout)
        self.assertTrue(self.lab.tunnels)
        for layer in ("origin", "proxy"):
            for name in ("revoked", "unavailable", "wrong-host"):
                with self.subTest(layer=layer, certificate=name):
                    endpoints = self.lab.origins if layer == "origin" else self.lab.proxies
                    bad = endpoints.get(name, endpoints["good"].replace("localhost", "127.0.0.1"))
                    result = self.request(bad if layer == "origin" else good,
                                          proxy=bad if layer == "proxy" else proxy)
                    self.assertEqual(result.returncode, 60, result.stderr)

    def test_reused_handle_revocation_bypass_flags_are_cleared(self):
        for mode, name in (("no-revoke", "revoked"), ("best-effort", "unavailable")):
            for layer in ("origin", "proxy"):
                with self.subTest(mode=mode, layer=layer):
                    result = self.request(self.lab.origins[name if layer == "origin" else "good"],
                                          proxy=self.lab.proxies[name] if layer == "proxy" else None,
                                          extra=("--seed-revocation", mode))
                    self.assertEqual(result.returncode, 60, result.stderr)

    def test_reused_handle_reenables_peer_and_hostname_verification(self):
        for anchors in (False, True):
            for layer in ("origin", "proxy"):
                with self.subTest(anchors=anchors, layer=layer):
                    # Trusted fixture + wrong hostname tests VERIFYHOST; without
                    # the fixture root the native policy must enforce VERIFYPEER.
                    target = self.lab.origins["good"]
                    proxy = self.lab.proxies["good"] if layer == "proxy" else None
                    if proxy and not anchors:
                        # Keep origin TLS out of this control: its untrusted
                        # issuer must not mask missing proxy peer verification.
                        target = target.replace("https://", "http://")
                    if anchors:
                        if proxy:
                            proxy = proxy.replace("localhost", "127.0.0.1")
                        else:
                            target = target.replace("localhost", "127.0.0.1")
                    result = self.request(target, proxy=proxy, anchors=anchors,
                                          extra=("--seed-insecure", "1"))
                    self.assertEqual(result.returncode, 60, result.stderr)

    def test_file_and_blob_anchors_are_cleared_with_available_revocation(self):
        # A valid signed CRL prevents unknown revocation from masking accidental
        # acceptance of a custom root. Test proxy trust independently over HTTP.
        for option in ("--ca-file", "--ca-blob"):
            for layer in ("origin", "proxy"):
                with self.subTest(option=option, layer=layer):
                    target = self.lab.origins["good"]
                    proxy = self.lab.proxies["good"] if layer == "proxy" else None
                    if proxy:
                        target = target.replace("https://", "http://")
                    result = self.request(target, proxy=proxy, anchors=False,
                                          extra=(option, self.lab.root / "root.pem"))
                    self.assertEqual(result.returncode, 60, result.stderr)

    def test_native_policy_ignores_legacy_adjacent_and_staged_ca_files(self):
        with tempfile.TemporaryDirectory(prefix="aifs-legacy-ca-") as temporary:
            root = Path(temporary)
            for name in (PROBE.name, "libcurl.dll", "zlib1.dll", "z.dll"):
                source = PROBE.parent / name
                if source.exists():
                    shutil.copyfile(source, root / name)
            paths = (root / "certs/cacert.pem", root / "profile/AIFileSorter/certs/cacert.pem")
            pem = (self.lab.root / "root.pem").read_bytes()
            for path in paths:
                path.parent.mkdir(parents=True)
                path.write_bytes(pem)
            env = dict(os.environ, AI_FILE_SORTER_CONFIG_DIR=str(root / "profile"))
            result = self.request(self.lab.origins["good"], anchors=False, executable=root / PROBE.name, env=env)
            self.assertEqual(result.returncode, 60, result.stderr)
            self.assertTrue(all(path.read_bytes() == pem for path in paths))
            env["AI_FILE_SORTER_CONFIG_DIR"] = str(root / "new-profile")
            result = self.request(self.lab.origins["good"], anchors=False, env=env)
            self.assertEqual(result.returncode, 60, result.stderr)
            self.assertFalse((root / "new-profile").exists(), "TLS must not stage a CA bundle")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True, type=Path)
    arguments, remaining = parser.parse_known_args()
    PROBE = arguments.probe.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
