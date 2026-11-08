## About

ksystemstats is the daemon behind Plasma's system monitor widgets and System Monitor. It owns
`org.kde.ksystemstats1` on the session bus and serves sensors from plugins: CPU, memory, disks,
network, power, pressure stall information, GPUs and the hardware sensors of
[lm-sensors](#port-lm-sensors). The sensor model it serves is [libksysguard](#port-libksysguard)'s.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_UDev=ON` — Make libudev a requirement. Upstream marks it only
  recommended and builds the GPU plugin only when it is found; without it there are no GPU
  sensors. The library comes from [eudev](#port-eudev).
- `-D CMAKE_DISABLE_FIND_PACKAGE_Libcap=ON` — No `setcap` step at install. Found, libcap makes
  the install run `setcap CAP_PERFMON=+ep` on `ksystemstats_intel_helper` and
  `ksystemstats_xe_helper`; kpkg's package archive does not keep file capabilities, so the step
  would set a capability no installed copy carries.

## Notes

The Intel GPU helpers read the kernel's performance counters for the engine-busy sensors. Without
`CAP_PERFMON` they can do so only where `kernel.perf_event_paranoid` allows an unprivileged user,
so on a default kernel the Intel and Xe GPU usage sensors read nothing while temperature, memory
and frequency still work. The Xe helper is built when the kernel headers provide
`drm/xe_drm.h`.

Network sensors come from [networkmanager-qt](#port-networkmanager-qt) (`WITH_NM`, on by default
on Linux), and socket counts from [libnl](#port-libnl)'s `sock_diag`.

The daemon is started by D-Bus activation from
`/usr/share/dbus-1/services/org.kde.ksystemstats1.service`, whose `Exec=` line names
`/usr/bin/ksystemstats`. The install also puts a systemd user unit, `plasma-ksystemstats.service`,
under `/usr/lib/systemd`; `build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
The activation file's `SystemdService=` line is then unused and the bus runs the `Exec=` line.
