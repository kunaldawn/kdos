#!/bin/bash
if [ ! -f /var/lib/dbus/machine-id ]; then
	dbus-uuidgen --ensure
fi
