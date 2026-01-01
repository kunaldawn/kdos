#!/bin/bash
set -e

echo "========================================"
echo "   KDOS Build System                    "
echo "========================================"

# if python exist run tui else fallback to bash only
if command -v python3 &> /dev/null; then
    python3 script/build_tui.py
else
    echo "Python3 not found, falling back to bash only."

    # Dynamic Phase Discovery and Execution
    for dir in script/[0-9][0-9]_*; do
        if [ -d "$dir" ]; then
            dirname=$(basename "$dir")
            # Extract phase name: 03_phase2 -> phase2
            phase_name="${dirname#*_}"
            env_file="script/${phase_name}.env.sh"
            
            # Check for Chroot Requirement
            use_chroot=0
            if [ -f "$env_file" ]; then
                # grep for export CHROOT=1 not commented out
                if grep -q "^export CHROOT=1" "$env_file"; then
                    use_chroot=1
                fi
            fi

            echo ">> Processing ${dirname} (Chroot: ${use_chroot})"
            
            for s in "$dir"/*.sh; do
                [ -e "$s" ] || continue
                echo "   Running $s..."
                if [ "$use_chroot" -eq 1 ]; then
                    script/chroot_exec.sh bash -e "$s"
                else
                    bash -e "$s"
                fi
            done
        fi
    done
fi

echo ">>> Build Complete!"