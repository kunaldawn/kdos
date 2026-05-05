#!/usr/bin/env python3
import os
import subprocess
import glob
import re

def run_with_logging(cmd, log_file, env=None):
    os.makedirs(os.path.dirname(log_file), exist_ok=True)
    with open(log_file, "w") as f:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            env=env
        )
        for line in process.stdout:
            print(line, end="")
            f.write(line)
        
        return_code = process.wait()
        if return_code != 0:
            raise subprocess.CalledProcessError(return_code, cmd)

def run_script(path, log_dir, use_chroot=False):
    print(f"==> Running: {path}")
    rel_path = os.path.relpath(path, "/workspace")
    log_file = os.path.join(log_dir, f"{os.path.basename(path)}.log")
    
    if use_chroot:
        cmd = ["/workspace/script/chroot_exec.sh", "bash", rel_path]
    else:
        cmd = ["bash", rel_path]
    
    run_with_logging(cmd, log_file)

def main():
    root = "/workspace/script"
    build_root = "/workspace/build"
    log_dir = os.path.join(build_root, "logs")
    
    # Phases to run
    phases = [
        "00_toolchain",
        "01_phase1",
        "02_phase2",
        "03_phase3"
    ]
    
    for phase in phases:
        phase_dir = os.path.join(root, phase)
        if not os.path.isdir(phase_dir):
            continue
            
        print(f"\n--- Phase: {phase} ---")
        phase_log_dir = os.path.join(log_dir, phase)
        
        # Check if phase needs chroot
        parts = phase.split('_', 1)
        phase_name = parts[1] if len(parts) > 1 else phase
        env_file = os.path.join(root, f"{phase_name}.env.sh")
        use_chroot = False
        if os.path.isfile(env_file):
            with open(env_file, 'r') as f:
                if "export CHROOT=1" in f.read():
                    use_chroot = True

        # Special case for packages.txt
        packages_file = os.path.join(phase_dir, "packages.txt")
        if os.path.isfile(packages_file):
            with open(packages_file, 'r') as f:
                pkgs = [line.strip() for line in f if line.strip() and not line.strip().startswith('#')]
            
            if pkgs:
                cmd_prefix = ["/workspace/script/chroot_exec.sh", "bash", "-c"] if use_chroot else ["bash", "-c"]
                env_rel = os.path.relpath(env_file, "/workspace")
                env_src = f"source {env_rel} && " if os.path.isfile(env_file) else ""
                
                print(f"Resolving dependencies for: {' '.join(pkgs)}")
                resolve_cmd = f"{env_src}export PKGDB_DIR=/dev/null && kpkgdepends {' '.join(pkgs)}"
                full_resolve_cmd = cmd_prefix + [resolve_cmd]
                
                try:
                    output = subprocess.check_output(full_resolve_cmd, text=True, stderr=subprocess.STDOUT).strip()
                    resolved_pkgs = output.split()
                    
                    for pkg in resolved_pkgs:
                        print(f"Installing package: {pkg}")
                        log_file = os.path.join(phase_log_dir, f"{pkg}.log")
                        install_cmd = f"{env_src}kpkg install -f {pkg}"
                        run_with_logging(cmd_prefix + [install_cmd], log_file)
                except subprocess.CalledProcessError as e:
                    # check_output above might fail if kpkgdepends fails
                    if hasattr(e, 'output'):
                        print(f"Error: {e.output}")
                    raise
        else:
            scripts = sorted(glob.glob(os.path.join(phase_dir, "*.sh")))
            for s in scripts:
                run_script(s, phase_log_dir, use_chroot)

if __name__ == "__main__":
    main()
