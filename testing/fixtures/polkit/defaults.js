/*
 * The shipped defaults of every polkit action KDOS relies on, names in the
 * rules file, or withholds: [allow_any, allow_inactive, allow_active] as each
 * action's own .policy file sets them, with an element the file omits written
 * as "no", which is how polkitd reads it.
 *
 * The values are those files in the releases this tree builds —
 * NetworkManager 1.58, ModemManager 1.24, udisks2 2.11, fwupd 2.1, bolt
 * 0.9.11, gvfs 1.62, elogind 255.24, accountsservice 26.27, Plasma 6.8, KDE
 * Gear 26.08, KF6 6.31, GParted 1.8, libvirt 12.7, iio-sensor-proxy 3.9,
 * pcsc-lite 2.5.2, Resources 1.10.2 and TLP 1.10.2's tlp-pd.
 *
 * KDOS's OWN ACTIONS ARE NOT HERE. testing/selftest.sh reads them from the
 * .policy files under src/ (timedate1, hostname1, locale1, sysupdate1 and the
 * io.kdos.* actions), so the model is what ships; a row here for an action a
 * KDOS port also ships must agree with that file. Once a tree is built, every
 * row is compared with build/fs/usr/share/polkit-1/actions as well, and a
 * difference fails.
 */
var d = polkit._defaults;

/* NetworkManager */
d["org.freedesktop.NetworkManager.checkpoint-rollback"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.NetworkManager.enable-disable-connectivity-check"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.enable-disable-network"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.enable-disable-statistics"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.enable-disable-wifi"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.enable-disable-wimax"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.enable-disable-wwan"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.network-control"] = ["auth_admin", "yes", "yes"];
d["org.freedesktop.NetworkManager.reload"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.NetworkManager.settings.modify.global-dns"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.NetworkManager.settings.modify.hostname"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.NetworkManager.settings.modify.own"] = ["auth_self_keep", "yes", "yes"];
d["org.freedesktop.NetworkManager.settings.modify.system"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.NetworkManager.sleep-wake"] = ["no", "no", "no"];
d["org.freedesktop.NetworkManager.wifi.scan"] = ["auth_admin", "yes", "yes"];
d["org.freedesktop.NetworkManager.wifi.share.open"] = ["no", "no", "yes"];
d["org.freedesktop.NetworkManager.wifi.share.protected"] = ["no", "no", "yes"];

/* ModemManager */
d["org.freedesktop.ModemManager1.Control"] = ["no", "no", "auth_admin"];
d["org.freedesktop.ModemManager1.Device.Control"] = ["no", "no", "yes"];
d["org.freedesktop.ModemManager1.Location"] = ["no", "no", "yes"];
d["org.freedesktop.ModemManager1.Messaging"] = ["no", "no", "yes"];

/* pcsc-lite */
d["org.debian.pcsc-lite.access_card"] = ["no", "no", "yes"];
d["org.debian.pcsc-lite.access_pcsc"] = ["no", "no", "yes"];

/* udisks2 */
d["org.freedesktop.udisks2.ata-check-power"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.ata-secure-erase"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.ata-smart-enable-disable"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.ata-smart-selftest"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.ata-smart-simulate"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.ata-smart-update"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.ata-standby"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.ata-standby-other-seat"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.ata-standby-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.btrfs.manage-btrfs"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.cancel-job"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.cancel-job-other-user"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.eject-media"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.eject-media-other-seat"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.eject-media-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.encrypted-change-passphrase"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.encrypted-change-passphrase-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.encrypted-lock-others"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.encrypted-unlock"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.encrypted-unlock-crypttab"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.encrypted-unlock-other-seat"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.encrypted-unlock-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.filesystem-fstab"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.filesystem-mount"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.filesystem-mount-other-seat"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.filesystem-mount-other-user"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.filesystem-mount-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.filesystem-take-ownership"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.filesystem-unmount-others"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.loop-delete-others"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.loop-modify-others"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.loop-setup"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.lvm2.manage-lvm"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.manage-md-raid"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.manage-swapspace"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.modify-device"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.modify-device-other-seat"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.modify-device-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.modify-drive-settings"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.modify-system-configuration"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.freedesktop.udisks2.nvme-connect"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.nvme-disconnect"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.nvme-format-namespace"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.nvme-sanitize"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.nvme-set-hostnqn-id"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.nvme-smart-selftest"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.nvme-smart-update"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.open-device"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.open-device-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.power-off-drive"] = ["auth_admin", "auth_admin", "yes"];
d["org.freedesktop.udisks2.power-off-drive-other-seat"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.power-off-drive-system"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.udisks2.read-system-configuration-secrets"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.freedesktop.udisks2.rescan"] = ["auth_admin", "auth_admin", "yes"];

/* fwupd */
d["org.freedesktop.fwupd.clean-remote"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.clear-results"] = ["auth_admin", "no", "yes"];
d["org.freedesktop.fwupd.device-activate"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.device-emulate"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.device-unlock"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.downgrade-hotplug"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.downgrade-hotplug-trusted"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.downgrade-internal"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.downgrade-internal-trusted"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.emulation-load"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.emulation-save"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.emulation-tag"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.enable-remote"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.fix-host-security-attr"] = ["auth_admin", "no", "auth_admin"];
d["org.freedesktop.fwupd.get-bios-settings"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.get-remotes"] = ["auth_admin", "no", "yes"];
d["org.freedesktop.fwupd.modify-config"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.modify-device"] = ["auth_admin", "no", "yes"];
d["org.freedesktop.fwupd.modify-remote"] = ["auth_admin", "no", "auth_admin"];
d["org.freedesktop.fwupd.quit"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.refresh-remote"] = ["auth_admin", "yes", "yes"];
d["org.freedesktop.fwupd.reset-config"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.self-sign"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.set-approved-firmware"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.set-bios-settings"] = ["auth_admin", "no", "auth_admin"];
d["org.freedesktop.fwupd.undo-host-security-attr"] = ["auth_admin", "no", "auth_admin"];
d["org.freedesktop.fwupd.update-hotplug"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.update-hotplug-trusted"] = ["auth_admin", "no", "yes"];
d["org.freedesktop.fwupd.update-internal"] = ["auth_admin", "no", "auth_admin_keep"];
d["org.freedesktop.fwupd.update-internal-trusted"] = ["auth_admin", "no", "yes"];
d["org.freedesktop.fwupd.verify"] = ["auth_admin", "no", "yes"];
d["org.freedesktop.fwupd.verify-update"] = ["auth_admin", "no", "auth_admin_keep"];

/* bolt */
d["org.freedesktop.bolt.authorize"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.bolt.enroll"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.bolt.manage"] = ["auth_admin", "auth_admin", "auth_admin_keep"];

/* gvfs */
d["org.gtk.vfs.file-operations"] = ["no", "no", "auth_admin_keep"];
d["org.gtk.vfs.file-operations-helper"] = ["no", "no", "auth_admin_keep"];

/* elogind's login1 */
d["org.freedesktop.login1.attach-device"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.chvt"] = ["auth_admin_keep", "yes", "yes"];
d["org.freedesktop.login1.flush-devices"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.halt"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.halt-ignore-inhibit"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.halt-multiple-sessions"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.hibernate"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.hibernate-ignore-inhibit"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.hibernate-multiple-sessions"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.inhibit-block-idle"] = ["yes", "yes", "yes"];
d["org.freedesktop.login1.inhibit-block-shutdown"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.inhibit-block-sleep"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.inhibit-delay-shutdown"] = ["yes", "yes", "yes"];
d["org.freedesktop.login1.inhibit-delay-sleep"] = ["yes", "yes", "yes"];
d["org.freedesktop.login1.inhibit-handle-hibernate-key"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.inhibit-handle-lid-switch"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.inhibit-handle-power-key"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.inhibit-handle-reboot-key"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.inhibit-handle-suspend-key"] = ["no", "yes", "yes"];
d["org.freedesktop.login1.lock-sessions"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.manage"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.power-off"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.power-off-ignore-inhibit"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.power-off-multiple-sessions"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.reboot"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.reboot-ignore-inhibit"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.reboot-multiple-sessions"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.reload-config"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.set-reboot-parameter"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.set-reboot-to-boot-loader-entry"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.set-reboot-to-boot-loader-menu"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.set-reboot-to-firmware-setup"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.set-self-linger"] = ["yes", "yes", "yes"];
d["org.freedesktop.login1.set-user-linger"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.set-wall-message"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.suspend"] = ["auth_admin_keep", "auth_admin_keep", "yes"];
d["org.freedesktop.login1.suspend-ignore-inhibit"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.freedesktop.login1.suspend-multiple-sessions"] = ["auth_admin_keep", "auth_admin_keep", "yes"];

/* accountsservice */
d["org.freedesktop.accounts.change-own-password"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.freedesktop.accounts.change-own-user-data"] = ["yes", "yes", "yes"];
d["org.freedesktop.accounts.set-login-option"] = ["auth_admin", "auth_admin", "auth_admin_keep"];
d["org.freedesktop.accounts.user-administration"] = ["auth_admin", "auth_admin", "auth_admin_keep"];

/* tlp-pd's PowerProfiles and UPower */
d["org.freedesktop.UPower.PowerProfiles.configure-battery-aware"] = ["no", "no", "yes"];
d["org.freedesktop.UPower.PowerProfiles.hold-profile"] = ["no", "no", "yes"];
d["org.freedesktop.UPower.PowerProfiles.release-profile"] = ["no", "no", "yes"];
d["org.freedesktop.UPower.PowerProfiles.set-loglevel"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.freedesktop.UPower.PowerProfiles.switch-profile"] = ["no", "no", "yes"];
d["org.freedesktop.UPower.PowerProfiles.sync-profile"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.freedesktop.UPower.enable-charging-limit"] = ["no", "no", "yes"];

/* iio-sensor-proxy */
d["net.hadess.SensorProxy.claim-sensor"] = ["no", "no", "yes"];

/* PowerDevil's KAuth helpers */
d["org.kde.powerdevil.backlighthelper.brightness"] = ["no", "yes", "yes"];
d["org.kde.powerdevil.backlighthelper.brightnessmax"] = ["no", "yes", "yes"];
d["org.kde.powerdevil.backlighthelper.setbrightness"] = ["no", "no", "yes"];
d["org.kde.powerdevil.backlighthelper.syspath"] = ["no", "yes", "yes"];
d["org.kde.powerdevil.chargethresholdhelper.getconservationmode"] = ["no", "yes", "yes"];
d["org.kde.powerdevil.chargethresholdhelper.getthreshold"] = ["no", "yes", "yes"];
d["org.kde.powerdevil.chargethresholdhelper.setconservationmode"] = ["no", "no", "auth_admin_keep"];
d["org.kde.powerdevil.chargethresholdhelper.setthreshold"] = ["no", "no", "auth_admin_keep"];
d["org.kde.powerdevil.discretegpuhelper.hasdualgpu"] = ["no", "no", "yes"];
d["org.kde.powerdevil.wakeupsourcehelper.setwakeupcount"] = ["no", "no", "yes"];

/* sddm-kcm */
d["org.kde.kcontrol.kcmsddm.installtheme"] = ["no", "no", "auth_admin"];
d["org.kde.kcontrol.kcmsddm.reset"] = ["no", "no", "auth_admin"];
d["org.kde.kcontrol.kcmsddm.save"] = ["no", "no", "auth_admin"];
d["org.kde.kcontrol.kcmsddm.sync"] = ["no", "no", "auth_admin"];
d["org.kde.kcontrol.kcmsddm.uninstalltheme"] = ["no", "no", "auth_admin"];

/* kpmcore (Partition Manager, the installer) */
d["org.kde.kpmcore.externalcommand.init"] = ["no", "no", "auth_admin_keep"];

/* kinfocenter */
d["org.kde.kinfocenter.dmidecode.memoryinformation"] = ["no", "no", "yes"];
d["org.kde.kinfocenter.dmidecode.systeminformation"] = ["no", "yes", "yes"];

/* plasma-setup (its own rules file grants these to its user) */
d["org.kde.plasmasetup.createflagfile"] = ["no", "no", "no"];
d["org.kde.plasmasetup.createnewuserautostarthook"] = ["no", "no", "no"];
d["org.kde.plasmasetup.createuser"] = ["no", "no", "no"];
d["org.kde.plasmasetup.removeautologin"] = ["no", "no", "no"];
d["org.kde.plasmasetup.setnewuserdisplayscaling"] = ["no", "no", "no"];
d["org.kde.plasmasetup.setnewuserglobaltheme"] = ["no", "no", "no"];
d["org.kde.plasmasetup.setnewuserhomedirectoryownership"] = ["no", "no", "no"];
d["org.kde.plasmasetup.setnewusertempautologin"] = ["no", "no", "no"];

/* kcron */
d["local.kcron.crontab.save"] = ["no", "no", "auth_admin_keep"];

/* KDE applications' KAuth helpers */
d["org.kde.fontinst.manage"] = ["no", "no", "auth_admin_keep"];
d["org.kde.k3b.addtogroup"] = ["no", "no", "auth_admin"];
d["org.kde.k3b.updatepermissions"] = ["no", "no", "auth_admin"];
d["org.kde.kio.admin.commands"] = ["no", "no", "auth_admin_keep"];
d["org.kde.ksysguard.processlisthelper.applyscheduling"] = ["no", "no", "auth_admin"];
d["org.kde.ksysguard.processlisthelper.changecpuscheduler"] = ["no", "no", "auth_admin"];
d["org.kde.ksysguard.processlisthelper.changeioscheduler"] = ["no", "no", "auth_admin"];
d["org.kde.ksysguard.processlisthelper.renice"] = ["no", "no", "auth_admin"];
d["org.kde.ksysguard.processlisthelper.sendsignal"] = ["no", "no", "auth_admin"];
d["org.kde.ktexteditor6.katetextbuffer.savefile"] = ["no", "no", "auth_admin"];

/* GParted, libvirt, Resources, pkexec */
d["net.nokyan.Resources.kill"] = ["no", "no", "auth_admin_keep"];
d["org.freedesktop.policykit.exec"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.gnome.gparted"] = ["auth_admin", "auth_admin", "auth_admin"];
d["org.libvirt.unix.manage"] = ["auth_admin_keep", "auth_admin_keep", "auth_admin_keep"];
d["org.libvirt.unix.monitor"] = ["yes", "yes", "yes"];

