#!/usr/bin/env python3
"""AT-SPI tree dumps of KDOS surfaces: the goldens, and the accessibility gate.

    atspi.py dump SURFACE [--out FILE] [--webdriver URL]   in a session
                                    (SURFACE: a name, or a path to a .json)
    atspi.py dump SURFACE --xml PAGE-SOURCE.xml            from a saved tree
    atspi.py check DUMP-DIR [--goldens DIR] [--update] [SURFACE...]
    atspi.py selftest

WHAT A DUMP IS. Every KDOS surface — a KCM, a welcome page, the installer —
is read the way Orca reads it: as the accessibility tree its toolkit
publishes over AT-SPI2. The dump is that tree as text, one node per line,
indented by depth, so two runs of the same build print the same bytes and a
change to a surface shows up as a line diff anyone can read:

    # kdos-atspi 1
    # surface: kcm_kdos_boxes
    application "kcmshell6"
      frame "Boxes" [enabled, sensitive]
        button "Apply" [enabled, focusable, sensitive]
        check_box "Allow network" [checkable, enabled, focusable, sensitive]

A line is `<role> <name> [desc=<description>] [<states>]`. Names and
descriptions are JSON strings. The role is AT-SPI's role name with spaces as
underscores, except that "push button" is written `button`: at-spi2-core
names that role either way depending on its version, and a golden must not
change with the bus library. Only the states
that describe the control are kept (checked, editable, expandable, required…);
the ones that describe the moment — focused, active, showing, armed, hovered —
change between two identical runs and are dropped. A node without "visible",
other than the application itself, gets the state `invisible`. A surface file
may MASK names that change by themselves, a clock or a size, which are then
written `"<masked>"`.

WHERE THE TREE COMES FROM. With --webdriver (or $KDOS_ATSPI_WEBDRIVER) the
surface is launched and driven by KDE's selenium-webdriver-at-spi, and its page
source — an XML rendering of the tree — is what is dumped; without one the
tree is read over the bus directly through Atspi's GObject binding, and
written into the same XML first, so both routes print identical dumps. A
surface's `steps` (clicks, typing, waits) run through whichever route is in
use, which is how a page behind a button gets its own golden. The two routes
click differently: the WebDriver's click runs a control's "Press" action
whenever it has one, and a Qt Quick check box answers Press without toggling,
so a step that toggles a check box, a switch or a toggle button behaves only
on the direct route, which runs "Toggle" for those roles. The goldens are
written from the direct route.

THE GATE. `check` compares each dump with its golden in testing/atspi/goldens
and fails on any difference; it also fails any dump in which a visible control
— a button, a check box, an entry, a menu item, a tab, a slider, a list item —
has no accessible name, because Orca reads nothing for it. A dump that holds a
host's error page instead of the surface — kcmshell6's "Could not find
plugin", a QML file that did not compile — fails too, so a surface that did
not load can never become its own golden. A surface may exempt
roles it does not own (`allow_unnamed`), never a control KDOS wrote. --update
writes the goldens, and refuses a dump that fails either check.
"""

import argparse
import difflib
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
SURFACES = os.path.join(HERE, "surfaces")
GOLDENS = os.path.join(HERE, "goldens")
FIXTURES = os.path.join(HERE, "fixtures")
FORMAT = "kdos-atspi 1"

# The states a dump keeps. Everything else AT-SPI reports is either the
# moment (focused, active, showing, armed, pressed, hovered, selectable-text
# carets) or the toolkit's bookkeeping (opaque, stale, defunct, transient).
KEEP_STATES = {
    "checkable", "checked", "collapsed", "editable", "enabled", "expandable",
    "expanded", "focusable", "has popup", "horizontal", "indeterminate",
    "invalid entry", "modal", "multi line", "multiselectable", "read only",
    "required", "selectable", "selected", "sensitive", "single line",
    "vertical",
}

# The controls a person operates. Each must have a name; a label, a panel, a
# filler or a scroll bar is read or skipped by its role alone.
INTERACTIVE = {
    "button", "toggle_button", "check_box", "radio_button", "combo_box",
    "entry", "password_text", "spin_button", "slider", "menu_item",
    "check_menu_item", "radio_menu_item", "page_tab", "link", "switch",
    "list_item", "tree_item",
}

ROLE_ALIASES = {"push_button": "button"}

# What a host prints in place of a surface it could not load: kcmshell6's
# missing plugin, a QML file that failed to compile, plasma-welcome's missing
# page. A tree with one of these is the error page, not the surface, and must
# fail rather than be written as the surface's golden.
LOAD_FAILED = re.compile(r"^(Could not find plugin |Could not find KCM|"
                         r"Error loading QML file|Couldn't load page|"
                         r"Failed to load the KCM)")


def norm_state(s):
    return re.sub(r"[-_]+", " ", s.strip().lower())


def norm_role(tag):
    role = re.sub(r"[\s-]+", "_", tag.strip().lower())
    return ROLE_ALIASES.get(role, role)


# ── surfaces ─────────────────────────────────────────────────────────────

def load_surface(name):
    """A surface file: what to launch, what to do, what to mask.

    {
      "app": "kcmshell6 kcm_kdos_boxes",      argv, split like a shell would,
                                              run with no shell
      "settle": 3,                            seconds after the window appears
      "steps": [{"click": "Network"},         by accessible name
                {"click": {"name": "Apply", "role": "button"}},
                {"type": {"name": "Search", "text": "fire"}},
                {"wait": 1}],
      "mask": [{"role": "label", "name": "^[0-9.]+ [KMG]iB$"}],
      "allow_unnamed": ["list_item"]
    }
    """
    path = os.path.join(SURFACES, name + ".json")
    if name.endswith(".json"):
        # A surface file outside the tree, for writing a new one.
        path = name
        name = os.path.basename(name)[:-5]
    with open(path, encoding="utf-8") as f:
        s = json.load(f)
    if "app" not in s:
        raise SystemExit("%s: no \"app\"" % path)
    s.setdefault("settle", 3)
    s.setdefault("steps", [])
    s.setdefault("mask", [])
    s.setdefault("allow_unnamed", [])
    s["name"] = name
    return s


def surface_names():
    if not os.path.isdir(SURFACES):
        return []
    return sorted(f[:-5] for f in os.listdir(SURFACES) if f.endswith(".json"))


# ── XML (selenium-webdriver-at-spi's page source) → dump ─────────────────

def masked(role, name, masks):
    for m in masks:
        if m.get("role") and norm_role(m["role"]) != role:
            continue
        if re.search(m["name"], name):
            return "<masked>"
    return name


def xml_to_dump(root, surface):
    """The dump of a page-source tree; `root` is an Element or XML text."""
    if isinstance(root, (str, bytes)):
        root = ET.fromstring(root)
    out = ["# " + FORMAT, "# surface: " + surface["name"],
           "# app: " + surface["app"]]

    def visit(e, depth):
        role = norm_role(e.tag)
        name = masked(role, e.get("name") or "", surface["mask"])
        desc = e.get("description") or ""
        raw = {norm_state(s) for s in (e.get("states") or "").split(",") if s.strip()}
        states = sorted(raw & KEEP_STATES)
        # An application is the bus's grouping of its windows and never
        # carries "visible"; only what is drawn can be hidden.
        if "visible" not in raw and role != "application":
            states.append("invisible")
        line = "  " * depth + role + " " + json.dumps(name, ensure_ascii=False)
        if desc:
            line += " desc=" + json.dumps(desc, ensure_ascii=False)
        if states:
            line += " [" + ", ".join(states) + "]"
        out.append(line)
        for c in e:
            visit(c, depth + 1)

    visit(root, 0)
    return "\n".join(out) + "\n"


# ── the accessibility check, on a dump ───────────────────────────────────

LINE_RE = re.compile(r'^( *)(\S+) ("(?:[^"\\]|\\.)*")(?: desc=("(?:[^"\\]|\\.)*"))?(?: \[(.*)\])?$')


def parse_dump(text):
    """(line number, depth, role, name, description, states) per node line."""
    nodes = []
    for n, line in enumerate(text.splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        m = LINE_RE.match(line)
        if not m:
            raise ValueError("line %d is not a dump line: %r" % (n, line))
        indent, role, name, desc, states = m.groups()
        nodes.append((n, len(indent) // 2, role, json.loads(name),
                      json.loads(desc) if desc else "",
                      [s.strip() for s in states.split(",")] if states else []))
    return nodes


def lint(text, allow=()):
    """Every visible control with no name, and every host error page in
    place of the surface, as `line N: ...` strings.

    A node under an invisible ancestor is not on the screen, so it is not
    read, and not checked."""
    bad = []
    hidden_below = None
    for n, depth, role, name, _desc, states in parse_dump(text):
        if hidden_below is not None and depth > hidden_below:
            continue
        hidden_below = None
        if "invisible" in states:
            hidden_below = depth
            continue
        if role in INTERACTIVE and role not in allow and not name.strip():
            bad.append("line %d: %s with no accessible name" % (n, role))
        if LOAD_FAILED.match(name):
            bad.append("line %d: the surface did not load (%s)" % (n, name))
    return bad


# ── route 1: KDE's selenium-webdriver-at-spi ─────────────────────────────

ELEMENT = "element-6066-11e4-a52e-4f735466cecf"


class WebDriver:
    """The W3C WebDriver calls the dumps need, over the standard library.

    selenium-webdriver-at-spi serves them on localhost; the Appium and
    Selenium client libraries its own tests use are not on the image and are
    not needed for six requests."""

    def __init__(self, url):
        self.url = url.rstrip("/")
        self.sid = None

    def _req(self, method, path, body=None, timeout=120):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(self.url + path, data=data, method=method,
                                     headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:
                payload = r.read()
        except urllib.error.HTTPError as e:
            payload = e.read()
            try:
                v = json.loads(payload).get("value", {})
                msg = v.get("message") or v.get("error") or payload[:200]
            except ValueError:
                msg = payload[:200]
            raise RuntimeError("%s %s: %s" % (method, path, msg))
        return json.loads(payload).get("value") if payload else None

    def ready(self):
        try:
            self._req("GET", "/status", timeout=5)
            return True
        except (OSError, RuntimeError, ValueError):
            return False

    def start(self, app, implicit_ms=10000, environ=None):
        caps = {"appium:app": app, "timeouts": {"implicit": implicit_ms}}
        if environ:
            caps["appium:environ"] = environ
        v = self._req("POST", "/session",
                      {"capabilities": {"alwaysMatch": caps}})
        self.sid = v["sessionId"]

    def find(self, name, role=None):
        if role:
            # The server's "class name" strategy matches str(accessible),
            # which pyatspi writes as "[<role> | <name>]".
            spoken = role.replace("_", " ")
            v = self._req("POST", "/session/%s/element" % self.sid,
                          {"using": "class name",
                           "value": "[%s | %s]" % (spoken, name)})
        else:
            v = self._req("POST", "/session/%s/element" % self.sid,
                          {"using": "name", "value": name})
        return v[ELEMENT]

    def click(self, name, role=None):
        self._req("POST", "/session/%s/element/%s/click"
                  % (self.sid, self.find(name, role)), {})

    def type(self, name, text, role=None):
        self._req("POST", "/session/%s/element/%s/value"
                  % (self.sid, self.find(name, role)), {"text": text})

    def source(self):
        return self._req("GET", "/session/%s/source" % self.sid)

    def stop(self):
        if self.sid:
            try:
                self._req("DELETE", "/session/%s" % self.sid, timeout=30)
            except (OSError, RuntimeError):
                pass
            self.sid = None


# ── route 2: the bus, directly ───────────────────────────────────────────

class Direct:
    """The same tree, read with Atspi's own binding, into the same XML.

    The application is started here, with no shell, and with Qt's bridge
    forced on: a Qt program publishes its tree only once something has asked
    the a11y bus for it, and QT_LINUX_ACCESSIBILITY_ALWAYS_ON removes the race
    with that question."""

    def __init__(self):
        import gi
        gi.require_version("Atspi", "2.0")
        from gi.repository import Atspi
        self.Atspi = Atspi
        self.proc = None
        self.app = None

    def start(self, app, timeout=30):
        env = dict(os.environ, QT_ACCESSIBILITY="1",
                   QT_LINUX_ACCESSIBILITY_ALWAYS_ON="1")
        # Its stderr is the dump's: a surface that dies says why there.
        self.proc = subprocess.Popen(shlex.split(app), env=env,
                                     stdout=subprocess.DEVNULL,
                                     start_new_session=True)
        end = time.time() + timeout
        while time.time() < end:
            if self.proc.poll() is not None:
                raise RuntimeError("%s exited with %s before it showed a window"
                                   % (app, self.proc.returncode))
            desk = self.Atspi.get_desktop(0)
            for i in range(desk.get_child_count()):
                a = desk.get_child_at_index(i)
                try:
                    if a is not None and a.get_process_id() == self.proc.pid \
                            and a.get_child_count() > 0:
                        self.app = a
                        return
                except Exception:
                    continue
            time.sleep(0.5)
        raise RuntimeError("%s never appeared on the a11y bus" % app)

    def _walk(self, acc, path):
        Atspi = self.Atspi
        e = ET.Element(acc.get_role_name().replace(" ", "_") or "accessible")
        e.set("name", acc.get_name() or "")
        e.set("description", acc.get_description() or "")
        e.set("path", " ".join(str(i) for i in path))
        names = []
        for st in acc.get_state_set().get_states():
            names.append(norm_state(Atspi.StateType(st).value_nick))
        e.set("states", ", ".join(sorted(names)))
        for i in range(acc.get_child_count()):
            c = acc.get_child_at_index(i)
            if c is not None:
                e.append(self._walk(c, path + [i]))
        return e

    def source(self):
        return self._walk(self.app, [])

    def _find(self, name, role=None, acc=None):
        acc = acc or self.app
        if acc.get_name() == name and (role is None or
                                       norm_role(acc.get_role_name()) == norm_role(role)):
            return acc
        for i in range(acc.get_child_count()):
            c = acc.get_child_at_index(i)
            if c is None:
                continue
            hit = self._find(name, role, c)
            if hit is not None:
                return hit
        return None

    def find(self, name, role=None, timeout=10):
        end = time.time() + timeout
        while True:
            hit = self._find(name, role)
            if hit is not None or time.time() > end:
                break
            time.sleep(0.5)
        if hit is None:
            raise RuntimeError("no %s named %r" % (role or "node", name))
        return hit

    # What a click is, by role: a control that holds a state is toggled, and
    # anything else pressed. A Qt Quick check box lists Press and answers it
    # without changing state.
    TOGGLED = {"check_box", "toggle_button", "radio_button", "switch",
               "check_menu_item", "radio_menu_item"}

    def click(self, name, role=None):
        acc = self.find(name, role)
        action = acc.get_action_iface()
        if action is None or action.get_n_actions() < 1:
            raise RuntimeError("%r has no action to click" % name)
        names = {action.get_action_name(i): i
                 for i in range(action.get_n_actions())}
        if norm_role(acc.get_role_name()) in self.TOGGLED:
            order = ("Toggle", "Press")
        else:
            order = ("Press", "Toggle", "ShowMenu")
        for n in order:
            if n in names:
                action.do_action(names[n])
                return
        action.do_action(0)

    def type(self, name, text, role=None):
        acc = self.find(name, role)
        editable = acc.get_editable_text_iface()
        if editable is None:
            raise RuntimeError("%r is not editable" % name)
        editable.set_text_contents(text)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
                self.proc.wait(timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                try:
                    os.killpg(self.proc.pid, signal.SIGKILL)
                except OSError:
                    pass


def run_steps(route, steps):
    for step in steps:
        ((kind, arg),) = step.items()
        if kind == "wait":
            time.sleep(float(arg))
        elif kind == "click":
            if isinstance(arg, str):
                route.click(arg)
            else:
                route.click(arg["name"], arg.get("role"))
            time.sleep(1)
        elif kind == "type":
            route.type(arg["name"], arg["text"], arg.get("role"))
            time.sleep(1)
        else:
            raise SystemExit("unknown step %r" % kind)


def dump_live(surface, webdriver_url=None):
    if webdriver_url:
        route = WebDriver(webdriver_url)
        if not route.ready():
            raise SystemExit("no WebDriver answering at %s" % webdriver_url)
    else:
        route = Direct()
    try:
        route.start(surface["app"])
        time.sleep(float(surface["settle"]))
        run_steps(route, surface["steps"])
        time.sleep(1)
        tree = route.source()
    finally:
        route.stop()
    return xml_to_dump(tree, surface)


# ── check / update ───────────────────────────────────────────────────────

def check(dumps, goldens, update=False, only=()):
    if not os.path.isdir(dumps):
        print("FAIL  no dump directory %s" % dumps)
        return 1
    names = sorted(f[:-4] for f in os.listdir(dumps) if f.endswith(".txt"))
    if only:
        names = [n for n in names if n in only]
        for n in only:
            if n not in names:
                print("FAIL  %s: no dump in %s" % (n, dumps))
    failed = len([n for n in only if n not in names])
    if not names:
        print("FAIL  no dumps in %s" % dumps)
        return 1
    for n in names:
        text = open(os.path.join(dumps, n + ".txt"), encoding="utf-8").read()
        allow = ()
        if os.path.exists(os.path.join(SURFACES, n + ".json")):
            allow = set(load_surface(n)["allow_unnamed"])
        try:
            bad = lint(text, allow)
        except ValueError as e:
            print("FAIL  %s: %s" % (n, e))
            failed += 1
            continue
        for b in bad:
            print("FAIL  %s: %s" % (n, b))
        gpath = os.path.join(goldens, n + ".txt")
        if update:
            if bad:
                print("FAIL  %s: not written — a golden must pass the check above" % n)
                failed += 1
                continue
            os.makedirs(goldens, exist_ok=True)
            with open(gpath, "w", encoding="utf-8") as f:
                f.write(text)
            print("ok    %s: golden written" % n)
            continue
        # One count per surface, however many ways it failed.
        if not os.path.exists(gpath):
            print("FAIL  %s: no golden (testing/atspi/golden.sh --update %s)" % (n, n))
            failed += 1
            continue
        gold = open(gpath, encoding="utf-8").read()
        if gold == text:
            if bad:
                failed += 1
            else:
                print("ok    %s" % n)
            continue
        failed += 1
        print("FAIL  %s: the tree differs from its golden" % n)
        sys.stdout.writelines(difflib.unified_diff(
            gold.splitlines(True), text.splitlines(True),
            "goldens/%s.txt" % n, "dump/%s.txt" % n))
    print("atspi: %d surface(s), %d failed" % (len(names), failed))
    return 1 if failed else 0


# ── selftest: the format and the gate, from fixtures ─────────────────────

def selftest():
    """The dump format, the lint and the diff, on a saved page source.

    fixtures/page-source.xml is a tree as selenium-webdriver-at-spi serves it;
    fixtures/expected.txt is its dump. The lint must pass it, and must fail the
    same tree with one button's name removed — the check that would otherwise
    be trusted without ever having been seen to fail."""
    import tempfile
    surface = {"name": "fixture", "app": "kcmshell6 kcm_fixture",
               "mask": [{"role": "label", "name": r"^\d+ MiB$"}],
               "allow_unnamed": []}
    src = open(os.path.join(FIXTURES, "page-source.xml"), encoding="utf-8").read()
    want = open(os.path.join(FIXTURES, "expected.txt"), encoding="utf-8").read()
    got = xml_to_dump(src, surface)
    fails = 0

    def expect(cond, what):
        nonlocal fails
        print(("ok    " if cond else "FAIL  ") + what)
        if not cond:
            fails += 1

    expect(got == want, "page source dumps to fixtures/expected.txt")
    if got != want:
        sys.stdout.writelines(difflib.unified_diff(
            want.splitlines(True), got.splitlines(True), "expected", "got"))
    expect(lint(got) == [], "the fixture has no unnamed control")
    root = ET.fromstring(src)
    for e in root.iter():
        if norm_role(e.tag) == "button" and e.get("name") == "Apply":
            e.set("name", "")
            break
    broken = xml_to_dump(root, surface)
    expect(any("button with no accessible name" in b for b in lint(broken)),
           "a button with its name removed fails the check")
    expect(lint(broken, {"button"}) == [], "allow_unnamed exempts a role")
    hidden = ET.fromstring(src)
    for e in hidden.iter():
        if e.get("name") == "Hidden page":
            for c in e.iter():
                if c is not e:
                    c.set("name", "")
    expect(lint(xml_to_dump(hidden, surface)) == [],
           "an unnamed control under an invisible node is not checked")
    failed_load = ET.fromstring(src)
    for e in failed_load.iter():
        if norm_role(e.tag) == "label" and e.get("name"):
            e.set("name", "Could not find plugin kcm_fixture")
            break
    expect(any("did not load" in b for b in lint(xml_to_dump(failed_load, surface))),
           "kcmshell6's missing-plugin page fails the check")
    expect(parse_dump(got)[0][2] == "application", "a dump parses back")
    expect("<masked>" in got, "a masked name is written <masked>")
    with tempfile.TemporaryDirectory() as d:
        dumps, gold = os.path.join(d, "d"), os.path.join(d, "g")
        os.makedirs(dumps)
        with open(os.path.join(dumps, "fixture.txt"), "w") as f:
            f.write(got)
        expect(check(dumps, gold) == 1, "a dump with no golden fails")
        expect(check(dumps, gold, update=True) == 0, "--update writes it")
        expect(check(dumps, gold) == 0, "the same dump then passes")
        with open(os.path.join(dumps, "fixture.txt"), "w") as f:
            f.write(got.replace('"Apply"', '"Apply now"'))
        expect(check(dumps, gold) == 1, "a renamed control fails the diff")
        with open(os.path.join(dumps, "fixture.txt"), "w") as f:
            f.write(broken)
        expect(check(dumps, gold, update=True) == 1,
               "--update refuses a dump with an unnamed control")
    print("atspi selftest: %d failed" % fails)
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("dump", help="dump one surface's tree")
    d.add_argument("surface")
    d.add_argument("--xml", help="dump this saved page source instead of a "
                                 "running surface")
    d.add_argument("--webdriver", default=os.environ.get("KDOS_ATSPI_WEBDRIVER"),
                   help="selenium-webdriver-at-spi's URL, e.g. "
                        "http://127.0.0.1:4723; without it the bus is read "
                        "directly")
    d.add_argument("--out", help="write here instead of stdout")
    c = sub.add_parser("check", help="compare dumps with the goldens")
    c.add_argument("dumps")
    c.add_argument("--goldens", default=GOLDENS)
    c.add_argument("--update", action="store_true",
                   help="write the dumps as the goldens")
    c.add_argument("only", nargs="*")
    sub.add_parser("list", help="the surfaces there are")
    sub.add_parser("selftest", help="the format and the gate, from fixtures")
    a = ap.parse_args()

    if a.cmd == "list":
        for n in surface_names():
            print(n)
        return 0
    if a.cmd == "selftest":
        return selftest()
    if a.cmd == "check":
        return check(a.dumps, a.goldens, a.update, a.only)
    surface = load_surface(a.surface)
    if a.xml:
        text = xml_to_dump(open(a.xml, encoding="utf-8").read(), surface)
    else:
        text = dump_live(surface, a.webdriver)
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
