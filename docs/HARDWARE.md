# Arcus Hub OS — Hardware Reference

This document covers both hardware platforms supported by Arcus Hub OS:
the **Hub V2** (TI AM335x) and the **Hub V3** (NXP i.MX6DualLite).

---

## Hub V2 — TI AM335x (BeagleBone Black)

### SoC and Memory

| Component | Detail |
|-----------|--------|
| SoC | TI AM335x ES2.1 |
| CPU | ARM Cortex-A8, single-core |
| RAM | 512 MiB |
| Storage | eMMC on mmcblk0 |
| Machine | `beaglebone-yocto` |

There is **no SD card slot** on this hardware revision.

### Boot Chain

```
ROM → SPL/MLO (factory 2013.10, NOT updatable)
    → u-boot.img (2016.03, updatable)
    → uImage (kernel)
```

The factory-burned MLO/SPL **cannot be reflashed**. Bricking it is permanent.
The `create_update_file` script uses a pre-built U-Boot from `build-ti/tools/uboot-debug/`.

### Partition Layout (mmcblk0)

| Partition | Device | Symlink | Content |
|-----------|--------|---------|---------|
| p1 | mmcblk0p1 | — | U-Boot (FAT) |
| p2 | mmcblk0p2 | /dev/kern1 | Kernel slot A (ext2) |
| p3 | mmcblk0p3 | /dev/fs1 | Rootfs slot A (squashfs) |
| p4 | mmcblk0p4 | — | Extended partition |
| p5 | mmcblk0p5 | /dev/kern2 | Kernel slot B (ext2) |
| p6 | mmcblk0p6 | /dev/fs2 | Rootfs slot B (squashfs) |
| p7 | mmcblk0p7 | /dev/mfg | Manufacturing data (ext4) |
| p8 | mmcblk0p8 | /dev/data | Persistent data (ext4) |

A/B boot selection is controlled by `bootindex` files in each kernel partition.

### Serial Console

- UART0 at 0x44E09000 — `ttyS0` (kernel 5.4+) or `ttyO0` (kernel 4.x)
- 115200n8

### Radios

| Radio | Chip | UART | Speed | Flow Control |
|-------|------|------|-------|--------------|
| Z-Wave | ZM5304 | ttyS1 (UART1) | 115200 | None |
| Zigbee | EM3587 | ttyS2 (UART2) | 115200 | Hardware (RTS/CTS) |
| BLE | TI CC2541 | ttyS4 (UART4) | 115200 | Hardware (RTS/CTS) |

### GPIO Map

GPIOs are on the AM335x banks. Kernel 6.6 uses dynamic base allocation; the
`irisinit-ti` script looks up each bank by hardware address to compute the
correct sysfs number.

| GPIO | Bank.Pin | Function | Direction |
|------|----------|----------|-----------|
| GPIO1_12 | 1.12 | Reset button | Input (both edges) |
| GPIO1_16 | 1.16 | Zigbee reset | Output (active-low) |
| GPIO1_17 | 1.17 | Z-Wave reset | Output (active-low) |
| GPIO1_21 | 1.21 | Green LED | Output |
| GPIO1_22 | 1.22 | Yellow LED | Output |
| GPIO1_23 | 1.23 | Red LED | Output |
| GPIO1_26 | 1.26 | USB1 over-current | Input (both edges) |
| GPIO2_26 | 2.26 | BLE reset | Output (active-low) |
| GPIO2_29 | 2.29 | BLE programming data | Output |
| GPIO2_30 | 2.30 | BLE programming clock | Output |
| GPIO3_20 | 3.20 | USB0 over-current | Input (both edges) |
| GPIO3_21 | 3.21 | Battery hold | Output (keeps backup battery power on) |

GPIO bank hardware addresses used for dynamic base lookup:
- Bank 1: `4804c000`
- Bank 2: `481ac000`
- Bank 3: `481ae000`

### Buttons

One button: reset (GPIO1_12). See [Button Behavior](#button-behavior) below
for hold times and actions.

### LEDs

Three discrete LEDs controlled via GPIO1:

| Color | GPIO | Sysfs |
|-------|------|-------|
| Green | GPIO1_21 | led2 |
| Yellow | GPIO1_22 | led3 |
| Red | GPIO1_23 | led4 |

All three solid = U-Boot is running, kernel not yet booted.

### Networking

**Ethernet:**
- Controller: TI CPSW (Common Platform Switch)
- PHY: auto-detected via MDIO (SMSC PHY driver enabled)
- Interface mode: MII (4-bit TX/RX data lines)
- PHY reset: GPIO1_8 (active-low, 300 us assert / 50 ms deassert)
- MAC address: loaded from `/mfg/config/macAddr1` at boot by `irisinit-ti`
- Only CPSW port 1 is used; port 2 is disabled

### Buzzer

PWM buzzer on `ehrpwm2B` (GPMC_AD9, pin mux mode 4). Controlled via
`/sys/class/pwm/pwmchip0/pwm1/`.

### Watchdog

Uses the AM335x internal watchdog timer at `/dev/watchdog0`. The SoC watchdog
supports long timeouts, so the system uses a 300-second (5-minute) period.

**Hardware details:**
- Device: `/dev/watchdog0` (standard Linux watchdog interface)
- Timeout: 300 seconds (`WATCHDOG_PERIOD`)
- Poke interval: 150 seconds (`WATCHDOG_POKE_PERIODIC`)
- Interface: standard `WDIOC_SETTIMEOUT` ioctl, character writes to feed/stop

**Software management** (see [Watchdog Architecture](#watchdog-architecture)
below for the full multi-layer design):
- **Dev images:** `irisinitd` opens `/dev/watchdog0`, sets the 300s timeout via
  `WDIOC_SETTIMEOUT`, then pokes every 150s via a GLib timer callback.
- **Release images:** the Java agent handles watchdog poking directly;
  `irisinitd` skips watchdog setup to avoid conflicts.
- `irislib.c` guards against dual access — `writeWatchdog()` checks `lsof` for
  an existing handle before writing to the device.

**Feed protocol** (via character writes to `/dev/watchdog0`):
- `"1"` — start/arm the watchdog
- `"A"` — feed/poke (any non-`V` character works)
- `"V"` — magic close (disarm the watchdog)

### UART Pin Mux

| UART | RX/TX Pins | Mode | CTS/RTS Pins | Mode |
|------|-----------|------|-------------|------|
| uart1 | UART1_RXD/TXD | 0 | — | — |
| uart2 | SPI0_SCLK/D0 | 1 | LCD_DATA8/9 | 6 |
| uart4 | GPMC_WAIT0/WPN | 6 | LCD_DATA12/13 | 6 |

### RTC

The AM335x internal RTC at 0x44E3E000 is **disabled** (not used by the hub).
Must be disabled in the device tree; on kernel 5.4, the parent `target-module@3e000`
node must also be disabled to prevent the ti-sysc driver from probing it.

---

## Hub V3 — NXP i.MX6DualLite (IMAGIC)

### SoC and Memory

| Component | Detail |
|-----------|--------|
| SoC | NXP i.MX6DualLite (i.MX6DL) / i.MX6Quad variants |
| CPU | ARM Cortex-A9, dual-core (SMP) |
| RAM | 1 GiB (base 0x10000000, size 0x40000000) |
| Storage | eMMC on mmcblk2 (8-bit, SD4/usdhc4) |
| Machine | `imxdimagic` |

Additional SDIO interface:
- **WiFi SDIO** on usdhc1 (non-removable)

### Boot Chain

```
i.MX6 Boot ROM → u-boot.imx (2016.03, on mmcblk2boot0)
               → zImage (kernel)
```

U-Boot is written to eMMC boot partition (`/dev/mmcblk2boot0`) at sector offset 2.

### Partition Layout (mmcblk2)

| Partition | Device | Symlink | Content |
|-----------|--------|---------|---------|
| p1 | mmcblk2p1 | /dev/kern1 | Kernel slot A (ext2) |
| p2 | mmcblk2p2 | /dev/fs1 | Rootfs slot A (squashfs) |
| p3 | mmcblk2p3 | /dev/kern2 | Kernel slot B (ext2) |
| p5 | mmcblk2p5 | /dev/fs2 | Rootfs slot B (squashfs) |
| p6 | mmcblk2p6 | /dev/mfg | Manufacturing data (ext4) |
| p7 | mmcblk2p7 | /dev/data | Persistent data (ext4) |

### Serial Console

- UART1 (`ttymxc0`) — 115200n8
- Note: i.MX UARTs are `ttymxc` (not `ttyS`)

### Serial Port Mapping

The `irisinit-imxdimagic` script creates symlinks so the agent uses consistent
device names across platforms:

| Function | i.MX UART | Kernel Device | Symlink |
|----------|-----------|---------------|---------|
| Console | UART1 | ttymxc0 | — |
| 4G Modem | UART2 | ttymxc1 | /dev/ttyO2 |
| Spare | UART3 | ttymxc2 | /dev/ttyO4 |
| Z-Wave | UART4 | ttymxc3 | — |
| BLE | UART5 | ttymxc4 | /dev/ttyO1 |

### Radios

| Radio | Chip | UART | Speed | Flow Control |
|-------|------|------|-------|--------------|
| Z-Wave | ZW050x | ttymxc3 (UART4) | 115200 | None |
| Zigbee | Silicon Labs EFR32MG1B (SPI) | — | — | — |
| BLE | Broadcom (via HCI) | ttymxc4 (UART5) | 115200 | Hardware (RTS/CTS) |

### Networking

**Ethernet:**
- PHY: Qualcomm/Atheros AR8035 (RGMII)
- Controller: i.MX6 FEC
- PHY reset: GPIO4_28
- Default MAC: dc:07:c1:00:ed:85 (overridden by `/mfg/config/macAddr1`)
- **Important:** The AR8035 needs `qca,clk-out-frequency = <125000000>` in the
  device tree, or the kernel resets the 125 MHz reference clock output

**WiFi:**
- Chip: Broadcom BCM43362 via SDIO (on usdhc1/mmc0)
- Firmware: `brcmfmac43362-sdio.bin` + `.txt`
- Interface: `wlan0`
- Power GPIOs:
  - WL_REG_ON: GPIO2_0
  - WL_HOST_WAKE: GPIO2_4
  - WIFI_PWR_EN: GPIO6_16 (CUT-2 boards)

### 4G/LTE Modem

- Chip: Quectel EC25-A
- UART: ttymxc1 (UART2) with hardware flow control
- GPIOs:
  - GPIO_4G_RST: GPIO5_30
  - GPIO_4G_KEY: GPIO5_31
  - GPIO_4G_PWR_EN: GPIO6_0
  - GPIO_4G_NET_STATE: GPIO6_1

### Audio

**Codec: Nuvoton NAU8810**
- I2C address: 0x1a (on I2C1)
- MCLK: 24 MHz from CKO2 (CSI0_MCLK pad → CCM_CLKO1)
- Must use **PLL mode** (MCLK goes through codec PLL for internal DAC clocking)
- Audio interface: I2S via SSI1
- **SSI is bus master, codec is slave** (CBC_CFC mode) — codec BCLK doesn't
  reliably reach SSI in slave mode on this board
- AUDMUX routing: internal port 1 (SSI1) ↔ external port 3 (codec pads)
- Speaker output: SPKOUTP/SPKOUTN via LM4871 amplifiers (GPIO6_11, GPIO6_14)

### LED Ring

12 RGB LEDs driven by an MBI6023 LED controller via bit-banged SPI:
- CLK: GPIO5_14
- SDI: GPIO5_15
- LED_PWR_EN: GPIO4_30
- Boot state: purple (R=max, G=0, B=max) on all 12 LEDs

### Buzzer

GPIO buzzer on GPIO1_9 via custom `imagic_buzzer` kernel driver (ioctl interface).

### Watchdog

External GPIO-based watchdog controlled by the custom `imagic_watchdog` kernel
driver. The i.MX6 internal watchdog only supports a max 128-second timeout, so
V3 uses an external watchdog chip driven by two GPIOs.

**Hardware details:**
- Feed pin: GPIO1_4 (`MX6QDL_PAD_GPIO_4__GPIO1_IO04`)
- Enable pin: GPIO1_3 (`MX6QDL_PAD_GPIO_3__GPIO1_IO03`)
- Timeout: 120 seconds (`WATCHDOG_PERIOD`)
- Poke interval: 60 seconds (`WATCHDOG_POKE_PERIODIC`)
- Device: `/dev/imagic_watchdog` (misc device)
- DT compatible: `"imagic,imagic_watchdog"`

**Driver interface** (`imagic_watchdog.c`):
- Opening the device enables the watchdog (sets enable GPIO high, feeds once)
- `ioctl(fd, FEED_DOG, 0)` — toggle the feed pin to reset the counter
- `ioctl(fd, CLOSE_DOG, 0)` — disable the watchdog (sets enable GPIO low)
- The feed pin toggles state on each feed (high→low→high) rather than pulsing
- Closing the file descriptor does NOT disable the watchdog (must use
  `CLOSE_DOG` ioctl)

**Software management** mirrors V2 but through `/dev/imagic_watchdog` instead of
`/dev/watchdog0` — see [Watchdog Architecture](#watchdog-architecture) below.

### Buttons

Three buttons, polled via sysfs (not gpio-keys):

| Button | GPIO | Sysfs # |
|--------|------|---------|
| Reset | GPIO4_26 | 122 |
| Power Down | GPIO4_27 | 123 |
| Iris Button | GPIO4_31 | 127 |

The **Power Down** button halts the hub if held for 10 seconds. The **Iris
Button** re-enters BLE provisioning if held for 5 seconds (release images
only). See [Button Behavior](#button-behavior) below for reset button hold
times.

### GPIO Map (Radio and Power)

**Zigbee:**
- GPIO6_2, GPIO6_3, GPIO6_4 — Zigbee control GPIOs
- GPIO6_5 — Zigbee power enable
- GPIO5_22 — Zigbee reset (CUT-2 boards)

**Z-Wave:**
- GPIO4_22 — Z-Wave reset
- GPIO4_23 — Z-Wave power enable

**BLE:**
- GPIO5_7 — BT_EN
- GPIO5_8 — BT_RESTORE
- GPIO5_9 — BT_PWR_EN
- GPIO5_17 — BT_RST

**Power:**
- GPIO4_10 — 5V0 system power enable
- GPIO1_2 — DC detect
- GPIO3_22 — USB OTG power

**Battery/Coulomb Counter:**
- GPIO4_14 — CMC_STAT
- GPIO4_15 — GG_ALRT

### NFC

- Chip: ST 95HF
- Interface: SPI via ECSPI2 (CS0, 2 MHz, CPHA=1)
- GPIOs: GPIO5_5 (IRQ_IN), GPIO5_6 (IRQ_OUT), GPIO5_12 (CS0)
- Currently disabled in software

### I2C Buses

| Bus | Speed | Devices |
|-----|-------|---------|
| I2C1 | 100 kHz | NAU8810 audio codec (0x1a) |
| I2C2 | 100 kHz | PCF8563 RTC (0x51) |
| I2C3 | 100 kHz | EDT FT5406 touchscreen (0x38, disabled) |

### Hardware Versions

The `irisinit-imxdimagic` script reads `/tmp/mfg/config/hwVer` to detect the
board revision. CUT-2 boards have different Zigbee reset wiring (GPIO5_22 vs
GPIO6_5) and additional WiFi power enable (GPIO6_16).

---

## USB

### Hub V2

Two USB ports with separate over-current detection GPIOs:
- USB0 over-current: GPIO3_20
- USB1 over-current: GPIO1_26
- Battery daemon can disable USB ports on battery power to save energy

### Hub V3

One physical USB port:
- **USB Host 1 (usbh1):** Full-size USB-A port, EHCI host mode, 5V VBUS always-on
- **USB OTG (usbotg):** ChipIdea dual-role controller — present on SoC but not
  exposed as a physical port. VBUS via GPIO3_22, OTG features disabled.
- Battery daemon controls USB port power via GPIO 86 (`/tmp/io/usbPower`)

Kernel USB support includes: mass storage, CDC Ethernet (for 4G), CP210x, FTDI SIO,
PL2303, and USB Option (4G modem serial).

---

## Cryptographic Accelerators

| Platform | Hardware | Kernel Config |
|----------|----------|---------------|
| Hub V2 | None (software-only crypto) | — |
| Hub V3 | i.MX6 CAAM (AES, SHA, HMAC) | `CONFIG_CRYPTO_DEV_FSL_CAAM=y` |
| Hub V3 | i.MX6 MXS DCP | `CONFIG_CRYPTO_DEV_MXS_DCP=y` |

The CAAM (Cryptographic Acceleration and Assurance Module) provides hardware-backed
AES, SHA, and HMAC acceleration on the Hub V3. Hub V2 uses software implementations.

---

## RTC

### Hub V2

The AM335x internal RTC at 0x44E3E000 is **disabled** — its clocks are gated and
probing it causes a bus fault. Must be disabled in the device tree; on kernel 5.4+
the parent `target-module@3e000` node must also be disabled to prevent the ti-sysc
driver from probing. No external RTC.

### Hub V3

Two RTC sources:
- **NXP PCF8563** — external battery-backed RTC on I2C2 at address 0x51
  (`CONFIG_RTC_DRV_PCF8563=y`). Standard coin cell backup (CR2032).
- **i.MX6 SNVS RTC** — SoC internal RTC in the Secure Non-Volatile Storage block
  (`CONFIG_RTC_DRV_SNVS=y`). Also provides system poweroff via `snvs_poweroff`.

---

## PCIe (Hub V3)

The i.MX6 PCIe host controller is present but **disabled** in the device tree:
- Reset GPIO: GPIO1_8 (active-low)
- 3.3V power regulator for mPCIe slot (`reg_pcie`, always-on)
- Kernel support: `CONFIG_PCI_IMX6_HOST=y`, `CONFIG_PCI_MSI=y`

The mPCIe slot can accept optional expansion modules (WiFi/4G adapters), though
the production hub uses SDIO for WiFi and UART for 4G instead.

---

## Voltage Regulators (Hub V3)

Fixed voltage regulators defined in the device tree:

| Regulator | Voltage | Control | Purpose |
|-----------|---------|---------|---------|
| `reg_usb_otg_vbus` | 5V | GPIO3_22 | USB OTG VBUS |
| `reg_usb_h1_vbus` | 5V | Always-on | USB host VBUS |
| `reg_pcie` | 3.3V | GPIO2_31 | PCIe/mPCIe power |
| `reg_sensor` | 3.3V | Always-on | Sensor supply (500 us startup) |

---

## eMMC Health Monitoring

The `emmcparm` tool (Micron-provided, installed to `/usr/bin/`) reads eMMC device
health via ioctl:

| Option | Function |
|--------|----------|
| `-i` | Read CID/CSD via sysfs (manufacturer, serial, date) |
| `-I` | Read CID/CSD via CMD8/9/10 |
| `-S` | Query spare block count and usage percentage |
| `-B` | Read initial and runtime bad block counts |
| `-E` | Read MLC and SLC area erase count statistics (min/max/avg) |

Useful for monitoring flash wear on deployed hubs.

---

## Unused/Disabled Interfaces

The following interfaces are defined in the Hub V3 device tree but **not used**
on production hardware:

| Interface | Chip | Bus | Status |
|-----------|------|-----|--------|
| Touchscreen | EDT FT5406 | I2C3 (0x38) | Disabled — no display panel |
| Resistive touch | ADS7846 | SPI ECSPI1 CS1 | Disabled |
| NFC | ST 95HF | SPI ECSPI2 CS0 | Disabled in software |
| PCIe host | — | i.MX6 PCIe | Disabled — uses SDIO/UART instead |
| Display | — | — | No LVDS/DPI/HDMI hardware present |
| Camera | — | — | CSI pads reused for GPIOs and clocks |

LCD-related pin mux pads in the device tree are reused for UART flow control,
AUDMUX audio routing, and GPIO functions — they are **not** connected to any
display hardware.

---

## Button Behavior

### Reset Button (Both Platforms)

The reset button (GPIO1_12 on V2, GPIO4_26 on V3) triggers escalating actions
based on hold duration. LED patterns change as you hold to indicate the current
action level:

| Hold Time | Action | LED Pattern |
|-----------|--------|-------------|
| < 1 s | Ignored | — |
| 1–9 s | Reboot | `button-reboot` |
| 10–19 s | Soft reset (agent config reset) | `button-soft-reset` |
| 20+ s | Factory default (full reset) | `button-factory-default` |

The action fires when the button is **released** after reaching the threshold.

- **Reboot:** calls `hub_restart now`
- **Soft reset:** creates `/data/agent/.soft_reset` marker, then reboots — the
  agent clears its configuration on next start
- **Factory default:** calls `factory_default` which wipes `/data`, resets all
  radios (Zigbee, Z-Wave, BLE), and reboots

### Reset Button During U-Boot

Holding the reset button during U-Boot startup (before Linux boots) flips the
A/B partition selection, allowing recovery from a bad firmware. See
[UBOOT.md](UBOOT.md) for details.

### Iris Button (Hub V3 Only)

| Hold Time | Action |
|-----------|--------|
| 5+ s | Re-enter BLE provisioning (release images only) |

### Power Down Button (Hub V3 Only)

| Hold Time | Action |
|-----------|--------|
| 10+ s | Halt the CPU (safe power off) |

---

## Watchdog Architecture

The system uses a three-layer watchdog design: hardware watchdog, software
watchdog management, and agent-level crash recovery.

### Layer 1: Hardware Watchdog

The hardware watchdog reboots the hub if software stops responding.

| | Hub V2 | Hub V3 |
|---|--------|--------|
| Type | AM335x internal WDT | External GPIO watchdog chip |
| Device | `/dev/watchdog0` | `/dev/imagic_watchdog` |
| Max timeout | Configurable (minutes) | ~128 s (i.MX6 internal), external varies |
| Configured timeout | 300 s (5 min) | 120 s (2 min) |
| Poke interval | 150 s | 60 s |
| Interface | Linux watchdog API (`WDIOC_*`) | Custom ioctl (`FEED_DOG`, `CLOSE_DOG`) |
| Driver | `omap_wdt` (in-tree) | `imagic_watchdog` (out-of-tree) |

### Layer 2: Software Watchdog Management (`irisinitd` / agent)

**Dev images** — `irisinitd` owns the hardware watchdog:
1. Opens the watchdog device and sets the timeout via ioctl
2. Starts a GLib periodic timer to poke the watchdog
3. `irislib.c` functions (`startWatchdog`, `pokeWatchdog`, `stopWatchdog`) guard
   against conflicts — they check `lsof` before writing to avoid interfering if
   the agent has the device open

**Release images** — the Java agent owns the hardware watchdog:
- `irisinitd` skips watchdog setup entirely (`!IRIS_isReleaseImage()` guard)
- The agent opens the device and pokes it as part of its main loop
- If the agent crashes, the watchdog stops being fed and the hub reboots

### Layer 3: Agent Crash Recovery (`irisagentd`)

`irisagentd` monitors the Java agent process and implements escalating recovery:

| Condition | Action |
|-----------|--------|
| Agent exits | Restart immediately |
| 3 rapid failures within 30 min (`SOFT_RESET_THRESHOLD`) | Signal a soft reset (creates `/data/agent/.soft_reset`) |
| 6 rapid failures within 30 min (`FACTORY_DEF_THRESHOLD`) | Signal factory default (creates `/data/agent/.factory_default`) |
| Agent runs > 30 min (`MIN_RUNNING_PERIOD`) | Failure counter resets |

A "rapid failure" means the agent crashes again within 30 minutes of the last
restart. The agent reads the soft reset / factory default marker files on
startup and takes the appropriate recovery action.

### Layer 4: Daemon Watcher (`dwatcher`)

`dwatcher` runs on **release images only** and monitors critical system daemons
every 5 seconds (after a 60-second startup delay):

| Daemon | Monitored | Notes |
|--------|-----------|-------|
| `irisinitd` | Always | Core init daemon — restarted if missing |
| `batteryd` | Release only | Battery/power management |
| `iris4gd` | Release only | 4G modem (only if binary exists) |
| `irisnfcd` | V3 only | NFC daemon (currently `#ifdef LATER` — disabled) |

### Watchdog Flow Summary

```
Hardware Watchdog Timer
    ↑ poke (periodic)
    |
Dev image: irisinitd ←──── GLib timer (150s V2, 60s V3)
Release image: Java agent ←── agent main loop
    ↑ restart on crash
    |
irisagentd (monitors agent process)
    ↑ restart on crash
    |
dwatcher (monitors irisinitd, batteryd, iris4gd)
```

If the agent crashes, `irisagentd` restarts it. If `irisinitd` crashes,
`dwatcher` restarts it. If all software stops, the hardware watchdog reboots
the hub. This ensures the hub recovers from any single-process failure.

### Key Source Files

| File | Purpose |
|------|---------|
| `meta-iris/recipes-core/iris-lib/files/irislib.c` | `startWatchdog()`, `pokeWatchdog()`, `stopWatchdog()` |
| `meta-iris/recipes-core/iris-lib/files/irisdefs.h` | `HW_WATCHDOG_DEV`, timeout constants |
| `meta-iris/recipes-core/iris-utils/files/irisinitd.c` | Watchdog setup (lines 1668–1683) |
| `meta-iris/recipes-core/iris-agent/files/irisagentd.c` | Agent crash recovery thresholds |
| `meta-iris/recipes-core/iris-utils/files/dwatcher.c` | Daemon process monitor |
| `meta-iris/recipes-kernel/linux/linux-fslc-lts/0002-*.patch` | `imagic_watchdog.c` driver |

---

## Platform Comparison

| Feature | Hub V2 | Hub V3 |
|---------|--------|--------|
| SoC | TI AM335x (Cortex-A8) | NXP i.MX6DL (Cortex-A9 x2) |
| RAM | 512 MiB | 1 GiB |
| Storage | eMMC (mmcblk0) | eMMC (mmcblk2, 8-bit) |
| SD card | No | No (usdhc3 in DT, not supported) |
| WiFi | No | BCM43362 (SDIO) |
| 4G modem | No | Quectel EC25-A |
| Audio | No | NAU8810 + LM4871 amplifiers |
| NFC | No | ST 95HF (disabled) |
| LEDs | 3 discrete (R/Y/G) | 12 RGB ring (MBI6023) |
| Buzzer | PWM (ehrpwm2B) | GPIO (custom driver) |
| Watchdog | Internal AM335x WDT, 300s timeout | External GPIO chip, 120s timeout |
| Z-Wave | ZM5304 | ZW050x |
| Zigbee | EM3587 (UART) | EFR32MG1B (SPI) |
| BLE | TI CC2541 | Broadcom (HCI UART) |
| Ethernet | CPSW + SMSC PHY (MII) | FEC + AR8035 (RGMII) |
| USB | 2 host ports | 1 host port (OTG on SoC, not exposed) |
| Crypto HW | None | CAAM + MXS DCP |
| RTC | Disabled (internal) | PCF8563 (external) + SNVS |
| PCIe | No | Yes (disabled, mPCIe slot) |
| Serial naming | ttyS* | ttymxc* |
| Kernel format | uImage | zImage |
| U-Boot location | FAT partition (p1) | eMMC boot partition (mmcblk2boot0) |
| Buttons | 1 (reset) | 3 (reset, power, iris) |

---

## Kernel and Software Stack

| Component | Hub V2 | Hub V3 |
|-----------|--------|--------|
| Kernel recipe | `linux-yocto` | `linux-fslc-iris` |
| Kernel version | 6.6 (Scarthgap) | 6.6 (Scarthgap) |
| U-Boot version | 2016.03 (TI patches) | 2016.03 (NXP patches) |
| Distro config | `poky-iris-ti` | `poky-iris-fsl` |
| Rootfs type | squashfs (read-only) | squashfs (read-only) |
| Image format | tar.bz2 + squashfs | tar.gz + squashfs |

Squashfs kernel support includes ZLIB and XZ compression, embedded mode with
fragment cache size 10.

### Kernel Config Fragments (Hub V2)

Located in `meta-iris/recipes-kernel/linux/linux-6.x/`:

| Fragment | Purpose |
|----------|---------|
| `serial.cfg` | 8250-OMAP serial driver, 6 UARTs |
| `debug.cfg` | Console on ttyO0+ttyS0, loglevel=7 |
| `fs.cfg` | Squashfs, MMC block (16 minors) |
| `usb.cfg` | USB networking (CDC Ethernet for LTE) |
| `adc.cfg` | TI AM335x ADC (battery voltage) |
| `gpio.cfg` | GPIO sysfs for userspace access |
| `leds.cfg` | GPIO and PWM LED drivers |
| `pwm.cfg` | TI ECAP and EHRPWM drivers |

### Hub V3 Custom Kernel Drivers

Built from `0002-IMAGIC-Hub-V3-custom-driver-support.patch`:

| Driver | Module | Purpose |
|--------|--------|---------|
| `imagic_buzzer` | `buzzer.c` | GPIO buzzer with ioctl (ON/OFF) |
| `imagic_watchdog` | `imagic_watchdog.c` | External watchdog (feed/enable via GPIO) |
| `mbi6023` | `mbi6023.c` | LED ring controller (bit-banged SPI, 39-word format) |
| `imagic_imx6_gpio` | `imagic_imx6_gpio.c` | Userspace GPIO get/set/direction |
| spidev NFC hooks | — | ST 95HF GPIO ioctls for IRQ and CS |

---

## Power and Battery Management

### Hub V2

- **Power detection:** I2C bus 0, device 0x24, register 0xA — bit 0x08 indicates AC power
- **Battery hold:** GPIO3_21 keeps backup battery regulator on (must delay 1s before
  asserting to avoid regulator glitching at boot)
- **Voltage monitoring:** 12-bit ADC via AM335x ADC
  - Voltage divider: R1=36.5k, R2=100k
  - Minimum voltage: 4.2V, no-batteries threshold: 1.25V
  - Diode drop: 0.03V (AC), 0.25V (battery)
- **USB power control:** Can disable USB ports on battery to save power; re-enables
  on AC restore
- **Status files:**
  - `/tmp/battery_on` — timestamp when battery mode entered
  - `/tmp/battery_level` — battery level 0-100
  - `/tmp/battery_voltage` — current voltage reading

### Hub V3

- **Charger IC:** SY6991 at I2C address 0x6A
  - Register 0x00 bit 0x01 = VIN_PRESENT (1=AC, 0=battery)
- **Coulomb counter:** SY6410 at I2C address 0x30
  - Registers 0x02-0x05 for voltage and charge level
  - Voltage formula: 2.50 + (raw_value * 2.50 / 0x0FFF)
  - Voltage range: min 2.68V, max 4.15V
- **USB power:** Single port controlled via GPIO 86 (`/tmp/io/usbPower`)
- **Status files:**
  - `/tmp/charge_state` — Ready/Charging/Done/Fault/Boost
  - `/tmp/charge_level` — coulomb counter percentage
- **LTE interaction:** Power loss/restore triggers LTE modem restart state machine

---

## Manufacturing Partition

The `/mfg` partition stores per-hub identity and provisioning data. It is mounted
at boot by `irisinit`, copied to `/tmp/mfg` (read-only), then unmounted.

```
/mfg/
├── config/
│   ├── model         # "IH200" (V2) or "IH304" (V3)
│   ├── customer      # "IRIS"
│   ├── batchNo       # Manufacturing batch number
│   ├── hubID         # Unique hub identifier (also used as hostname)
│   ├── hwVer         # Hardware revision (V3 CUT-1 vs CUT-2)
│   ├── macAddr1      # Primary Ethernet MAC address
│   ├── macAddr2      # Secondary MAC address
│   └── hwFlashSize   # eMMC size in bytes (computed at boot)
├── certs/            # X.509 certificates for platform TLS
└── keys/             # Private keys for hub identity
```

The hub's flash size is computed at boot from `/sys/class/block/mmcblk{0,2}/size`
and written to `/tmp/mfg/config/hwFlashSize`.

---

## Debug Interfaces

### Serial Console

The only physical debug interface. No JTAG is exposed.

| Platform | UART | Device | Baud |
|----------|------|--------|------|
| Hub V2 | UART0 | ttyS0 / ttyO0 | 115200n8 |
| Hub V3 | UART1 | ttymxc0 | 115200n8 |

For early kernel debug (before console driver loads), interrupt U-Boot and set:
```
setenv optargs "earlycon=omap8250,mmio32,0x44e09000,115200n8"   # V2 only
```

### Debug Dongle

A USB flash drive containing an RSA-signed file (`<hubID>.dbg`) that enables
SSH access on release images:

1. `irisinitd` scans `/run/media` for a file matching `<hubID>.dbg`
2. File is RSA-verified against `/etc/.ssh/iris-fwsign.pub`
3. If valid, SSH starts with password authentication and debug mode flag
   (`/tmp/debug`) is set
4. Dongle removal is detected and SSH is stopped

Agent configuration can also be loaded from the dongle (`<hubID>.cfg` → `/tmp/agent_cfg`).

### Persistent SSH

For headless access without the dongle, create `/data/config/dropbear/ssh_enabled`.
See `RELEASE.md` for details.

---

## 4G/LTE Connectivity (Hub V3)

### Supported Modems

| Modem | USB ID | Interface | Notes |
|-------|--------|-----------|-------|
| QuecTel EC25-A (mPCIe) | 2c7c:0125 | wwan0 | Primary; uses `quectel-CM` daemon |
| Huawei E3372h | 12d1:14dc | eth1 | Fixed 192.168.8.x subnet |
| ZTE WeLink ME3630 | 19d2:1476 | usb0 | AT commands on /dev/ttyUSB1 |

### Control Interface

- **Control file:** `/tmp/lteControl` — write `connect`, `disconnect`, or `init`
- **Status file:** `/tmp/lte_dongle` — modem ID when detected
- **SIM info logged:** IMEI, IMSI, ICCID, MSISDN

### Network Management

- Primary DNS: `/tmp/pri_resolv.conf`
- Backup DNS: `/tmp/bkup_resolv.conf`
- Primary gateway: `/tmp/pri_gw.conf`
- Primary availability: `/tmp/primary_available`
- Start/stop scripts: `backup_start` / `backup_stop` (SUID 4755)

For system daemons, utilities, radio firmware tools, and key source files,
see [SYSTEM.md](SYSTEM.md).
