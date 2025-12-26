#!/usr/bin/env python3
"""Security Unit Tests for Macless Haystack - Tests fail when vulnerabilities are present."""

import json
import os
import sys
import unittest
from io import StringIO

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "endpoint"))
import mh_config




class TestHTTPSConfiguration(unittest.TestCase):
    def test_certificate_file_must_exist_for_https(self):
        cert_path = mh_config.getCertFile()
        self.assertTrue(os.path.isfile(cert_path), f"No certificate at {cert_path}")
    
    def test_certificate_path_is_configured(self):
        cert_file = mh_config.getCertFile()
        self.assertNotEqual(cert_file, mh_config.getConfigPath() + "/", "Certificate path empty")
        self.assertTrue(cert_file.endswith('.pem'), f"Expected .pem file, got: {cert_file}")

class TestBasicAuthSecurity(unittest.TestCase):
    def test_basic_auth_requires_https_certificate(self):
        cert_path = mh_config.getCertFile()
        key_path = mh_config.getKeyFile()
        has_https = os.path.isfile(cert_path) and os.path.isfile(key_path)
        try:
            endpoint_user = mh_config.getEndpointUser()
            endpoint_pass = mh_config.getEndpointPass()
        except Exception:
            self.skipTest("Cannot check Basic Auth config")
            return
        uses_basic_auth = bool(endpoint_user or endpoint_pass)
        if uses_basic_auth:
            self.assertTrue(has_https, "Basic Auth requires HTTPS certificate")
        else:
            self.skipTest("No Basic Auth configured")


class TestCredentialStorage(unittest.TestCase):
    def test_config_does_not_store_plaintext_passwords(self):
        import mh_encryption
        import re
        config_path = mh_config.getConfigPath() + "/config.ini"
        if not os.path.isfile(config_path):
            self.skipTest("config.ini not found")
        has_plaintext = False
        with open(config_path, 'r') as f:
            for line in f:
                for field in ['appleid_pass', 'endpoint_pass']:
                    match = re.match(rf'{field}\s*=\s*(.*)$', line.strip())
                    if match:
                        value = match.group(1).strip()
                        if value and not mh_encryption.is_encrypted(value):
                            has_plaintext = True
                            break
        self.assertFalse(has_plaintext, "config.ini contains plaintext passwords")

    def test_auth_json_does_not_store_plaintext_tokens(self):
        import mh_encryption
        auth_path = mh_config.getConfigFile()
        if not os.path.isfile(auth_path):
            self.skipTest("auth.json not found")
        with open(auth_path, 'r') as f:
            content = f.read().strip()
        is_encrypted = mh_encryption.is_encrypted(content)
        if not is_encrypted:
            try:
                data = json.loads(content)
                has_tokens = data.get('dsid') or data.get('searchPartyToken')
                if has_tokens:
                    self.fail("auth.json stores tokens in plaintext")
            except json.JSONDecodeError:
                self.fail("auth.json is neither encrypted nor valid JSON")


def run_tests():
    stream = StringIO()
    runner = unittest.TextTestRunner(stream=stream, verbosity=0)
    suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])
    result = runner.run(suite)
    
    total = result.testsRun
    failed = len(result.failures) + len(result.errors)
    skipped = len(result.skipped)
    passed = total - failed - skipped
    
    test_results = {}
    for f in result.failures + result.errors:
        name = f[0].id().split('.')[-1]
        msg = f[1].strip().split('\n')[-1] if f[1].strip() else "Error"
        test_results[name] = ('FAIL', msg)
    for s in result.skipped:
        test_results[s[0].id().split('.')[-1]] = ('SKIP', s[1])
    
    all_tests = [m for c in [TestHTTPSConfiguration, TestBasicAuthSecurity, TestCredentialStorage]
                  for m in dir(c) if m.startswith('test_')]
    
    print("Security Tests Results:")
    for name in sorted(set(all_tests)):
        if name in test_results:
            status, msg = test_results[name]
            print(f"{status:4} - {name}" + (f": {msg[:50]}" if status == 'FAIL' else ""))
        else:
            print(f"PASS - {name}")
    
    print(f"\nSummary: {passed} passed, {failed} failed, {skipped} skipped")
    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(run_tests())
