#!/usr/bin/env python3
import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build_test")
BASE_TAR = os.path.join(REPO_ROOT, "testing", "kdos_fs.tar.gz")
IMAGE_NAME = "kdos-base-test"

def run(cmd, cwd=REPO_ROOT):
    print(f"Running: {' '.join(cmd)}")
    subprocess.check_call(cmd, cwd=cwd)

def main():
    print("=== KDOS Base Image Preparation ===")
    
    # 1. Ensure build_test is clean or exists
    os.makedirs(BUILD_DIR, exist_ok=True)
    
    # 2. Build the os-dev image
    run(["docker", "build", "-t", "os-dev", "."])
    
    # 3. Run the mini build up to phase 2
    # We mount testing/mini_build.py as well
    uid = os.getuid()
    gid = os.getgid()
    
    print("\n--- Running Build up to Phase 2 ---")
    run([
        "docker", "run", "--rm", "--privileged",
        "-e", f"HOST_UID={uid}", "-e", f"HOST_GID={gid}",
        "-v", f"{REPO_ROOT}/build_test:/workspace/build",
        "-v", f"{REPO_ROOT}/src:/workspace/src:ro",
        "-v", f"{REPO_ROOT}/fs:/workspace/fs:ro",
        "-v", f"{REPO_ROOT}/script:/workspace/script:ro",
        "-v", f"{REPO_ROOT}/ports:/workspace/ports:ro",
        "-v", f"{REPO_ROOT}/testing:/workspace/testing:ro",
        "os-dev", "python3", "/workspace/testing/mini_build.py"
    ])
    
    # 4. Tar the resulting filesystem
    # The filesystem is in build_test/fs
    fs_root = os.path.join(BUILD_DIR, "fs")
    if not os.path.isdir(fs_root):
        print(f"Error: Filesystem root not found at {fs_root}")
        sys.exit(1)
        
    print(f"\n--- Creating Tarball: {BASE_TAR} ---")
    # We use sudo because some files might be owned by root (from kpkg install)
    run(["sudo", "tar", "-C", fs_root, "-czf", BASE_TAR, "."])
    run(["sudo", "chown", f"{uid}:{gid}", BASE_TAR])
    
    # 5. Create Docker Image
    print(f"\n--- Building Docker Image: {IMAGE_NAME} ---")
    dockerfile_content = f"""FROM scratch
ADD {os.path.basename(BASE_TAR)} /
CMD ["/bin/bash"]
"""
    dockerfile_path = os.path.join(REPO_ROOT, "testing", "Dockerfile.base")
    with open(dockerfile_path, "w") as f:
        f.write(dockerfile_content)
        
    run(["docker", "build", "-t", IMAGE_NAME, "-f", dockerfile_path, "testing"])
    
    print(f"\nSuccess! Base image {IMAGE_NAME} is ready.")

if __name__ == "__main__":
    main()
