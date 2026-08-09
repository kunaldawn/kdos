#!/bin/bash
if [ -d /usr/share/glib-2.0/schemas ]; then
	echo "Compiling GSettings schemas..."
	glib-compile-schemas /usr/share/glib-2.0/schemas
else
	echo "Warning: /usr/share/glib-2.0/schemas not found, skipping compilation."
fi
