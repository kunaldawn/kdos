#!/usr/bin/env python3
import os
import subprocess
import json
import time
import argparse
from concurrent.futures import ThreadPoolExecutor

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE_NAME = "kdos-base-test"
RESULTS_FILE = os.path.join(REPO_ROOT, "testing", "test_results.json")
LOGS_DIR = os.path.join(REPO_ROOT, "testing", "logs")

def get_ports():
    ports = []
    core_dir = os.path.join(REPO_ROOT, "ports", "core")
    if not os.path.isdir(core_dir):
        return []
    for entry in os.listdir(core_dir):
        if os.path.isdir(os.path.join(core_dir, entry)):
            ports.append(entry)
    return sorted(ports)

def test_package(pkg):
    print(f"[TESTING] {pkg}...")
    log_file = os.path.join(LOGS_DIR, f"{pkg}.log")
    
    # Docker command
    # We mount:
    # - ports -> /ports (ReadOnly)
    # - src -> /src (ReadOnly)
    # Tmpfs for work, packages, and sources (to keep it clean)
    cmd = [
        "docker", "run", "--rm", "--privileged",
        "-v", f"{REPO_ROOT}/ports:/ports:ro",
        "-v", f"{REPO_ROOT}/src:/src:ro",
        "--tmpfs", "/var/cache/kpkg/work:exec,mode=1777",
        "--tmpfs", "/var/cache/kpkg/packages:exec,mode=1777",
        "--tmpfs", "/var/cache/kpkg/sources:exec,mode=1777",
        IMAGE_NAME,
        "kpkg", "install", "-f", pkg
    ]
    
    start_time = time.time()
    try:
        with open(log_file, "w") as lf:
            # We use /usr/bin/python3 -u to avoid buffering in print if needed, 
            # but here we just run kpkg (bash)
            proc = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, text=True)
            exit_code = proc.returncode
    except Exception as e:
        exit_code = -1
        with open(log_file, "a") as lf:
            lf.write(f"\nINTERNAL ERROR: {e}\n")
            
    duration = time.time() - start_time
    status = "PASS" if exit_code == 0 else "FAIL"
    print(f"[{status}] {pkg} ({duration:.1f}s)")
    
    return {
        "package": pkg,
        "status": status,
        "exit_code": exit_code,
        "duration": duration,
        "log_path": os.path.relpath(log_file, REPO_ROOT)
    }

def main():
    parser = argparse.ArgumentParser(description="KDOS Package Test Runner")
    parser.add_argument("--package", help="Test a specific package")
    parser.add_argument("--parallel", type=int, default=1, help="Number of parallel tests")
    args = parser.parse_args()

    os.makedirs(LOGS_DIR, exist_ok=True)
    
    if args.package:
        pkgs = [args.package]
    else:
        pkgs = get_ports()
        
    if not pkgs:
        print("No packages found to test.")
        return

    print(f"Starting test run for {len(pkgs)} packages...")
    results = []
    
    with ThreadPoolExecutor(max_workers=args.parallel) as executor:
        futures = {executor.submit(test_package, pkg): pkg for pkg in pkgs}
        for future in futures:
            res = future.result()
            results.append(res)
            # Update summary after each result
            with open(RESULTS_FILE, "w") as f:
                json.dump(results, f, indent=2)
            
    print(f"\nTesting complete. Results saved to {RESULTS_FILE}")

if __name__ == "__main__":
    main()
