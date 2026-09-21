#!/usr/bin/env python3
"""
Serve a firmware image for OTA testing.

    tools/ota/serve.py [--port 8070] [--dir build] [--file smart_home.bin]

Why not `python -m http.server`: that answers every request with 200 and the
whole body, and does not advertise Accept-Ranges. esp_https_ota's
partial_http_download option requires Range support, so the obvious server is
the one that silently breaks it. This one implements single-range requests.

It also logs what the device actually asks for, which is most of the diagnostic
value when an update fails: you can see whether the device connected at all,
what it requested, and how many bytes it took.
"""

import argparse
import os
import re
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CHUNK = 64 * 1024


class FirmwareHandler(BaseHTTPRequestHandler):
    server_version = "FirmwareServer/1.0"
    root = "."
    forced_file = None

    def log_message(self, fmt, *args):
        sys.stderr.write("  %s  %s\n" % (self.log_date_time_string(), fmt % args))
        sys.stderr.flush()

    def _resolve(self):
        if self.forced_file is not None:
            return os.path.join(self.root, self.forced_file)
        return self.translate_path(self.path)

    def do_HEAD(self):
        path = self._resolve()
        if not os.path.isfile(path):
            self.send_error(404, "Not found")
            return
        size = os.path.getsize(path)
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(size))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()

    def do_GET(self):
        path = self._resolve()
        if not os.path.isfile(path):
            self.log_message("404  %s", self.path)
            self.send_error(404, "Not found")
            return

        size = os.path.getsize(path)
        start, end = 0, size - 1
        partial = False

        rng = self.headers.get("Range")
        if rng:
            m = re.match(r"bytes=(\d*)-(\d*)$", rng.strip())
            if m:
                lo, hi = m.group(1), m.group(2)
                if lo:
                    start = int(lo)
                    if hi:
                        end = int(hi)
                elif hi:                      # suffix range: bytes=-N
                    start = max(0, size - int(hi))
                end = min(end, size - 1)
                if start > end:
                    self.log_message("416  %s  %s", self.path, rng)
                    self.send_error(416, "Range not satisfiable")
                    return
                partial = True

        length = end - start + 1
        self.log_message("%s  %s  %d bytes%s",
                         "206" if partial else "200", self.path, length,
                         "  (%s)" % rng if partial else "")

        self.send_response(206 if partial else 200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(length))
        self.send_header("Accept-Ranges", "bytes")
        if partial:
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, size))
        self.end_headers()

        with open(path, "rb") as f:
            f.seek(start)
            remaining = length
            while remaining > 0:
                chunk = f.read(min(CHUNK, remaining))
                if not chunk:
                    break
                try:
                    self.wfile.write(chunk)
                except (BrokenPipeError, ConnectionResetError):
                    self.log_message("client disconnected after %d bytes",
                                     length - remaining)
                    return
                remaining -= len(chunk)


def main():
    ap = argparse.ArgumentParser(description="Serve a firmware image for OTA testing")
    ap.add_argument("--port", type=int, default=8070)
    ap.add_argument("--dir", default="build")
    ap.add_argument("--file", default="smart_home.bin")
    ap.add_argument("--bind", default="0.0.0.0")
    args = ap.parse_args()

    path = os.path.join(args.dir, args.file)
    if not os.path.isfile(path):
        sys.exit("no firmware at %s -- build it first" % path)

    size = os.path.getsize(path)

    FirmwareHandler.root = args.dir
    FirmwareHandler.forced_file = args.file

    server = ThreadingHTTPServer((args.bind, args.port), FirmwareHandler)

    print("Serving %s" % path)
    print("  size   : %d bytes (%.0f KB)" % (size, size / 1024))
    print("  url    : http://<this-host>:%d/%s" % (args.port, args.file))
    print("  ranges : supported")
    print()
    print("Trigger an update with:")
    print("  mosquitto_pub -h <broker> -t smart_home/devices/<id>/command \\")
    print("    -m '{\"command\":\"ota\",\"url\":\"http://<this-host>:%d/%s\"}'"
          % (args.port, args.file))
    print()
    print("Requests:")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")


if __name__ == "__main__":
    main()
