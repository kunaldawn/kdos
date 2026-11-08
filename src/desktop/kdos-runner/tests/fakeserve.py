#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   fakeserve.py PORTFILE — kiwix-serve, Kolibri and llama-server, as far as
#   the runner asks
#
# Three servers on 127.0.0.1, on ports the kernel picks, written to PORTFILE
# as "KIWIX KOLIBRI LLAMA" once all listen. Each answers the endpoints the
# runner uses, in the shape libkiwix 14, Kolibri 0.19 and llama.cpp's router
# write them, and records every request line in PORTFILE.log so a test can see
# what was asked:
#
#   kiwix   /suggest?content=<book>&term=<t>&count=N   only for the two books
#           tree.sh's library file names, routed the way libkiwix routes them;
#           values carry HTML entities, and the last item is the full-text
#           "pattern" suggestion that is not a row
#   kolibri /api/content/contentnode_search/?search=<t>&max_results=N
#           one id that is not hex, which must not become a URL; the term
#           `slowly` answers after two seconds, past any deadline
#   llama   /v1/models                 two models, `small` loaded
#           /v1/chat/completions       refuses any model but `small` as the
#           router does; logs the model and the user's question; the
#           question `slowly` answers after three seconds, `broken` is a 500
#           with llama-server's error body, `silent` an empty answer, and any
#           other is a reasoning model's <think> block and an answer
# ---------------------------------
import http.server
import json
import socketserver
import sys
import threading
import time
import urllib.parse

PORTFILE = sys.argv[1]
BOOKS = {"wikipedia_en_all_maxi_2024-01", "wiktionaryplusfr"}
LOCK = threading.Lock()


def log(line):
    with LOCK, open(PORTFILE + ".log", "a") as f:
        f.write(line + "\n")


class Kiwix(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        log("kiwix " + self.path)
        u = urllib.parse.urlsplit(self.path)
        q = urllib.parse.parse_qs(u.query)
        book = q.get("content", [""])[0]
        term = q.get("term", [""])[0]
        if u.path != "/suggest" or book not in BOOKS:
            self.send_response(404)
            self.end_headers()
            return
        items = [
            {"value": "Photosynthesis &amp; light", "label": "<b>Photo</b>synthesis",
             "kind": "path", "path": "A/Photosynthesis"},
            {"value": term + " in " + book, "label": "x", "kind": "path",
             "path": "A/Odd path/é?#"},
            {"value": term + " ", "label": "containing '" + term + "'...",
             "kind": "pattern"},
        ]
        body = json.dumps(items).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.end_headers()
        self.wfile.write(body)


class Kolibri(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        log("kolibri " + self.path)
        u = urllib.parse.urlsplit(self.path)
        q = urllib.parse.parse_qs(u.query)
        term = q.get("search", [""])[0]
        if u.path != "/api/content/contentnode_search/":
            self.send_response(404)
            self.end_headers()
            return
        if term == "slowly":
            time.sleep(2)
        body = json.dumps({
            "channel_ids": [], "content_kinds": [], "total_results": 3,
            "results": [
                {"id": "a3f1c2", "title": "Photosynthesis basics", "kind": "video"},
                {"id": "b7e9", "title": "Plants", "kind": "topic"},
                {"id": "../../etc", "title": "Not an id", "kind": "video"},
            ]}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(body)


class Llama(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def reply(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        log("llama GET " + self.path)
        if self.path != "/v1/models":
            self.reply(404, {"error": {"code": 404, "message": "File Not Found"}})
            return
        self.reply(200, {"object": "list", "data": [
            {"id": "tiny", "object": "model", "status": {"value": "unloaded"}},
            {"id": "small", "object": "model", "status": {"value": "loaded"}},
        ]})

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        req = json.loads(self.rfile.read(n) or b"{}")
        q = [m["content"] for m in req.get("messages", []) if m.get("role") == "user"]
        q = q[0] if q else ""
        log("llama POST %s model=%s stream=%s q=%s"
            % (self.path, req.get("model"), req.get("stream"), q))
        if req.get("model") != "small":
            self.reply(400, {"error": {"code": 400, "type": "invalid_request_error",
                                       "message": "model '%s' not found" % req.get("model")}})
            return
        if q == "slowly":
            time.sleep(3)
        if q == "broken":
            self.reply(500, {"error": {"code": 500, "type": "server_error",
                                       "message": "the model failed to load"}})
            return
        text = "" if q == "silent" else \
            "<think>\nhm\n</think>\n\nThe answer to “%s” is 42.\n" % q
        self.reply(200, {"object": "chat.completion", "model": "small", "choices": [
            {"index": 0, "finish_reason": "stop",
             "message": {"role": "assistant", "content": text}}]})


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True

    # A client that gives up on a slow answer closes its socket, which is
    # what the deadline and timeout tests do; the write that follows fails,
    # and its traceback would bury the test's own lines.
    def handle_error(self, request, client_address):
        pass


def main():
    a = Server(("127.0.0.1", 0), Kiwix)
    b = Server(("127.0.0.1", 0), Kolibri)
    c = Server(("127.0.0.1", 0), Llama)
    for s in (a, b, c):
        threading.Thread(target=s.serve_forever, daemon=True).start()
    with open(PORTFILE + ".tmp", "w") as f:
        f.write("%d %d %d\n" % (a.server_address[1], b.server_address[1],
                                 c.server_address[1]))
    import os
    os.rename(PORTFILE + ".tmp", PORTFILE)
    while True:
        time.sleep(60)


main()
