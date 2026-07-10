#!/usr/bin/env python3
"""Publish a built firmware image to the update server.

Builds a PlatformIO env (unless --no-build), reads the firmware.json emitted by
tools/gen_manifest.py, and calls the server publication API (see
docs/ota-calibration-protocol.md §8bis, docs/ota-server.md §3):

  A1  PUT  {base}/api/hw/codes/{hw_code}              register/update the HW code
  A2  PUT  {base}/api/hw/codes/{hw_code}/revs/{hw_rev} register/update the revision
  A3  POST {base}/api/firmwares                        upload the .bin + metadata

Publishing is NOT deploying: this only populates the registry + catalog + hosting.

Config (precedence: CLI flag > environment variable > tools/ota.env file):
  --base-url / OTA_API_URL        e.g. http://srv.interne
  --token    / OTA_PUBLISH_TOKEN  bearer token (CI credential, distinct from read API)
Credentials can live in tools/ota.env (gitignored; see tools/ota.env.example).

NOTE: the server API is still being finalised; endpoint paths follow the frozen
contract and may need small adjustments once the server implements them.

Uses only the Python standard library (no pip install).
"""

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request
import uuid


def _load_env_file():
    """Load tools/ota.env (gitignored) into the environment if present.

    Uses setdefault so real environment variables (and thus CLI flags) win over
    file values. Format: KEY=VALUE, one per line, '#' comments allowed.
    """
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ota.env")
    if not os.path.exists(path):
        return
    with open(path) as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            os.environ.setdefault(key.strip(), value.strip().strip('"').strip("'"))


def _build(env):
    print("==> pio run -e %s" % env)
    subprocess.run(["pio", "run", "-e", env], check=True)


def _read_manifest(env):
    build_dir = os.path.join(".pio", "build", env)
    manifest_path = os.path.join(build_dir, "firmware.json")
    bin_path = os.path.join(build_dir, "firmware.bin")
    if not os.path.exists(manifest_path):
        sys.exit("error: %s not found (build first, without --no-build)" % manifest_path)
    with open(manifest_path) as fh:
        manifest = json.load(fh)
    if not os.path.exists(bin_path):
        sys.exit("error: %s not found" % bin_path)
    manifest["_bin"] = bin_path
    return manifest


def _request(method, url, token, json_body=None):
    data = json.dumps(json_body).encode() if json_body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", "Bearer %s" % token)
    if data is not None:
        req.add_header("Content-Type", "application/json")
    return _send(req, method, url)


def _upload(url, token, fields, bin_path):
    boundary = uuid.uuid4().hex
    parts = []
    for key, value in fields.items():
        parts.append(
            ('--%s\r\nContent-Disposition: form-data; name="%s"\r\n\r\n%s\r\n'
             % (boundary, key, value)).encode()
        )
    filename = os.path.basename(bin_path)
    # Server reads the upload from the field named "firmware.bin" (ota/views.py).
    parts.append(
        ('--%s\r\nContent-Disposition: form-data; name="firmware.bin"; filename="%s"\r\n'
         'Content-Type: application/octet-stream\r\n\r\n' % (boundary, filename)).encode()
    )
    with open(bin_path, "rb") as fh:
        parts.append(fh.read())
    parts.append(("\r\n--%s--\r\n" % boundary).encode())
    body = b"".join(parts)

    req = urllib.request.Request(url, data=body, method="POST")
    req.add_header("Authorization", "Bearer %s" % token)
    req.add_header("Content-Type", "multipart/form-data; boundary=%s" % boundary)
    return _send(req, "POST", url)


def _send(req, method, url):
    try:
        with urllib.request.urlopen(req) as resp:
            print("  %s %s -> %s" % (method, url, resp.status))
            return resp.status, resp.read()
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode(errors="replace")
        print("  %s %s -> HTTP %s\n  %s" % (method, url, exc.code, detail), file=sys.stderr)
        if exc.code in (301, 302, 307, 308):
            print("  (server redirected — is OTA_API_URL using https:// ?)", file=sys.stderr)
        elif exc.code == 409:
            print("  (already published — re-run with --overwrite to replace)", file=sys.stderr)
        elif exc.code == 401:
            print("  (check OTA_PUBLISH_TOKEN)", file=sys.stderr)
        sys.exit(1)
    except urllib.error.URLError as exc:
        sys.exit("  %s %s -> connection failed: %s" % (method, url, exc.reason))


def main():
    _load_env_file()  # populate env from tools/ota.env before reading defaults
    ap = argparse.ArgumentParser(description="Publish a firmware image to the update server.")
    ap.add_argument("env", help="PlatformIO environment (e.g. sensor_8266_bmp80)")
    ap.add_argument("--base-url", default=os.environ.get("OTA_API_URL"),
                    help="server base URL (or OTA_API_URL env var)")
    ap.add_argument("--token", default=os.environ.get("OTA_PUBLISH_TOKEN"),
                    help="publication bearer token (or OTA_PUBLISH_TOKEN env var)")
    ap.add_argument("--no-build", action="store_true", help="skip the pio build step")
    ap.add_argument("--overwrite", action="store_true",
                    help="replace an already-published (hw_code, hw_rev, version)")
    args = ap.parse_args()

    if not args.base_url:
        sys.exit("error: --base-url or OTA_API_URL is required")
    if not args.token:
        sys.exit("error: --token or OTA_PUBLISH_TOKEN is required")

    if not args.no_build:
        _build(args.env)
    m = _read_manifest(args.env)

    hw_code = m["hw_code"]
    hw_rev = m["hw_rev"]
    base = args.base_url.rstrip("/")
    print("==> publishing %s rev %s v%s (md5 %s, %d bytes)"
          % (hw_code, hw_rev, m["version"], m["md5"], m["size"]))

    # A1 — hardware code
    _request("PUT", "%s/api/hw/codes/%s" % (base, hw_code), args.token, {
        "platform": m.get("platform", ""),
        "description": "%s: %s" % (m.get("platform", ""), " + ".join(m.get("modules", []))),
        "modules": m.get("modules", []),
    })

    # A2 — hardware revision
    _request("PUT", "%s/api/hw/codes/%s/revs/%s" % (base, hw_code, hw_rev), args.token, {
        "description": "revision %s" % hw_rev,
    })

    # A3 — firmware image
    url = "%s/api/firmwares" % base
    if args.overwrite:
        url += "?overwrite=true"
    _upload(url, args.token, {
        "hw_code": hw_code,
        "hw_rev": str(hw_rev),
        "version": m["version"],
        "md5": m["md5"],
        "size": str(m["size"]),
    }, m["_bin"])

    print("==> published OK (publish != deploy: push to devices separately)")


if __name__ == "__main__":
    main()
