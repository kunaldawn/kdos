## About

pass is the standard Unix password manager: a shell script that keeps each password in a file
encrypted with [gnupg](#port-gnupg), in a directory that can be a [git](#port-git) repository.
`pass -c` copies a password to the clipboard through [wl-clipboard](#port-wl-clipboard) and puts
the previous clipboard back after 45 seconds; `pass otp` and the QR-code output use
[qrencode](#port-qrencode).

## Notes

`klipper-sensitive.patch` changes the clipboard copy in `src/password-store.sh`. On Wayland the
password is copied with `wl-copy --sensitive`, which offers the
`x-kde-passwordManagerHint=secret` type beside the text; Plasma's Klipper honours it and keeps the
copy out of its history and out of `~/.local/share/klipper`. The timed wipe of Klipper's history
calls `qdbus6`, the Qt 6 name of the D-Bus tool: the tree installs no unsuffixed `qdbus`, and the
script discards the call's errors, so with the upstream name the wipe never runs and nothing says
so. The previous clipboard content is put back without the hint. The patch is to be offered to
password-store upstream (the `password-store` mailing list, which takes patches in place of merge
requests).
