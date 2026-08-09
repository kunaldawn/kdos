#!/bin/bash
# Remove stale kernel modules from previous kernel versions
for d in /lib/modules/*/; do
	ver=$(basename "$d")
	[ "$ver" = "$version" ] && continue
	echo "Removing stale kernel modules: $ver"
	rm -rf "$d"
done

# Regenerate module dependencies
echo "Generating modules.dep for $version..."
if command -v depmod >/dev/null; then
	depmod -a "$version"
else
	echo "Warning: depmod not found, skipping."
fi
