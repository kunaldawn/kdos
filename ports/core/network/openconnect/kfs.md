## Notes
`hipreport-android.sh` is removed from `/usr/libexec/openconnect` after the install. It is the
Android variant of the GlobalProtect HIP report script, whose first line is `#!/system/bin/sh`,
an interpreter that exists only on Android; `hipreport.sh`, the one for Linux, stays, and
`--csd-wrapper=/usr/libexec/openconnect/hipreport.sh` is the one to name. How the client is
configured, and the script it runs on connect, are in [VPN](../../../../docs/kfs/16-wiki/vpn.md).
