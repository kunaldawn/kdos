#!/usr/bin/env python3

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

import curses
import sys
import os
import time
import subprocess
import threading
import queue
import re
import select
import glob

# --- Constants & Configuration ---
# Color pairs
CP_BG = 1       # Blue/White (Default)
CP_HL = 2       # Grey/Black (Selection)
CP_RUN = 3      # Yellow/Blue (Running)
CP_DONE = 4     # Green/Blue (Done)
CP_FAIL = 5     # Red/Blue (Fail)
CP_TITLE = 6    # Cyan/Blue (Box/Title)
CP_BAR = 7      # White/Cyan (Progress Bar)
CP_LOG = 8      # White/Black (Log View)
CP_TIME = 9     # Cyan/Blue (for timestamps)
CP_KEY = 10     # Yellow/Blue (for Footer keys)
CP_DONE_REV = 11 # White/Green (Badge for OK)
CP_TRACK = 12   # White/Black (Progress Bar Track)

RE_TITLE = re.compile(r'^#\s*Title:\s*(.+)$', re.IGNORECASE)

class BuildStep:
    def __init__(self, path, is_group=False, level=0):
        self.path = path
        self.is_group = is_group
        self.level = level
        self.children = []
        self.parent = None
        self.status = "PENDING"  # PENDING, RUNNING, DONE, FAIL
        self.logs = []
        self.start_time = None
        self.end_time = None
        self.return_code = 0
        self.title = self._derive_title()
        self.step_type = "HOST" # HOST, CHROOT
        self.custom_cmd = None
        self.custom_cmd = None
        self.packages_file = None
        self.script_dir = None
        self.env_file = None
        self.env_vars = {}
        self.expanded = True 

    def _derive_title(self):
        if not self.is_group:
            try:
                with open(self.path, 'r', encoding='utf-8', errors='ignore') as f:
                    for _ in range(5):
                        line = f.readline()
                        if not line: break
                        m = RE_TITLE.match(line.strip())
                        if m: return m.group(1)
            except: pass
        
        name = os.path.basename(self.path)
        name = os.path.splitext(name)[0]
        name = re.sub(r'^[0-9]+_', '', name)
        return name.replace('_', ' ').replace('-', ' ').title()

    def add_log(self, line):
        self.logs.append(line)
        if len(self.logs) > 2000: 
            self.logs = self.logs[-2000:]

    def duration(self):
        if self.is_group:
            return sum(c.duration() for c in self.children)
            
        if self.start_time is None: return 0
        end = self.end_time if self.end_time else time.time()
        return end - self.start_time

class BuildManager:
    def __init__(self, root_dir):
        self.root_dir = root_dir
        self.roots = []
        self.execution_order = [] # Flattened list of steps to run
        self.current_step = None  # actively running step
        self.is_running = False
        self.stop_requested = False
        self.error_step = None
        self.start_time = None
        
        self._discover()

    def _discover(self, parent_dir=None, parent_node=None, level=0):
        # Helper to add a group of scripts
        def add_group_from_dir(name, dir_path, use_chroot=False, env_file=None):
            if not os.path.isdir(dir_path): return
            
            # Check for packages.txt
            packages_file = os.path.join(dir_path, "packages.txt")
            if os.path.isfile(packages_file):
                # Defer resolution: Just create the group and store metadata
                group = BuildStep(name, is_group=True, level=0)
                group.packages_file = packages_file
                group.env_file = env_file
                group.step_type = "CHROOT" if use_chroot else "HOST" # Context for resolution
                self.roots.append(group)
                self.execution_order.append(group)
                return

            # Create Group Node (Deferred Scripts)
            group = BuildStep(name, is_group=True, level=0)
            group.script_dir = dir_path
            group.step_type = "CHROOT" if use_chroot else "HOST"
            self.roots.append(group)
            self.execution_order.append(group)
            return

        # Dynamic Discovery
        try:
            entries = os.listdir(self.root_dir)
        except OSError:
            return

        sorted_entries = sorted(entries)
        
        for entry in sorted_entries:
            full_path = os.path.join(self.root_dir, entry)
            
            if not os.path.isdir(full_path): continue
            if entry.startswith(".") or entry == "util" or entry == "__pycache__": continue
            
            # Determine Phase Name and Env
            # Example: 03_phase2 -> phase2
            parts = entry.split('_', 1)
            phase_name = parts[1] if len(parts) > 1 else entry
            env_file = os.path.join(self.root_dir, f"{phase_name}.env.sh")
            
            # Check Chroot
            use_chroot = False
            if os.path.isfile(env_file):
                try:
                    with open(env_file, 'r') as f:
                        if "export CHROOT=1" in f.read():
                            use_chroot = True
                except: pass

            # Nice Name
            nice_name = re.sub(r'^[0-9]+_', '', entry)
            nice_name = nice_name.replace('_', ' ').replace('-', ' ').title()
            if use_chroot: nice_name += " (Chroot)"
            
            fullname = f"{nice_name}"
            
            add_group_from_dir(fullname, full_path, use_chroot, env_file)

    def start_build(self):
        self.start_time = time.time()
        self.is_running = True
        t = threading.Thread(target=self._run_loop, daemon=True)
        t.start()
        return t

    def _run_loop(self):
        os.makedirs("build/logs", exist_ok=True)
        
        idx = 0
        while idx < len(self.execution_order):
            if self.stop_requested: break
            
            step = self.execution_order[idx]
            self.current_step = step # Ensure TUI follows even for Groups/Expansion
            
            # Dynamic Expansion (Packages)
            if step.is_group and step.packages_file and not step.children:
                step.status = "RUNNING"
                try:
                    with open(step.packages_file, 'r') as f:
                        pkgs = [line.strip() for line in f if line.strip() and not line.strip().startswith('#')]
                    
                    if pkgs:
                         # Resolve Dependencies
                        wrapper = os.path.abspath("script/chroot_exec.sh")
                        
                        # Use chroot exec if needed
                        cmd_prefix = [wrapper, "bash", "-c"] if step.step_type == "CHROOT" else ["bash", "-c"]
                        
                        # Env setup
                        env_src = f"source {step.env_file} && " if step.env_file else ""
                        
                        # Assuming kpkgdepends is available in PATH (host or chroot)
                        # Force PKGDB_DIR=/dev/null to list all
                        resolve_cmd = f"{env_src}export PKGDB_DIR=/dev/null && kpkgdepends {' '.join(pkgs)}"
                        
                        full_cmd = cmd_prefix + [resolve_cmd]
                        
                        output = subprocess.check_output(full_cmd, text=True, stderr=subprocess.STDOUT).strip()
                        resolved_pkgs = output.split()
                        
                        new_nodes = []
                        for i, pkg in enumerate(resolved_pkgs):
                            # Use parent dir to ensure logs go to build/logs/path/to/script/
                            parent_dir = os.path.dirname(step.packages_file) if step.packages_file else self.root_dir
                            node_name = f"{i:02d}_{pkg}.install"
                            node = BuildStep(os.path.join(parent_dir, node_name), is_group=False, level=1)
                            node.parent = step
                            node.title = pkg.title()
                            node.step_type = "CUSTOM"
                            
                            # Command
                            install_cmd = f"{env_src}kpkg install -f {pkg}"
                            node.custom_cmd = cmd_prefix + [install_cmd]
                            
                            new_nodes.append(node)
                            
                        step.children.extend(new_nodes)
                        self.execution_order[idx+1:idx+1] = new_nodes
                        step.status = "DONE" 
                        
                except Exception as e:
                     step.status = "FAIL"
                     step.add_log(f"Expansion Failed: {e}")
                     self.error_step = step
                     self.stop_requested = True
                     return

                idx += 1
                continue

            # Dynamic Expansion (Scripts)
            if step.is_group and step.script_dir and not step.children:
                step.status = "RUNNING"
                try:
                    files = sorted(glob.glob(os.path.join(step.script_dir, "*.sh")))
                    if files:
                        new_nodes = []
                        for f in files:
                            node = BuildStep(f, is_group=False, level=1)
                            node.parent = step
                            node.step_type = step.step_type # Inherit CHROOT/HOST
                            new_nodes.append(node)
                        
                        step.children.extend(new_nodes)
                        self.execution_order[idx+1:idx+1] = new_nodes
                        step.status = "DONE"
                except Exception as e:
                     step.status = "FAIL"
                     step.add_log(f"Script Expansion Failed: {e}")
                     self.error_step = step
                     self.stop_requested = True
                     return

                idx += 1
                continue

            if step.status == "DONE" or step.is_group:
                idx += 1
                continue

            self._update_family_status(step, "RUNNING")
            step.start_time = time.time()
            
            # Calculate relative path to preserve structure
            rel_path = os.path.relpath(step.path, self.root_dir) 
            log_file_path = os.path.join("build/logs", rel_path + ".log")
            
            # Ensure parent dir exists
            os.makedirs(os.path.dirname(log_file_path), exist_ok=True)
            
            # Default to bash execution on host
            cmd = ["bash", step.path]
            
            if hasattr(step, 'custom_cmd') and step.custom_cmd:
                cmd = step.custom_cmd
            elif step.step_type == "CHROOT":
                # Use absolute path for safety or relative if cwd is correct
                # We assume running from repo root
                wrapper = os.path.abspath("script/chroot_exec.sh")
                cmd = [wrapper, "bash", step.path]
            
            with open(log_file_path, 'w') as lf:
                try:
                    proc = subprocess.Popen(
                        cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, 
                        text=True,
                        bufsize=1
                    )
                    
                    # Robust output reading
                    while True:
                        if self.stop_requested:
                            proc.terminate()
                            break

                        # Check for data
                        reads = [proc.stdout.fileno()]
                        ret = select.select(reads, [], [], 0.05)
                        
                        if ret[0]:
                            line = proc.stdout.readline()
                            if line:
                                clean = line.rstrip()
                                step.add_log(clean)
                                lf.write(clean + "\n")
                            else:
                                # EOF
                                break
                        elif proc.poll() is not None:
                            # Process finished
                            rest = proc.stdout.read()
                            if rest:
                                for l in rest.splitlines():
                                    step.add_log(l)
                                    lf.write(l + "\n")
                            break
                    
                    step.return_code = proc.wait()
                except Exception as e:
                    step.add_log(f"INTERNAL ERROR: {e}")
                    step.return_code = 999
            
            step.end_time = time.time()
            
            if step.return_code == 0:
                self._update_family_status(step, "DONE")
            else:
                self._update_family_status(step, "FAIL")
                self.error_step = step
                self.stop_requested = True
                self.is_running = False
                return

        self.is_running = False

    def _update_family_status(self, step, status):
        step.status = status
        # Bubble up
        curr = step.parent
        while curr:
            has_running = any(c.status == "RUNNING" for c in curr.children)
            has_fail = any(c.status == "FAIL" for c in curr.children)
            all_done = all(c.status == "DONE" for c in curr.children)
            has_started = any(c.status != "PENDING" for c in curr.children)
            
            if has_fail: 
                curr.status = "FAIL"
            elif has_running: 
                curr.status = "RUNNING"
            elif all_done: 
                curr.status = "DONE"
            elif has_started:
                curr.status = "RUNNING"
            else: 
                curr.status = "PENDING"
            
            curr = curr.parent

class TUI:
    def __init__(self, stdscr, manager):
        self.stdscr = stdscr
        self.manager = manager
        self.selected_node = None
        self.visible_nodes = []
        self.scroll_offset = 0
        self.auto_follow = True
        self.h, self.w = 0, 0
        self._init_colors()
        self.stdscr.nodelay(True)
        self.stdscr.keypad(True)
        curses.curs_set(0)

    def _init_colors(self):
        try:
            curses.start_color()
            # Note: We do NOT call use_default_colors() because we want to enforce the Blue theme.
            
            # Theme: Modern Dark
            # Color 1: BG - White on Black
            curses.init_pair(CP_BG, curses.COLOR_WHITE, curses.COLOR_BLACK)
            
            # Color 2: HL - White on Blue (Selection) for better contrast
            curses.init_pair(CP_HL, curses.COLOR_WHITE, curses.COLOR_BLUE)
            
            # Color 3: RUN - Yellow on Black
            curses.init_pair(CP_RUN, curses.COLOR_YELLOW, curses.COLOR_BLACK)
            
            # Color 4: DONE - Green on Black
            curses.init_pair(CP_DONE, curses.COLOR_GREEN, curses.COLOR_BLACK)
            
            # Color 5: FAIL - Red on Black
            curses.init_pair(CP_FAIL, curses.COLOR_RED, curses.COLOR_BLACK)
            
            # Color 6: TITLE - Cyan on Black (for borders/titles)
            curses.init_pair(CP_TITLE, curses.COLOR_CYAN, curses.COLOR_BLACK)
            
            # Color 7: BAR - Blue on Black (Progress Bar Fill)
            curses.init_pair(CP_BAR, curses.COLOR_BLUE, curses.COLOR_BLACK)
            
            # Color 12: CP_TRACK - Grey on Black (for Progress Bar Track)
            curses.init_pair(CP_TRACK, curses.COLOR_WHITE, curses.COLOR_BLACK)
            
            # Color 8: LOG - Grey on Black (Console view)
            curses.init_pair(CP_LOG, curses.COLOR_WHITE, curses.COLOR_BLACK)
            
            # Color 9: TIME - Cyan on Black (for timestamps)
            curses.init_pair(CP_TIME, curses.COLOR_CYAN, curses.COLOR_BLACK)
            
            # Color 10: KEY - Yellow on Black (for Footer keys)
            curses.init_pair(CP_KEY, curses.COLOR_YELLOW, curses.COLOR_BLACK)
            
            # Color 11: DONE_REV - Black on Green (Badge for OK)
            curses.init_pair(CP_DONE_REV, curses.COLOR_BLACK, curses.COLOR_GREEN)

            # Color 13: CP_RUN_SEL - Yellow on Blue (Selected Running Icon)
            curses.init_pair(13, curses.COLOR_YELLOW, curses.COLOR_BLUE)

        except: pass

    def _flatten(self, nodes):
        out = []
        for node in nodes:
            out.append(node)
            if node.is_group and node.expanded:
                out.extend(self._flatten(node.children))
        return out

    def update(self):
        self.h, self.w = self.stdscr.getmaxyx()
        self.visible_nodes = self._flatten(self.manager.roots)
        
        # Auto-Follow
        if self.auto_follow and self.manager.current_step:
            self.selected_node = self.manager.current_step
            
        if not self.selected_node and self.visible_nodes:
            self.selected_node = self.visible_nodes[0]
            
        # Scroll logic
        try:
            sel_idx = self.visible_nodes.index(self.selected_node)
        except ValueError:
            sel_idx = 0
            self.selected_node = self.visible_nodes[0]
            
        list_h = self.h - 4 # Borders
        if sel_idx < self.scroll_offset:
            self.scroll_offset = sel_idx
        elif sel_idx >= self.scroll_offset + list_h:
            self.scroll_offset = sel_idx - list_h + 1

    def draw_screen(self):
        # Force Background Color
        self.stdscr.bkgd(' ', curses.color_pair(CP_BG))
        self.stdscr.erase()
        
        try:
            # Main Border
            self.stdscr.attron(curses.color_pair(CP_TITLE) | curses.A_BOLD)
            self.stdscr.border()
            self.stdscr.attroff(curses.color_pair(CP_TITLE) | curses.A_BOLD)
            
            # Title with consistent background
            title = " KDOS BUILD SYSTEM "
            if len(title) < self.w:
                self.stdscr.attron(curses.color_pair(CP_RUN) | curses.A_BOLD) # Yellow Title
                self.stdscr.addstr(0, (self.w - len(title))//2, title)
                self.stdscr.attroff(curses.color_pair(CP_RUN) | curses.A_BOLD)
            
            # Dynamic Layout Calculation
            # Find max text width in visible nodes
            max_text = 20
            for node in self.visible_nodes:
                # indentation (2 * level) + icon (2 chars) + space (1) + title + space(1) + time (~8 chars)
                # Estimation for time: "(999m59s)" = ~9 chars
                w = (node.level * 2) + 2 + 1 + len(node.title) + 1 + 9
                if w > max_text: max_text = w
            
            # Add padding
            tree_w = max_text + 4
            
            # Clamp width
            min_w = 25
            max_w = int(self.w * 0.45) # Increase max slightly to allow for time
            
            if tree_w < min_w: tree_w = min_w
            if tree_w > max_w: tree_w = max_w
                
            detail_x = tree_w + 1
            detail_w = self.w - tree_w - 2
            
            if detail_w < 5: return # Too small
            
            # Vertical Divider
            self.stdscr.attron(curses.color_pair(CP_TITLE) | curses.A_BOLD)
            for y in range(1, self.h - 1):
                self.stdscr.addch(y, tree_w, curses.ACS_VLINE)
            
            # Join top/bottom
            self.stdscr.addch(0, tree_w, curses.ACS_TTEE)
            self.stdscr.addch(self.h-1, tree_w, curses.ACS_BTEE)
            self.stdscr.attroff(curses.color_pair(CP_TITLE) | curses.A_BOLD)
                
            self._draw_tree(tree_w)
            self._draw_detail(detail_x, detail_w)
            self._draw_footer()
            
        except curses.error: pass
        self.stdscr.refresh()

    def _draw_tree(self, width):
        max_y = self.h - 2
        CP_RUN_SEL = 13
        
        for i in range(max_y):
            idx = self.scroll_offset + i
            if idx >= len(self.visible_nodes): break
            
            node = self.visible_nodes[idx]
            y = 1 + i
            
            # Prepare Components
            indent = "  " * node.level
            
            # Standardize Icon Width to 4
            icon_str = "    "
            icon_attr = curses.color_pair(CP_BG)
            text_attr = curses.color_pair(CP_BG) | curses.A_BOLD # Default Bold White
            
            if node.is_group:
                icon_str = " [-]" if node.expanded else " [+]"
                icon_attr = curses.color_pair(CP_TITLE) | curses.A_BOLD
                text_attr = curses.color_pair(CP_TITLE) | curses.A_BOLD
            else:
                if node.status == "PENDING":
                    icon_str = "    "
                    text_attr = curses.color_pair(CP_BG) # Pending is dim/normal
                elif node.status == "RUNNING":
                    # Dots Spinner (Braille)
                    frames = ["⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"]
                    # frame change every 0.1s
                    idx = int(time.time() * 10) % 8
                    # Ensure width is 4 to match other icons ("    " or " OK ")
                    # Space + char + 2 spaces = 4 chars
                    icon_str = f" {frames[idx]}  "
                    icon_attr = curses.color_pair(CP_RUN) | curses.A_BOLD
                    # User requested Pure White and Bold for the text
                    text_attr = curses.color_pair(CP_BG) | curses.A_BOLD
                elif node.status == "DONE":
                    icon_str = " OK " 
                    # Using badge style: White on Green (Centered)
                    icon_attr = curses.color_pair(CP_DONE_REV) | curses.A_BOLD 
                    text_attr = curses.color_pair(CP_BG) | curses.A_BOLD # White Bold
                elif node.status == "FAIL":
                    icon_str = " !! "
                    icon_attr = curses.color_pair(CP_FAIL) | curses.A_BOLD
                    text_attr = curses.color_pair(CP_FAIL) | curses.A_BOLD
            
            # Time String
            dur = node.duration()
            t_str = ""
            if dur > 0 or node.status == "RUNNING":
                if dur < 60: t_str = f"({int(dur)}s)"
                else: t_str = f"({int(dur)//60}m{int(dur)%60}s)"

            # Construct display logic
            # Format: INDENT + ICON + " " + TITLE + ... + TIME
            
            # Available width for content
            avail_w = width - 1
            
            # Calc lengths
            len_pre = len(indent) + len(icon_str) + 1 # +1 for space after icon
            len_time = len(t_str)
            len_title = len(node.title)
            
            # Check fit
            # We need: len_pre + len_title + 1 (space) + len_time <= avail_w
            # Space for title
            space_for_title = avail_w - len_pre - 1 - len_time
            
            disp_title = node.title
            if space_for_title < len(disp_title):
                # Truncate
                if space_for_title < 3: disp_title = "…" # Should not happen with reasonable width
                else: disp_title = disp_title[:space_for_title-1] + "…"
            
            # Pad length calculation for background fill
            used_len = len_pre + len(disp_title) + len_time
            pad_len = avail_w - used_len
            if pad_len < 0: pad_len = 0 # Safety
            
            # DRAW
            if node == self.selected_node:
                # Selected: Granular drawing for better control
                curr_x = 1
                
                # 1. Indent (Selection Blue)
                self.stdscr.addstr(y, curr_x, indent, curses.color_pair(CP_HL)); curr_x += len(indent)
                
                # 2. Icon 
                if node.status == "RUNNING" and not node.is_group:
                    # Specific style for Running Selected: Yellow on Blue
                    self.stdscr.addstr(y, curr_x, icon_str, curses.color_pair(CP_RUN_SEL) | curses.A_BOLD)
                else:
                    # Inherit selection style (White on Blue)
                    self.stdscr.addstr(y, curr_x, icon_str, curses.color_pair(CP_HL) | curses.A_BOLD)
                curr_x += len(icon_str)
                
                # 3. Spacer
                self.stdscr.addstr(y, curr_x, " ", curses.color_pair(CP_HL)); curr_x += 1
                
                # 4. Text (Force White Bold on Blue)
                self.stdscr.addstr(y, curr_x, disp_title, curses.color_pair(CP_HL) | curses.A_BOLD); curr_x += len(disp_title)
                
                # 5. Padding (Selection Blue)
                self.stdscr.addstr(y, curr_x, " " * pad_len, curses.color_pair(CP_HL)); curr_x += pad_len
                
                # 6. Time (White Bold on Blue, or Cyan on Blue if we had pair)
                # Using White Bold on Blue for visibility
                self.stdscr.addstr(y, curr_x, t_str, curses.color_pair(CP_HL) | curses.A_BOLD)
                
            else:
                # Unselected: Draw components
                result_x = 1
                try:
                    # Indent
                    self.stdscr.addstr(y, result_x, indent, curses.color_pair(CP_BG))
                    result_x += len(indent)
                    
                    # Icon
                    if node.status == "DONE" and not node.is_group:
                         self.stdscr.addstr(y, result_x, icon_str, icon_attr) 
                         # Note: ICON len is 4 (" OK ")
                    else:
                        self.stdscr.addstr(y, result_x, icon_str, icon_attr)
                    result_x += len(icon_str)
                    
                    # Space
                    self.stdscr.addstr(y, result_x, " ", curses.color_pair(CP_BG))
                    result_x += 1
                    
                    # Title
                    self.stdscr.addstr(y, result_x, disp_title, text_attr)
                    result_x += len(disp_title)
                    
                    # Padding (Background)
                    self.stdscr.addstr(y, result_x, " " * pad_len, curses.color_pair(CP_BG))
                    result_x += pad_len
                    
                    # Time (Cyan for distinct visibility)
                    self.stdscr.addstr(y, result_x, t_str, curses.color_pair(CP_TIME) | curses.A_BOLD)
                    
                except curses.error: pass

    def _draw_detail(self, x, w):
        if not self.selected_node: return
        node = self.selected_node
        
        # 1. INFO BOX
        header_attr = curses.color_pair(CP_BG) | curses.A_BOLD
        if node.status == "RUNNING": header_attr = curses.color_pair(CP_RUN) | curses.A_BOLD
        elif node.status == "DONE": header_attr = curses.color_pair(CP_DONE) | curses.A_BOLD
        elif node.status == "FAIL": header_attr = curses.color_pair(CP_FAIL) | curses.A_BOLD
        
        try:
            self.stdscr.addstr(1, x, f" STEP: {node.title} ".ljust(w), header_attr)
            
            status_s = f" STATUS: {node.status} "
            if node.status == "RUNNING":
                 if node.start_time:
                     t_run = time.time() - node.start_time
                     status_s += f"({int(t_run)}s)"
                 else:
                     status_s += "(Group)"
            elif node.duration() > 0:
                status_s += f"({node.duration():.1f}s)"
                
            self.stdscr.addstr(2, x, status_s.ljust(w), curses.color_pair(CP_BG) | curses.A_BOLD)
            
            # Separator Line
            self.stdscr.attron(curses.color_pair(CP_TITLE) | curses.A_BOLD)
            self.stdscr.addch(3, x - 1, curses.ACS_LTEE) # Connect to vline
            for i in range(0, w): 
                self.stdscr.addch(3, x + i, curses.ACS_HLINE)
            # Right side connection?
            # self.stdscr.addch(3, x + w, curses.ACS_RTEE) # If we want to connect to right border
            self.stdscr.attroff(curses.color_pair(CP_TITLE) | curses.A_BOLD)

            # 2. LOG WINDOW
            log_y = 4
            # Extend to bottom of screen (h-1 is border, so h-2 is last content line)
            # Range is 4, 5, ..., h-2
            # Height = (h-2) - 4 + 1 = h - 5
            log_h = self.h - 5
            
            if log_h <= 0: return

            logs = node.logs
            if not logs:
                msg = ""
                if node.is_group: msg = f"Group: {len(node.children)} items."
                elif node.status == "PENDING": msg = "Pending..."
                elif node.status == "RUNNING": msg = "Starting..."
                else: msg = "No logs."
                
                self.stdscr.attron(curses.color_pair(CP_LOG) | curses.A_DIM)
                self.stdscr.addstr(log_y + i, x, msg[:w])
                self.stdscr.attroff(curses.color_pair(CP_LOG) | curses.A_DIM)
                return
                
            visible_logs = logs[-log_h:]
            for i, line in enumerate(visible_logs):
                # Clean and ensure visibility
                clean = line.replace('\t', '    ')
                
                # Use Bold White for visibility
                self.stdscr.addstr(log_y + i, x, clean[:w].ljust(w), curses.color_pair(CP_LOG) | curses.A_BOLD)
                
        except curses.error: pass

    def _draw_footer(self):
        y = self.h - 1
        x = 2
        
        # Calculate Progress
        nodes = self.manager.execution_order
        total = len(nodes)
        done = sum(1 for n in nodes if n.status in ("DONE", "FAIL"))
        pct = done / total if total > 0 else 0
        
        # Helper to draw parts
        cx = x
        self.stdscr.addstr(y, cx, " ", curses.color_pair(CP_BG)); cx+=1
        
        # Keys
        mode = "AUTO" if self.auto_follow else "MANUAL"
        self.stdscr.addstr(y, cx, "[F]", curses.color_pair(CP_KEY) | curses.A_BOLD); cx+=3
        self.stdscr.addstr(y, cx, f":{mode} ", curses.color_pair(CP_BG)); cx += len(f":{mode} ")
        
        self.stdscr.addstr(y, cx, "[ARROWS]", curses.color_pair(CP_KEY) | curses.A_BOLD); cx+=8
        self.stdscr.addstr(y, cx, ":Scroll ", curses.color_pair(CP_BG)); cx += len(":Scroll ")
        
        self.stdscr.addstr(y, cx, "[Q]", curses.color_pair(CP_KEY) | curses.A_BOLD); cx+=3
        self.stdscr.addstr(y, cx, ":Quit  ", curses.color_pair(CP_BG)); cx += len(":Quit  ")
        
        # Time
        elapsed = time.time() - (self.manager.start_time or time.time())
        t_str = f"{int(elapsed)//60}m{int(elapsed)%60}s"
        self.stdscr.addstr(y, cx, f"Time: {t_str} ", curses.color_pair(CP_BG)); cx += len(f"Time: {t_str} ")
        
        # Progress Label
        self.stdscr.addstr(y, cx, "Progress: ", curses.color_pair(CP_BG)); cx += len("Progress: ")
        
        # Progress Info: "50% (5/10) "
        info_txt = f"{int(pct*100)}% ({done}/{total}) "
        self.stdscr.addstr(y, cx, info_txt, curses.color_pair(CP_BG) | curses.A_BOLD); cx += len(info_txt)

        # Draw Smart Progress Bar
        # Layout: [==============      ]
        bar_x = cx
        bar_w = (self.w - 2) - cx
        
        if bar_w < 5: return

        # Draw Brackets
        self.stdscr.addstr(y, bar_x, "[", curses.color_pair(CP_BG) | curses.A_BOLD)
        self.stdscr.addstr(y, bar_x + bar_w - 1, "]", curses.color_pair(CP_BG) | curses.A_BOLD)
        
        inner_x = bar_x + 1
        inner_w = bar_w - 2
        fill_w = int(inner_w * pct)
        
        # Render Bar
        if fill_w > 0:
            # Filled Section (Green)
            # Use DONE_REV (White on Green) for filled area (using space char)
            self.stdscr.attron(curses.color_pair(CP_DONE_REV))
            self.stdscr.addstr(y, inner_x, " " * fill_w)
            self.stdscr.attroff(curses.color_pair(CP_DONE_REV))
            
        remaining = inner_w - fill_w
        if remaining > 0:
            # Empty Section (Black)
            # Use TRACK (White on Black) with CKBOARD
            self.stdscr.attron(curses.color_pair(CP_TRACK))
            for i in range(remaining):
                self.stdscr.addch(y, inner_x + fill_w + i, curses.ACS_CKBOARD)
            self.stdscr.attroff(curses.color_pair(CP_TRACK))

    def input(self):
        k = self.stdscr.getch()
        if k == -1: return

        if k == ord('q') or k == ord('Q'):
            self.manager.stop_requested = True
        elif k == ord('f') or k == ord('F'):
            self.auto_follow = True
        elif k == curses.KEY_UP:
            self.auto_follow = False
            try:
                idx = self.visible_nodes.index(self.selected_node)
                if idx > 0: self.selected_node = self.visible_nodes[idx-1]
            except: pass
        elif k == curses.KEY_DOWN:
            self.auto_follow = False
            try:
                idx = self.visible_nodes.index(self.selected_node)
                if idx < len(self.visible_nodes) - 1: self.selected_node = self.visible_nodes[idx+1]
            except: pass
        elif k == curses.KEY_PPAGE: # Page Up
            self.auto_follow = False
            try:
                idx = self.visible_nodes.index(self.selected_node)
                new_idx = max(0, idx - 10)
                self.selected_node = self.visible_nodes[new_idx]
            except: pass
        elif k == curses.KEY_NPAGE: # Page Down
            self.auto_follow = False
            try:
                idx = self.visible_nodes.index(self.selected_node)
                new_idx = min(len(self.visible_nodes)-1, idx + 10)
                self.selected_node = self.visible_nodes[new_idx]
            except: pass

def main(stdscr):
    manager = BuildManager("script")
    tui = TUI(stdscr, manager)
    
    manager.start_build()
    
    try:
        while True:
            # Update & Draw
            tui.update()
            tui.draw_screen()
            
            # Input
            tui.input()
            
            # Check exit
            if not manager.is_running:
                if manager.stop_requested or manager.error_step:
                    # If error, stay open until Q
                    if manager.error_step:
                        tui.auto_follow = False
                        tui.selected_node = manager.error_step
                        # Wait for Q
                        continue 
                    break
                else:
                    # Done successfully
                    msg = " BUILD COMPLETE - PRESS Q TO EXIT "
                    h, w = stdscr.getmaxyx()
                    stdscr.attron(curses.color_pair(CP_DONE) | curses.A_BOLD)
                    stdscr.addstr(h//2, (w-len(msg))//2, msg)
                    stdscr.refresh()
                    while stdscr.getch() not in (ord('q'), ord('Q')): pass
                    break
            
            curses.napms(30)
            
    except KeyboardInterrupt:
        manager.stop_requested = True

if __name__ == "__main__":
    os.environ.setdefault('ESCDELAY', '25')
    curses.wrapper(main)
