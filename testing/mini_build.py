#!/usr/bin/env python3
import os
import subprocess
import glob
import re

def run_script(path, use_chroot=False):
    print(f"==> Running: {path}")
    if use_chroot:
        cmd = ["/workspace/script/chroot_exec.sh", "bash", path]
    else:
        cmd = ["bash", path]
    subprocess.check_call(cmd)

def main():
    root = "/workspace/script"
    # Phases to run
    phases = [
        "00_toolchain",
        "01_phase1",
        "02_phase2"
    ]
    
    for phase in phases:
        phase_dir = os.path.join(root, phase)
        if not os.path.isdir(phase_dir):
            continue
            
        print(f"\n--- Phase: {phase} ---")
        
        # Check if phase needs chroot
        parts = phase.split('_', 1)
        phase_name = parts[1] if len(parts) > 1 else phase
        env_file = os.path.join(root, f"{phase_name}.env.sh")
        use_chroot = False
        if os.path.isfile(env_file):
            with open(env_file, 'r') as f:
                if "export CHROOT=1" in f.read():
                    use_chroot = True

        # Special case for packages.txt in phase 2
        packages_file = os.path.join(phase_dir, "packages.txt")
        if os.path.isfile(packages_file):
            with open(packages_file, 'r') as f:
                pkgs = [line.strip() for line in f if line.strip() and not line.strip().startswith('#')]
            
            if pkgs:
                # Install packages
                env_src = f"source {env_file} && " if os.path.isfile(env_file) else ""
                # We need kpkgdepends and kpkg
                for pkg in pkgs:
                    print(f"Installing package: {pkg}")
                    cmd_str = f"{env_src}kpkg install -f {pkg}"
                    if use_chroot:
                        subprocess.check_call(["/workspace/script/chroot_exec.sh", "bash", "-c", cmd_str])
                    else:
                        subprocess.check_call(["bash", "-c", cmd_str])
        else:
            # Run scripts
            scripts = sorted(glob.glob(os.path.join(phase_dir, "*.sh")))
            for s in scripts:
                run_script(s, use_chroot)

if __name__ == "__main__":
    main()
