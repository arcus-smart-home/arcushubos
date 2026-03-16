# Yocto Scarthgap 5.0 Upgrade

This document covers the upgrade of Arcus Hub OS from Yocto Dunfell 3.1
(kernel 5.4) to Yocto Scarthgap 5.0 LTS (kernel 6.6), performed in
February-March 2026 on the `yocto-5.0-scarthgap` branch. Both the Hub V2
(TI AM335x) and Hub V3 (NXP i.MX6DL) platforms were ported.

## Version History

| Release      | Yocto         | Kernel  | Branch                 |
|--------------|---------------|---------|------------------------|
| Original     | Warrior 2.7   | 4.19    | (legacy)               |
| Previous     | Dunfell 3.1   | 5.4.273 | `yocto-3.1-dunfell`    |
| **Current**  | Scarthgap 5.0 | 6.6.127 | `yocto-5.0-scarthgap`  |

Scarthgap is an LTS release supported through April 2028.

## Submodules

| Submodule                | Branch/Version |
|--------------------------|----------------|
| poky                     | scarthgap      |
| meta-openembedded        | scarthgap      |
| meta-freescale           | scarthgap      |
| meta-freescale-3rdparty  | scarthgap      |

## Build System Changes

### Override Syntax Migration

Scarthgap requires the colon-based override syntax introduced in Honister.
Every recipe, config, and bbappend in `meta-iris` was migrated:

- `_append` / `_prepend` / `_remove` &rarr; `:append` / `:prepend` / `:remove`
- `_pn-foo` &rarr; `:pn-foo`
- `_class-target` &rarr; `:class-target`
- `BBMASK`, `PACKAGECONFIG`, all machine overrides

### Config Updates

- `CONF_VERSION`: 1 &rarr; 2
- `BB_ENV_EXTRAWHITE` &rarr; `BB_ENV_PASSTHROUGH_ADDITIONS`
- `LICENSE_FLAGS_WHITELIST` &rarr; `LICENSE_FLAGS_ACCEPTED`
- Removed `image-mklibs` and `image-prelink` from `USER_CLASSES` (dropped
  upstream in Kirkstone)
- `PREFERRED_VERSION_linux-yocto`: `5.4%` &rarr; `6.6%`
- Added `ipv4` and `ipv6` to `DISTRO_FEATURES`

### Recipe Compatibility Fixes

| Recipe             | Issue                                        | Fix                                           |
|--------------------|----------------------------------------------|-----------------------------------------------|
| curl               | Scarthgap ships 8.x; `--with-ssh` removed    | Widened bbappend, removed obsolete flags       |
| dropbear           | Version jump to 2022.x                       | Widened bbappend wildcard                      |
| ca-certificates    | bbappend broke native build                  | Scoped Java keystore to `class-target`         |
| openjdk-8-armhf    | Override script mangled `_arm` in URL         | Restored correct download URL                 |
| validate\_image.c  | GCC 14 `-Werror=mismatched-dealloc`          | Fixed `pclose()`/`fclose()` mismatch          |
| bluez5             | `g_memdup` deprecated                        | Migrated to `g_memdup2`                       |
| All patches        | Scarthgap enforces `Upstream-Status` QA      | Added headers to all patches                  |
| GitHub fetches     | `git://` protocol no longer supported        | `FETCHCMD_git` redirect to `https://`         |

## Hub V2 (TI AM335x) Platform Changes

Build directory: `build-ti/`, machine: `beaglebone-yocto`.

### Kernel 5.4 &rarr; 6.6

- New `linux-yocto_6.6.bbappend` targeting `v6.6/standard/beaglebone`
- Old `linux-yocto_5.4.bbappend` masked via `BBMASK` (not deleted, for
  reference)
- Kernel output remains `uImage` (required by the factory U-Boot 2016.03
  which cannot be updated)
- Defconfig cleaned up: removed deprecated `CONFIG_LEDS_TRIGGER_GPIO`,
  `CONFIG_USB_DEBUG`, `CONFIG_PWM_TIPWMSS`

### Device Tree Rework

The 6.6 kernel reorganised ARM device tree sources into
`arch/arm/boot/dts/ti/omap/`. The `am335x-boneblack-common.dtsi` was also
restructured (HDMI split into a separate file). All DTS patches were
regenerated against the actual 6.6 tree.

- MMC device numbering now handled via DT aliases (previously patched in
  `drivers/mmc/core/block.c`)
- HDMI and PRUSS disabled via DTS overlay
- Internal RTC disabled in DTS only (the hwmod table approach from 5.4 is
  no longer needed)
- AM33XX_PADCONF macros used for pin muxing

### GPIO Probe Order

Kernel 6.6 probes GPIO banks in a different order (1&rarr;2&rarr;3&rarr;0 instead
of 0&rarr;1&rarr;2&rarr;3), and GPIO base numbers are dynamically allocated.
`irisinitd` was updated to look up GPIOs by hardware address rather than
assuming fixed base numbers.

### Serial Console

Kernel 5.4+ uses the 8250-OMAP driver (`ttyS0`) instead of the legacy OMAP
serial driver (`ttyO0`). Symlinks `ttyO0`&rarr;`ttyS0` etc. are created by
`irisinit` for backward compatibility with the agent.

### JVM Memory

The hub has only 512 MiB RAM. `JAVA_DBG_OPTS` now constrains
`ReservedCodeCacheSize`, `MaxMetaspaceSize`, and `CICompilerCount` to
prevent native memory exhaustion.

### Miscellaneous

- `mount.blacklist` renamed to `mount.ignorelist` (udev change)
- `/mfg` and `/data` removed from fstab (`irisinit` manages these mounts)
- Console security logic inverted for new `getty` behavior
- Ethernet DHCP fallback added

## Hub V3 (NXP i.MX6DL) Platform Changes

Build directory: `build-fsl/`, machine: `imxdimagic`.

The Hub V3 port to Scarthgap was done from scratch since the previous
kernel (4.14) had significant driver differences. The kernel uses
`linux-fslc-lts` 6.6 from meta-freescale with five custom patches:

| Patch | Description |
|-------|-------------|
| 0001  | Hub V3 device tree (UART, GPIO, RGMII PHY, I2C, audio, RTC) |
| 0002  | Custom kernel drivers (NFC ST95HF, radio interfaces) |
| 0003  | imx-nau8810 ASoC machine driver |
| 0004  | FEC `fec_mac` boot parameter for MAC address passthrough |
| 0005  | NAU8810 BCLK divider fix for PLL mode |

### RGMII Networking

The AR8035 Ethernet PHY requires `qca,clk-out-frequency = <125000000>` in
the device tree or the kernel's PHY driver resets the 125 MHz clock output,
breaking RGMII timing.

### Audio (NAU8810 Codec)

The NAU8810 mono codec receives a 24 MHz master clock from the i.MX6
CKO1 output (OSC24M). This clock frequency cannot produce standard audio
sample rates via the codec's MCLK prescaler alone, so PLL mode is
required.

Key issues resolved:

- **SSI clock mode:** The SSI must be configured as clock consumer
  (`CBC_CFC` / I2S slave) so it syncs to the codec's BCLK and FS. The
  machine driver uses `snd_soc_daifmt_clock_provider_flipped()` to
  correctly flip `CBP_CFP` for the CPU DAI. Passing the unflipped value
  puts the SSI in master mode, causing bus contention.
- **BCLK divider:** In PLL mode, IMCLK = 1024&times;fs (not 256&times;fs
  as the upstream BCLK auto-config assumes). Patch 0005 increases the BCLK
  divider by 4&times; when PLL mode is active.
- **TDM slots:** The CPU DAI requires explicit TDM slot configuration
  (`set_tdm_slot(cpu_dai, 0, 0, 2, width)`) for correct I2S framing.
- **AUDMUX routing:** Internal port configured to receive BCLK/FS from the
  external (codec) port in codec-master mode.
- **Audio group:** The agent user is added to the `audio` group for
  `/dev/snd/` access.

### Serial Ports

The i.MX6 serial ports use `ttymxc*` device names (not `ttyS*` as on the
AM335x). Symlinks are created in the platform-specific `irisinit-imxdimagic`
script, not in the agent.

### Other Hardware

- **RTC:** PCF8563 on I2C2, enabled in device tree
- **Buttons:** GPIO-keys DTS node removed; `irisinitd` polls buttons via
  sysfs GPIO directly
- **DHCP:** ifplugd race condition workaround added to init scripts

## U-Boot

U-Boot is **not upgraded** as part of this port. Both platforms continue
using the Warrior-era 2016.03 builds:

- Hub V2: Factory MLO (SPL) is permanently burned and cannot be reflashed.
  The `u-boot.img` second stage is updatable but remains on 2016.03.
- Hub V3: Custom `u-boot-imxgs` recipe builds from 2016.03 with
  platform-specific patches.

Both U-Boot versions expect `uImage` format kernels.

## Testing Checklist

After flashing a new build, verify on each platform:

- [ ] Kernel boots, correct version in `uname -a`
- [ ] Ethernet link up, DHCP obtains address
- [ ] All serial radios detected: Z-Wave (ZM5304), Zigbee (EM3587), BLE
- [ ] GPIO LEDs controllable (green, yellow, red)
- [ ] Buzzer (PWM) produces tone
- [ ] Battery hold GPIO asserted
- [ ] Agent starts and connects to platform
- [ ] OTA update installs and reboots to new partition
- [ ] Audio playback: `play /data/agent/conf/sounds/Success.mp3` (Hub V3)
- [ ] RTC preserves time across reboot (Hub V3: PCF8563)
