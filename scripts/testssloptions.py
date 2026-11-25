#!/usr/bin/env python3
#
# GNU TLS SSLOptions integration tests for libcups.
#
# Copyright © 2026 by OpenPrinting.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#
# Usage (after building with --with-tls=gnutls):
#
#   python3 scripts/testssloptions.py
#
# Requires Python 3 with TLS 1.3 support and the openssl command.  The test
# client is built with the compiler and flags from Makedefs and linked against
# the static libcups.  All certificates, configuration, and connections are
# local to this test.
#
# Must be run as a normal user - libcups ignores CUPS_SYSCONFIG and
# CUPS_USERCONFIG when running as root.
#

import os
import re
import shlex
import socket
import ssl
import subprocess
import tempfile
import threading
from pathlib import Path

root = Path(__file__).resolve().parent.parent

if "#define HAVE_GNUTLS 1" not in (root / "config.h").read_text():
    raise SystemExit("Configure libcups with --with-tls=gnutls before running this test.")

if os.getuid() == 0:
    raise SystemExit("Run this test as a normal user, not root.")

# Get the compiler and flags from Makedefs...
makedefs = {}
for line in (root / "Makedefs").read_text().splitlines():
    if m := re.match(r"^([A-Z_]+)\s*=\s*(.*)$", line):
        makedefs[m.group(1)] = m.group(2)

failures = 0

with tempfile.TemporaryDirectory(prefix="libcups-ssloptions-") as d:
    p = Path(d)
    (p / "client.c").write_text(r"""
#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
  char    security[1024];	// Negotiated TLS settings
  http_t  *http;		// Connection to the test server

  if (argc != 2)
    return (2);

  http = httpConnect("localhost", atoi(argv[1]), NULL, AF_INET,
                     HTTP_ENCRYPTION_ALWAYS, true, 2000, NULL);
  if (!http)
  {
    fprintf(stderr, "%s\n", cupsGetErrorString());
    return (1);
  }

  puts(httpGetSecurity(http, security, sizeof(security)));
  httpClose(http);
  return (0);
}
""")
    subprocess.run(
        shlex.split(os.environ.get("CC", makedefs.get("CC", "cc")))
        + shlex.split(makedefs.get("OPTIM", ""))
        + shlex.split(makedefs.get("LDFLAGS", ""))
        + [
            "-I" + str(root),
            "-o",
            str(p / "client"),
            str(p / "client.c"),
            str(root / "cups" / "libcups3.a"),
        ]
        + shlex.split(makedefs.get("LIBS", "")),
        check=True,
    )
    subprocess.run(
        [
            os.environ.get("OPENSSL", "openssl"),
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-keyout",
            str(p / "key.pem"),
            "-out",
            str(p / "cert.pem"),
            "-days",
            "1",
            "-subj",
            "/CN=localhost",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    # Each case is (name, GNU TLS system config, SSLOptions, server minimum
    # TLS version, server maximum TLS version, client should connect,
    # expected text in output or cipher suite to force on the server)
    cases = [
        (
            "missing SYSTEM honors MaxTLS1.2",
            "",
            "MinTLS1.2 MaxTLS1.2",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_3,
            True,
            "TLS/1.2",
        ),
        (
            "missing SYSTEM rejects TLS1.3-only server",
            "",
            "MinTLS1.2 MaxTLS1.2",
            ssl.TLSVersion.TLSv1_3,
            ssl.TLSVersion.TLSv1_3,
            False,
            None,
        ),
        (
            "missing SYSTEM honors MinTLS1.3",
            "",
            "MinTLS1.3",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_2,
            False,
            None,
        ),
        (
            "NoSystem remains functional",
            "",
            "NoSystem MinTLS1.2 MaxTLS1.2",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_3,
            True,
            "TLS/1.2",
        ),
        (
            "configured SYSTEM remains functional",
            "[priorities]\nSYSTEM = NORMAL\n",
            "MinTLS1.2 MaxTLS1.2",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_3,
            True,
            "TLS/1.2",
        ),
        (
            "configured SYSTEM keeps cipher restriction",
            "[priorities]\nSYSTEM = NORMAL:-AES-128-GCM\n",
            "MinTLS1.2 MaxTLS1.2",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_2,
            False,
            "GCM",
        ),
        (
            "NoSystem bypasses named cipher restriction",
            "[priorities]\nSYSTEM = NORMAL:-AES-128-GCM\n",
            "NoSystem MinTLS1.2 MaxTLS1.2",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_2,
            True,
            "GCM",
        ),
        (
            "missing SYSTEM honors DenyCBC",
            "",
            "MinTLS1.2 MaxTLS1.2 DenyCBC",
            ssl.TLSVersion.TLSv1_2,
            ssl.TLSVersion.TLSv1_2,
            False,
            "CBC",
        ),
    ]

    for name, policy, options, minimum, maximum, success, expected in cases:
        (p / "gnutls.conf").write_text(policy)
        (p / "client.conf").write_text("SSLOptions " + options + "\n")

        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = minimum
        ctx.maximum_version = maximum
        ctx.load_cert_chain(p / "cert.pem", p / "key.pem")
        if expected == "CBC":
            ctx.set_ciphers("ECDHE-RSA-AES128-SHA256")
        if expected == "GCM":
            ctx.set_ciphers("ECDHE-RSA-AES128-GCM-SHA256")

        listener = socket.socket()
        listener.bind(("127.0.0.1", 0))
        listener.listen()
        listener.settimeout(5)
        port = listener.getsockname()[1]

        def serve(listener, ctx):
            try:
                conn, _ = listener.accept()
                with conn:
                    conn.settimeout(4)
                    with ctx.wrap_socket(conn, server_side=True) as tls:
                        tls.recv(1)
            except (ssl.SSLError, OSError):
                pass
            finally:
                listener.close()

        thread = threading.Thread(target=serve, args=(listener, ctx))
        thread.start()

        env = dict(
            os.environ,
            GNUTLS_SYSTEM_PRIORITY_FILE=str(p / "gnutls.conf"),
            CUPS_SYSCONFIG=d,
            CUPS_USERCONFIG=d,
        )
        r = subprocess.run(
            [str(p / "client"), str(port)],
            env=env,
            check=False,
            capture_output=True,
            text=True,
            timeout=8,
        )
        thread.join(6)

        if success:
            passed = r.returncode == 0 and expected in r.stdout
        else:
            passed = r.returncode == 1 and "TLS" in r.stderr
        failures += not passed

        print(
            ("PASS" if passed else "FAIL"),
            name,
            "=>",
            (r.stdout + r.stderr).strip(),
            flush=True,
        )

print("Failures:", failures)
raise SystemExit(bool(failures))
