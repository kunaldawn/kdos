#!/usr/bin/env python3
import curses
import subprocess
import os
import sys
import time
import select
import math

STEPS = [
    ("00_verify.sh", "Asset Verification"),
    ("01_cross_toolchain.sh", "Cross-Toolchain"),
    ("02_target_base.sh", "Target Core/FS"),
    ("03_target_libs.sh", "Target Libraries"),
    ("04_target_tools.sh", "Target Tools"),
    ("05_native_toolchain.sh", "Native Toolchain"),
    ("06_kernel.sh", "Linux Kernel"),
    ("06_bootloaders.sh", "Bootloader"),
    ("07_package.sh", "Packaging (ISO/Initrd)"),
]

SPINNER_CHARS = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"

def format_time(seconds):
    if seconds is None:
        return "--:--"
    m = int(seconds // 60)
    s = int(seconds % 60)
    return f"{m:02d}:{s:02d}"

def draw_box(win, y, x, h, w, title=""):
    """Draw a box with a title"""
    try:
        win.attron(curses.color_pair(7)) # Border color
        # Draw corners
        win.addch(y, x, curses.ACS_ULCORNER)
        win.addch(y, x + w - 1, curses.ACS_URCORNER)
        win.addch(y + h - 1, x, curses.ACS_LLCORNER)
        win.addch(y + h - 1, x + w - 1, curses.ACS_LRCORNER)
        
        # Draw horizontal lines
        for i in range(1, w - 1):
            win.addch(y, x + i, curses.ACS_HLINE)
            win.addch(y + h - 1, x + i, curses.ACS_HLINE)
            
        # Draw vertical lines
        for i in range(1, h - 1):
            win.addch(y + i, x, curses.ACS_VLINE)
            win.addch(y + i, x + w - 1, curses.ACS_VLINE)
            
        # Draw Title
        if title:
            title = f" {title} "
            if len(title) < w - 2:
                win.addstr(y, x + 2, title, curses.A_BOLD | curses.color_pair(8)) # Title color
        
        win.attroff(curses.color_pair(7))
    except curses.error:
        pass

def draw_status(stdscr, current_idx, step_data, start_time, step_start_time, spinner_idx):
    h, w = stdscr.getmaxyx()
    
    # Calculate panel dimensions
    # Header: 3 lines
    # Footer: 1 line
    # Steps: fixed height (len(STEPS) + 4 padding)
    # Logs: Remaining
    
    steps_panel_h = len(STEPS) + 4
    if steps_panel_h > h - 5: # Min logs
        steps_panel_h = h - 5
        
    # Draw Background
    stdscr.bkgd(' ', curses.color_pair(1))
    stdscr.erase()
    
    # 1. HEADER
    header_text = " KDOS BUILD SYSTEM "
    stdscr.attron(curses.color_pair(2) | curses.A_BOLD)
    stdscr.addstr(0, 0, " " * w)
    stdscr.addstr(0, (w - len(header_text)) // 2, header_text)
    stdscr.attroff(curses.color_pair(2) | curses.A_BOLD)
    
    # 2. STEPS PANEL (Top)
    draw_box(stdscr, 1, 0, steps_panel_h, w, "Build Progress")
    
    for i, (script, name) in enumerate(STEPS):
        y_pos = 3 + i
        if y_pos >= steps_panel_h: break
        
        status, duration = step_data[i]
        
        icon = "  "
        attr = curses.A_NORMAL
        
        # Calculate time for this step
        if duration is not None:
             # Finished step
             time_val = duration
        elif status == "RUNNING":
             # Running step
             time_val = time.time() - step_start_time
        else:
             time_val = None

        time_str = format_time(time_val)
        
        if status == "PENDING":
             icon = "○ "
             attr = curses.color_pair(9) # Grey/Dim
             time_str = "--:--"
        elif status == "RUNNING":
             icon = f"{SPINNER_CHARS[spinner_idx]} "
             attr = curses.color_pair(3) | curses.A_BOLD # Yellow/Blue
        elif status == "DONE":
             icon = "✔ " # or ●
             attr = curses.color_pair(4) | curses.A_BOLD # Green
        elif status == "FAIL":
             icon = "✖ "
             attr = curses.color_pair(5) | curses.A_BOLD # Red
            
        # Highlight current row background
        if i == current_idx:
            # Draw highlight bar
            stdscr.attron(curses.color_pair(10))
            stdscr.addstr(y_pos, 2, " " * (w - 4))
            stdscr.attroff(curses.color_pair(10))
            
        # Draw items
        item_str = f"{icon}{name}"
        stdscr.addstr(y_pos, 4, item_str, attr)
        
        # Draw Time (Right aligned in box)
        stdscr.addstr(y_pos, w - 10, time_str, attr)

    # 3. LOGS PANEL (Bottom)
    log_y = steps_panel_h + 1
    log_h = h - log_y - 1
    
    if log_h > 2:
        draw_box(stdscr, log_y, 0, log_h, w, "Live Logs")
        
    # 4. FOOTER
    total_elapsed = time.time() - start_time
    footer_text = f" Total Time: {format_time(total_elapsed)} | Press 'q' to abort "
    stdscr.attron(curses.color_pair(2))
    stdscr.addstr(h - 1, 0, footer_text.ljust(w)[:w-1])
    stdscr.attroff(curses.color_pair(2))
    
    return log_y + 1, log_h - 2

def main(stdscr):
    # Setup Colors
    curses.start_color()
    curses.use_default_colors()
    
    # Pairs: ID, FG, BG
    curses.init_pair(1, curses.COLOR_WHITE, -1)                 # Default: White on Transp
    curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)  # Header/Footer: White on Blue
    curses.init_pair(3, curses.COLOR_CYAN, -1)                  # Running: Cyan
    curses.init_pair(4, curses.COLOR_GREEN, -1)                 # Done: Green
    curses.init_pair(5, curses.COLOR_RED, -1)                   # Fail: Red
    curses.init_pair(6, curses.COLOR_WHITE, -1)                 # Logs: White
    curses.init_pair(7, curses.COLOR_BLUE, -1)                  # Borders: Blue
    curses.init_pair(8, curses.COLOR_YELLOW, -1)                # Box Titles: Yellow
    curses.init_pair(9, curses.COLOR_BLACK, -1)                 # Pending: Black/Grey
    try:
        if curses.can_change_color():
             curses.init_pair(9, 8, -1) # Grey
    except:
        pass

    curses.init_pair(10, -1, curses.COLOR_BLACK)
    curses.curs_set(0)
    stdscr.nodelay(True)
    
    # State
    # List of (Status, Duration)
    # Status: PENDING, RUNNING, DONE, FAIL
    step_data = [["PENDING", None] for _ in STEPS]
    
    log_lines = []
    max_log_lines = 1000
    
    start_time = time.time()
    
    # Logs directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    log_dir = "build/logs"
    os.makedirs(log_dir, exist_ok=True)
    
    spinner_idx = 0
    last_ui_update = 0
    
    # Main Loop
    for i, (script_name, disp_name) in enumerate(STEPS):
        step_data_start = time.time()
        step_data[i][0] = "RUNNING"
        
        # Log File
        log_file_path = os.path.join(log_dir, f"{script_name}.log")
        try:
             log_file = open(log_file_path, "w")
        except:
             log_file = None
             
        cmd = ["bash", os.path.join(script_dir, script_name)]
        
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
        
        while True:
            current_time = time.time()
            
            # Read Output
            # We use select for non-blocking check
            reads = [process.stdout.fileno()]
            ret = select.select(reads, [], [], 0.05) # 50ms timeout for UI responsiveness
            
            if ret[0]:
                line = process.stdout.readline()
                if line:
                    line = line.rstrip()
                    log_lines.append(line)
                    if len(log_lines) > max_log_lines:
                        log_lines.pop(0)
                    if log_file:
                        log_file.write(line + "\n")
                        log_file.flush()
                else:
                    break # EOF
            else:
                if process.poll() is not None:
                    break
            
            # Update UI (limit FPS to ~10-20 to save CPU)
            if current_time - last_ui_update > 0.1:
                spinner_idx = (spinner_idx + 1) % len(SPINNER_CHARS)
                
                # Draw Everything
                log_y, log_h = draw_status(stdscr, i, step_data, start_time, step_data_start, spinner_idx)
                
                # Draw Logs Window (Clipping)
                if log_h > 0:
                    # Create a subwindow or just addstr?
                    # addstr with clipping is easier than managing subwin refresh artifacts
                    # Draw last N lines
                    to_draw = log_lines[-log_h:]
                    for idx, ln in enumerate(to_draw):
                        try:
                            # Truncate line to width - 4
                            max_w = stdscr.getmaxyx()[1] - 4
                            if len(ln) > max_w: ln = ln[:max_w]
                            stdscr.addstr(log_y + idx, 2, ln, curses.color_pair(6))
                        except curses.error:
                            pass
                
                stdscr.refresh()
                last_ui_update = current_time
                
            # Check Input
            k = stdscr.getch()
            if k == ord('q'):
                process.terminate()
                sys.exit(1)
                
        if log_file: log_file.close()
        rc = process.wait()
        step_data[i][1] = time.time() - step_data_start
        
        if rc == 0:
            step_data[i][0] = "DONE"
        else:
            step_data[i][0] = "FAIL"
            # Draw failure state and wait
            draw_status(stdscr, i, step_data, start_time, step_data_start, spinner_idx)
            # Logs are already there
            # Add message
            h, w = stdscr.getmaxyx()
            msg = f" Build FAILED at {disp_name}. Press 'q' to exit. "
            stdscr.attron(curses.color_pair(5) | curses.A_BOLD)
            stdscr.addstr(h//2, (w-len(msg))//2, msg)
            stdscr.attroff(curses.color_pair(5) | curses.A_BOLD)
            stdscr.refresh()
            while True:
                if stdscr.getch() == ord('q'): break
                time.sleep(0.1)
            sys.exit(1)

    # All Done
    h, w = stdscr.getmaxyx()
    draw_status(stdscr, len(STEPS)-1, step_data, start_time, start_time, spinner_idx)
    msg = f" Build Complete Successfully! ({format_time(time.time() - start_time)}) Press any key. "
    stdscr.attron(curses.color_pair(4) | curses.A_BOLD)
    stdscr.addstr(h//2, (w-len(msg))//2, msg)
    stdscr.attroff(curses.color_pair(4) | curses.A_BOLD)
    stdscr.refresh()
    stdscr.nodelay(False)
    stdscr.getch()

if __name__ == "__main__":
    # Ensure correct encoding (important for box drawing chars)
    os.environ.setdefault('ESCDELAY', '25')
    try:
        curses.wrapper(main)
    except KeyboardInterrupt:
        sys.exit(1)
