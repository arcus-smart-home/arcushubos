# Arcus Hub OS — System Software Reference

This document covers the system daemons, utilities, radio firmware tools,
and key source files for the Arcus Hub OS.

For hardware details see [HARDWARE.md](HARDWARE.md). For firmware image format
and signing see [FIRMWARE-FORMAT.md](FIRMWARE-FORMAT.md).

---

## System Daemons

| Daemon | Purpose | Platforms |
|--------|---------|-----------|
| `irisinitd` | Main init: button/LED/SSH/provisioning handler | Both |
| `batteryd` / `batterydv3` | Battery and power management | Both |
| `dwatcher` | Daemon watchdog — restarts crashed daemons (release only) | Both |
| `iris4gd` | 4G/LTE modem management | V3 |
| `irisnfcd` | NFC daemon (currently disabled) | V3 |
| `irisagentd` | Agent launcher (starts Java agent) | Both |

`dwatcher` checks every 5 seconds (after 60s initial delay) and restarts:
`irisinitd`, `batteryd`, `iris4gd` (if present), `irisnfcd` (V3, if enabled).

---

## Radio Firmware Flash Tools

All installed to `/usr/bin/` with SUID (4755) so the agent can invoke them:

| Tool | Radio | Recipe |
|------|-------|--------|
| `zwave_flash` | Z-Wave firmware programming | `zwave-utils` |
| `zwave_nvram` | Z-Wave NVRAM access | `zwave-utils` |
| `zwave_default` | Z-Wave factory reset | `zwave-utils` |
| `zigbee_flash` | Zigbee firmware programming | `zigbee-utils` |
| `zigbee_default` | Zigbee factory reset | `zigbee-utils` |
| `zigbee_mfg_tokens` | Zigbee manufacturing tokens | `zigbee-utils` |
| `ble_prog` / `ble_ti_prog` | BLE firmware programming | `ble-utils` |

Platform-specific binaries are selected at build time (e.g., `zwave_fsl_*` for
V3 vs `zwave_ti_*` for V2).

---

## System Utilities

Installed to `/usr/bin/` or `/home/root/bin/`, many with SUID (4755):

| Utility | Purpose |
|---------|---------|
| `fwinstall` | Firmware archive installer (A/B partitions) |
| `update` | Signed firmware updater (downloads, validates, calls fwinstall) |
| `validate_image` | Validates signed firmware headers |
| `hub_restart` | Graceful reboot with LED animation |
| `factory_default` | Factory reset |
| `update_cert` / `update_key` | Certificate and key updates |
| `ledctrl` / `ringctrl` | LED control (V2 discrete / V3 ring) |
| `play_tones` | Buzzer/audio playback |
| `emmcparm` | Micron eMMC flash parameter tool |
| `wifi_scan` | WiFi network scanning (V3 only) |
| `agent_start` / `agent_stop` | Agent lifecycle management |
| `agent_install` / `agent_reinstall` | Agent installation and reset |
| `agent_reset` | Agent data reset |

---

## Key Files

### Machine and Build Configuration

| File | Purpose |
|------|---------|
| `meta-iris/conf/machine/imxdimagic.conf` | V3 machine configuration |
| `meta-iris/conf/distro/poky-iris-ti.conf` | V2 distro config (kernel version, image types) |
| `meta-iris/conf/distro/poky-iris-fsl.conf` | V3 distro config |
| `build-ti/conf/local.conf` | V2 build config (MACHINE=beaglebone-yocto) |
| `build-fsl/conf/local.conf` | V3 build config (MACHINE=imxdimagic) |

### Hardware Init and Daemons

| File | Purpose |
|------|---------|
| `meta-iris/recipes-core/iris-utils/files/irisinit-ti` | V2 hardware init (GPIO, partitions, mfg) |
| `meta-iris/recipes-core/iris-utils/files/irisinit-imxdimagic` | V3 hardware init |
| `meta-iris/recipes-core/iris-utils/files/irisinitd.c` | Main daemon (buttons, LEDs, SSH, debug dongle) |
| `meta-iris/recipes-core/iris-utils/files/batteryd.c` | V2 battery/power management |
| `meta-iris/recipes-core/iris-utils/files/batterydv3.c` | V3 battery/power management |
| `meta-iris/recipes-core/iris-utils/files/dwatcher.c` | Daemon watchdog (release images) |
| `meta-iris/recipes-core/iris-4g/files/iris4gd.c` | 4G/LTE modem daemon |
| `meta-iris/recipes-core/iris-lib/files/irisdefs.h` | Shared hardware constants and paths |

### Kernel Patches and Config

| File | Purpose |
|------|---------|
| `meta-iris/recipes-kernel/linux/linux-6.x/*.cfg` | V2 kernel config fragments |
| `meta-iris/recipes-kernel/linux/linux-fslc-lts/0001-*.patch` | V3 device tree support |
| `meta-iris/recipes-kernel/linux/linux-fslc-lts/0002-*.patch` | V3 custom kernel drivers |
| `meta-iris/recipes-kernel/linux/linux-fslc-lts/0003-*.patch` | V3 sound machine driver |
| `meta-iris/recipes-kernel/linux/linux-fslc-lts/0005-*.patch` | NAU8810 PLL BCLK fix |

### Firmware and Update Tools

| File | Purpose |
|------|---------|
| `meta-iris/recipes-core/iris-utils/files/fwinstall.c` | Firmware installer (A/B partitions) |
| `meta-iris/recipes-core/iris-utils/files/update.c` | Signed firmware updater |
| `meta-iris/recipes-core/iris-utils/files/build_image.c` | Build-time firmware signing tool |
| `meta-iris/recipes-core/iris-utils/files/validate_image.c` | Runtime image validation |
| `meta-iris/recipes-core/iris-lib/files/irisversion.h` | Firmware version definition |
