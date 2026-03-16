# Arcus Hub OS — U-Boot Reference

U-Boot 2016.03 is the bootloader for both Hub V2 and Hub V3. It was **not
upgraded** during the Yocto Scarthgap migration — the same 2016.03 builds are
carried forward as pre-built binaries.

---

## Overview

| | Hub V2 (AM335x) | Hub V3 (i.MX6DL) |
|---|--------|--------|
| Version | 2016.03 | 2016.03 (NXP fork) |
| Source | `u-boot.denx.de/u-boot.git` | `imx/uboot-imx.git` branch `nxp/imx_v2016.03_4.1.15_2.0.0_ga` |
| SPL | MLO (separate file) | Combined in `u-boot.imx` |
| U-Boot | `u-boot.img` | `u-boot.imx` |
| Storage | FAT partition (mmcblk0p1) | eMMC boot partition (mmcblk2boot0) |
| Console | `ttyS0` 115200n8 | `ttymxc0` 115200n8 |
| Boot delay | 3s (debug) / 0s (release) | 0s (production-locked) |
| Kernel format | uImage | zImage |
| Defconfig | `am335x_boneblack_defconfig` (patched) | `mx6dlimagic_defconfig` |

---

## A/B Boot Selection (Bootindex)

Both platforms use a `bootindex` file in each kernel partition to implement A/B
boot selection. U-Boot's `last_stage_init()` function reads both bootindex
files and selects the partition with the **higher** value.

### Boot Flow

```
U-Boot last_stage_init()
    ├── Read bootindex from kernel partition A
    ├── Read bootindex from kernel partition B
    ├── Check reset button GPIO (held = flip selection)
    ├── Select partition with higher bootindex
    └── Set mmcroot, bootpart environment variables
        → Boot kernel from selected partition
```

### Partition Mapping

| | Hub V2 | Hub V3 |
|---|--------|--------|
| Kernel A | mmcblk0p2 (mmc 1:2) | mmcblk2p1 (mmc 2:1) |
| Rootfs A | mmcblk0p3 | mmcblk2p2 |
| Kernel B | mmcblk0p5 (mmc 1:5) | mmcblk2p3 (mmc 2:3) |
| Rootfs B | mmcblk0p6 | mmcblk2p5 |

### Bootindex Update During Firmware Install

When `fwinstall` installs new firmware:

1. Reads bootindex from both kernel partitions
2. Determines which partition is **not** currently running (via `/proc/cmdline`)
3. Installs kernel + rootfs to the inactive partition
4. Writes `bootindex = max(index1, index2) + 1` to the target partition

This ensures the next boot uses the newly installed firmware. If the new
firmware fails, the old partition remains bootable with the lower index.

### Reset Button Override

Holding the reset button during U-Boot startup flips the partition selection:

| Platform | GPIO | Pin |
|----------|------|-----|
| Hub V2 | GPIO 44 | GPIO1_12 |
| Hub V3 | GPIO 122 | GPIO4_26 |

This allows recovering from a bad firmware by booting the previous slot.

---

## U-Boot Installation

### Hub V2

U-Boot lives on the FAT partition (mmcblk0p1):

```
/dev/mmcblk0p1 (vfat)
├── MLO          ← SPL (first-stage loader)
└── u-boot.img   ← Main U-Boot
```

`fwinstall` mounts the FAT partition, compares files with `diff`, and only
overwrites if different. Files are verified after write.

**Warning:** The factory-burned MLO/SPL in the AM335x ROM **cannot be
reflashed**. The MLO on the FAT partition is the second-stage SPL, which IS
updatable. But bricking the ROM boot is permanent.

### Hub V3

U-Boot is written directly to the eMMC hardware boot partition:

```bash
dd if=u-boot.imx of=/dev/mmcblk2boot0 seek=2 bs=512
```

The `seek=2` skips the first 1024 bytes (i.MX6 ROM boot header offset).
`fwinstall` compares the flash contents with `cmpFlashToFile()` before writing,
and verifies after. The `force_ro` sysfs flag must be cleared first:

```bash
echo 0 > /sys/block/mmcblk2boot0/force_ro
```

---

## Environment Variables

### Hub V2

```
loadaddr=0x82000000
fdtaddr=0x88000000
bootm_size=0x10000000
bootdir=/boot
bootfile=uImage
fdtfile=am335x-boneblack.dtb
console=ttyS0,115200n8
optargs=lpj=4980736 mtdoops.mtddev=omap2.nand
mmcdev=1
mmcroot=/dev/mmcblk0p3 ro
mmcrootfstype=ext4 rootwait
```

Environment is stored on MMC (CONFIG_ENV_IS_IN_MMC).

### Hub V3

```
loadaddr=0x12000000
fdt_addr=0x18000000
mmcdev=2
console=ttymxc0,115200
silent=1
```

Environment is stored on MMC at offset 768 KB (12 * 64 * 1024), size 8 KB.

---

## Production Lockdown (Hub V3)

The `0005-Lockdown-for-production.patch` applies these restrictions:

- `CONFIG_BOOTDELAY = 0` — no interactive prompt
- `CONFIG_SILENT_CONSOLE` — suppresses U-Boot output
- `CONFIG_SILENT_U_BOOT_ONLY` — kernel messages still visible
- `silent=1` environment variable
- `quiet` added to kernel bootargs

The pre-built binary at `build-fsl/tools/u-boot-release.imx.production-locked`
has this patch applied.

---

## DDR Configuration (Hub V3)

Multiple DDR patches support different memory chips found across board
revisions:

| Patch | Memory |
|-------|--------|
| `0003-REV-B-DDR-Support.patch` | Micron DDR (Rev B boards) |
| `0004-Fix-to-Samsung-K4B4G1646E-BYMA-DDR3-1866.patch` | Samsung K4B4G1646E |
| `0006-Update-to-Micron-DDR-settings.patch` | Updated Micron DDR timings |

U-Boot auto-detects the DDR variant and applies the correct timings.

---

## GPIO Usage in U-Boot

### Hub V2

| GPIO | Function |
|------|----------|
| 44 | Reset button (input — held = flip boot partition) |
| 53, 54, 55 | LEDs (output — all lit during U-Boot) |

### Hub V3

| GPIO | Function |
|------|----------|
| 122 (GPIO4_26) | Reset button (input — held = flip boot partition) |

---

## Pre-Built Binaries

U-Boot is not rebuilt from source during normal Yocto builds. Pre-built
binaries are stored in the build tool directories:

### Hub V2

| File | Purpose |
|------|---------|
| `build-ti/tools/u-boot-release.img` | Release U-Boot image |
| `build-ti/tools/MLO-release` | Release SPL |
| `build-ti/tools/uboot-debug/u-boot-debug.img` | Debug U-Boot (3s boot delay) |
| `build-ti/tools/uboot-debug/MLO-debug` | Debug SPL |

The `create_update_file` script copies from `uboot-debug/` for dev images.

### Hub V3

| File | Purpose |
|------|---------|
| `build-fsl/tools/u-boot-release.imx` | Release U-Boot (combined SPL+u-boot) |
| `build-fsl/tools/u-boot-release.imx.production-locked` | Production-locked variant |
| `build-fsl/tools/SPL-release` | Standalone SPL |

---

## Recipes and Patches

### Recipes

| File | Platform |
|------|----------|
| `meta-iris/recipes-bsp/u-boot/u-boot_2016.03.bb` | Hub V2 |
| `meta-iris/recipes-bsp/u-boot/u-boot-imxgs.bb` | Hub V3 |
| `meta-iris/recipes-bsp/u-boot/u-boot.inc` | Shared build rules |

### Hub V2 Patches

| Patch | Purpose |
|-------|---------|
| `0001-Iris-patches-2016.03.patch` | Core Iris customizations, A/B boot, GPIOs |
| `0007-Fix-Overflow-GCC8_1.patch` | GCC 8.1 overflow fix |

### Hub V3 Patches

| Patch | Purpose |
|-------|---------|
| `0001-imxdimagic-platform-support-2016.03.patch` | i.MX6DL board support |
| `0002-mx6imagic-IRIS-multi-partition-2016.03.patch` | A/B multi-partition boot |
| `0003-REV-B-DDR-Support.patch` | Micron DDR support |
| `0004-Fix-to-Samsung-K4B4G1646E-BYMA-DDR3-1866.patch` | Samsung DDR tuning |
| `0005-Lockdown-for-production.patch` | Silent console, zero boot delay |
| `0006-Update-to-Micron-DDR-settings.patch` | Updated Micron DDR timings |
