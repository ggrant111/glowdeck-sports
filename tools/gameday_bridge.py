#!/usr/bin/env python3
"""Local, opt-in test bench for live NHL scores and one WLED controller.

Python standard library only. This is NOT a cloud relay and never opens a public
port intentionally. For phone access use --listen 0.0.0.0 on a TRUSTED home LAN.
"""
from __future__ import annotations
import argparse
import copy
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import os
from pathlib import Path
import secrets
import socket
import threading
import time
from urllib.error import URLError, HTTPError
from urllib.request import Request, urlopen
from urllib.parse import urlparse

NHL_URL = "https://api-web.nhle.com/v1/score/now"
TEAM_COLORS = {
    "ANA": "#F47A38", "BOS": "#FFB81C", "BUF": "#003087", "CGY": "#C8102E",
    "CAR": "#CC0000", "CHI": "#CF0A2C", "COL": "#6F263D", "CBJ": "#002654",
    "DAL": "#00843D", "DET": "#CE1126", "EDM": "#FF4C00", "FLA": "#C8102E",
    "LAK": "#A2AAAD", "MIN": "#154734", "MTL": "#AF1E2D", "NSH": "#FFB81C",
    "NJD": "#CE1126", "NYI": "#00539B", "NYR": "#0038A8", "OTT": "#E31837",
    "PHI": "#F74902", "PIT": "#FCB514", "SJS": "#006D75", "SEA": "#99D9D9",
    "STL": "#002F87", "TBL": "#002868", "TOR": "#00205B", "UTA": "#6CACE4",
    "VAN": "#00205B", "VGK": "#B4975A", "WSH": "#C8102E", "WPG": "#041E42",
}
SEG_KEYS = ("id", "on", "bri", "fx", "sx", "ix", "pal", "col", "frz", "cct", "n", "sel", "mi", "rev", "rpt", "grp", "spc", "o1", "o2", "o3", "si", "m12")
WEB = Path(__file__).with_name("bridge_web") / "index.html"

class BridgeError(Exception):
    pass

def compact_nhl(data: dict) -> dict:
    """Only expose fields the test interface needs; never trust remote HTML."""
    games = []
    for g in data.get("games", [])[:20]:
        if not isinstance(g, dict):
            continue
        try:
            game_id = int(g["id"])
            if game_id < 1: continue
            away = str(g.get("awayTeam", {}).get("abbrev", "?")).upper()[:4]
            home = str(g.get("homeTeam", {}).get("abbrev", "?")).upper()[:4]
            if not away.isalpha() or not home.isalpha(): continue
            games.append({
                "id": game_id, "away": away, "home": home,
                "awayScore": max(0, int(g.get("awayTeam", {}).get("score", 0) or 0)),
                "homeScore": max(0, int(g.get("homeTeam", {}).get("score", 0) or 0)),
                "state": str(g.get("gameState", "UNKNOWN"))[:16],
                "period": max(0, int(g.get("period", 0) or 0)),
                "clock": str((g.get("clock") or {}).get("timeRemaining", ""))[:18],
            })
        except (KeyError, TypeError, ValueError):
            continue
    return {"date": str(data.get("currentDate", ""))[:16], "games": games, "fetchedAt": datetime.now(timezone.utc).isoformat(), "source": "NHL", "demo": False}

def restore_payload(state: dict) -> dict:
    segments = state.get("seg") or []
    seg = next((s for s in segments if isinstance(s, dict) and s.get("id") == 0), None)
    if seg is None:
        raise BridgeError("WLED has no segment 0; refusing to modify it")
    keys = {k: copy.deepcopy(seg[k]) for k in SEG_KEYS if k in seg}
    return {"on": bool(state.get("on", False)), "bri": int(state.get("bri", 128)),
            "transition": state.get("transition", 0), "seg": [keys]}

def http_json(url: str, payload=None, timeout=5.0) -> dict:
    body = None if payload is None else json.dumps(payload, separators=(",", ":")).encode("utf-8")
    request = Request(url, data=body, headers={"Accept": "application/json", "User-Agent": "GlowDeckGameDayTestBridge/0.1", **({"Content-Type":"application/json"} if body is not None else {})}, method="POST" if body is not None else "GET")
    with urlopen(request, timeout=timeout) as res:
        if not 200 <= res.status < 300:
            raise BridgeError("Upstream HTTP %d" % res.status)
        data = res.read(512 * 1024 + 1)
        if len(data) > 512 * 1024:
            raise BridgeError("Upstream response too large")
        return json.loads(data.decode("utf-8"))

class Controller:
    def __init__(self, ip: str, journal: Path, fetch=http_json):
        ip_obj = ipaddress.ip_address(ip)
        if ip_obj.version != 4 or ip_obj.is_multicast or ip_obj.is_unspecified or ip_obj.is_loopback or ip_obj.is_link_local:
            raise ValueError("Use your WLED controller's private IPv4 address")
        if not ip_obj.is_private:
            raise ValueError("Refusing non-private/public IP; use a trusted local WLED address")
        self.ip = str(ip_obj)
        self.base = "http://" + self.ip
        self.fetch = fetch
        self.journal = journal
        self.lock = threading.RLock()
        self.timer = None
        self.pending = None
        self.last_error = ""
        self.nhl_cache = None
        self.nhl_at = 0.0

    def wled(self, path, payload=None):
        return self.fetch(self.base + path, payload, timeout=4.0)

    def read_state(self):
        with self.lock:
            data = self.wled("/json/state")
            return {"on": bool(data.get("on")), "bri": data.get("bri"), "seg0": next((s for s in data.get("seg", []) if s.get("id") == 0), None), "previewActive": bool(self.pending), "recoveryPending": self.journal.exists()}

    def get_nhl(self):
        with self.lock:
            if self.nhl_cache is not None and time.monotonic() - self.nhl_at < 25:
                return self.nhl_cache
        raw = self.fetch(NHL_URL, timeout=10.0)
        value = compact_nhl(raw)
        with self.lock:
            self.nhl_cache, self.nhl_at = value, time.monotonic()
        return value

    def _journal(self, payload):
        self.journal.parent.mkdir(parents=True, exist_ok=True)
        temp = self.journal.with_suffix(".tmp")
        with temp.open("w", encoding="utf-8") as f:
            json.dump({"ip": self.ip, "savedAt": datetime.now(timezone.utc).isoformat(), "restore": payload}, f)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(temp, 0o600)
        os.replace(temp, self.journal)

    def restore(self):
        with self.lock:
            if self.timer:
                self.timer.cancel()
                self.timer = None
            if not self.journal.exists():
                self.pending = None
                return {"restored": True, "message": "Nothing to restore"}
            record = json.loads(self.journal.read_text(encoding="utf-8"))
            if record.get("ip") != self.ip:
                raise BridgeError("Recovery file belongs to another device; not touching WLED")
            # Keep journal until a successful response so a process restart can retry.
            self.wled("/json/state", record["restore"])
            self.journal.unlink()
            self.pending = None
            self.last_error = ""
            return {"restored": True, "message": "Previous WLED scene restored"}

    def preview(self, team: str, seconds: int):
        team = team.upper()
        if team not in TEAM_COLORS:
            raise BridgeError("Unsupported NHL team code")
        if not isinstance(seconds, int) or isinstance(seconds, bool) or seconds < 2 or seconds > 10:
            raise BridgeError("Preview duration must be 2–10 seconds")
        with self.lock:
            if self.journal.exists():
                self.restore()
            raw = self.wled("/json/state")
            saved = restore_payload(raw)
            self._journal(saved)  # persist BEFORE any WLED change
            col = TEAM_COLORS[team]
            rgb = [int(col[n:n+2], 16) for n in (1,3,5)]
            try:
                self.wled("/json/state", {"on": True, "bri": 110, "transition": 0,
                     "seg": [{"id": 0, "on": True, "frz": False, "fx": 0, "col": [rgb]}]})
            except Exception:
                try: self.restore()
                except Exception as e: self.last_error = "Preview failed; restore needed: " + str(e)
                raise
            self.pending = {"team": team, "seconds": seconds, "startedAt": datetime.now(timezone.utc).isoformat()}
            self.timer = threading.Timer(seconds, self._restore_timer)
            self.timer.daemon = True
            self.timer.start()
            return {"previewing": True, "team": team, "color": col, "seconds": seconds, "restoresAutomatically": True}

    def _restore_timer(self):
        try: self.restore()
        except Exception as e: self.last_error = "Restore failed; recovery journal preserved: " + str(e)

    def recover_at_startup(self):
        if self.journal.exists():
            try:
                print("Found WLED recovery journal. Attempting immediate restore…", flush=True)
                print(self.restore(), flush=True)
            except Exception as e:
                self.last_error = "Startup restore failed: " + str(e)
                print("WARNING:", self.last_error, "Use Restore when WLED is online.", flush=True)

class APIHandler(BaseHTTPRequestHandler):
    server_version = "GlowDeckLocalTest/0.1"
    def reply(self, status, value):
        data = json.dumps(value, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def authorized(self):
        key = self.headers.get("X-GlowDeck-Token", "")
        if not secrets.compare_digest(key, self.server.token):
            self.reply(403, {"error": "Access code required"})
            return False
        origin = self.headers.get("Origin")
        if origin and urlparse(origin).netloc != self.headers.get("Host"):
            self.reply(403, {"error": "Cross-site requests rejected"})
            return False
        return True

    def do_GET(self):
        if self.path == "/":
            data = WEB.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if not self.path.startswith("/api/"):
            self.reply(404, {"error": "Not found"}); return
        if not self.authorized(): return
        try:
            c = self.server.controller
            if self.path == "/api/nhl/scores": self.reply(200, c.get_nhl())
            elif self.path == "/api/wled/state": self.reply(200, c.read_state())
            elif self.path == "/api/status": self.reply(200, {"wledIp": c.ip, "preview": c.pending, "recoveryPending": c.journal.exists(), "lastError": c.last_error, "mode": "LOCAL TEST ONLY"})
            else: self.reply(404, {"error": "Not found"})
        except (BridgeError, URLError, HTTPError, TimeoutError, ValueError, json.JSONDecodeError) as e:
            self.reply(502, {"error": str(e)[:160]})

    def do_POST(self):
        if not self.path.startswith("/api/"):
            self.reply(404, {"error": "Not found"}); return
        if not self.authorized(): return
        if self.headers.get("Content-Type", "").split(";")[0] != "application/json":
            self.reply(415, {"error": "JSON required"}); return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 2 or length > 512:
                self.reply(413, {"error": "Invalid request size"}); return
            data = json.loads(self.rfile.read(length))
            if not isinstance(data, dict): raise ValueError("Object expected")
            if self.path == "/api/wled/preview":
                result = self.server.controller.preview(data.get("team", ""), data.get("seconds", 5))
            elif self.path == "/api/wled/restore":
                result = self.server.controller.restore()
            else: self.reply(404, {"error": "Not found"}); return
            self.reply(200, result)
        except (BridgeError, URLError, HTTPError, TimeoutError, ValueError, TypeError, json.JSONDecodeError) as e:
            self.reply(400, {"error": str(e)[:160]})

    def log_message(self, fmt, *args):
        print("[%s] %s" % (self.address_string(), fmt % args), flush=True)

def main():
    parser = argparse.ArgumentParser(description="GlowDeck local NHL + real-WLED test bench")
    parser.add_argument("--wled-ip", required=True, help="One private LAN IPv4 address for the WLED to test")
    parser.add_argument("--listen", default="127.0.0.1", help="127.0.0.1 default; use 0.0.0.0 to reach from phone on trusted home Wi-Fi")
    parser.add_argument("--port", type=int, default=8787)
    args = parser.parse_args()
    if args.listen not in ("127.0.0.1", "0.0.0.0"):
        parser.error("Only loopback or all local IPv4 interfaces are supported")
    journal = Path.home()/".glowdeck-gameday"/"bridge-recovery.json"
    controller = Controller(args.wled_ip, journal)
    controller.recover_at_startup()
    server = ThreadingHTTPServer((args.listen, args.port), APIHandler)
    server.controller = controller
    server.token = secrets.token_urlsafe(12)
    print("\nGlowDeck GameDay LOCAL TEST BENCH (no cloud connection)", flush=True)
    print("Open http://127.0.0.1:%d/ on this computer." % args.port, flush=True)
    if args.listen == "0.0.0.0":
        print("For iPhone: find this computer's Wi-Fi IPv4 address and open http://<computer-ip>:%d/" % args.port, flush=True)
        print("Keep this running ONLY on a trusted home network. Never forward port %d." % args.port, flush=True)
    print("ACCESS CODE (enter in browser):", server.token, flush=True)
    print("Configured WLED:", controller.ip, flush=True)
    print("Ctrl+C to stop. Preview restores after <=10 seconds; if interrupted, restart bridge to restore.", flush=True)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("\nStopping bridge; restoring WLED scene if necessary…", flush=True)
    finally:
        server.server_close()
        try: controller.restore()
        except Exception as e: print("WARNING: WLED restore failed; recovery file kept. Restart bridge to retry:", e, flush=True)
if __name__ == "__main__":
    main()
