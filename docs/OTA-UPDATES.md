# Arcus Hub OS — OTA Update Flow

This document covers the complete over-the-air firmware update pipeline, from
download through installation and reboot.

---

## Overview

Updates are **agent-driven** (pull model). The hub agent checks for available
firmware, downloads it, and triggers the update pipeline. The system uses A/B
partitioning so a failed update can be rolled back by booting the previous
partition.

```
Agent triggers update
    → download (curl/wget/tftp)
    → validate_image (RSA verify + AES decrypt + SHA256 check)
    → fwinstall (A/B partition installer)
    → reboot
```

---

## Update Command

```bash
update [options] URL
  -h              Print help
  -c user:passwd  HTTP Basic Auth credentials
  -f              Force install even if version matches
  -k              Kill agent before installing radio firmware
```

Supported URL schemes: `https://`, `http://`, `tftp://`, `ftp://`, `file://`

For local files (dev workflow):
```bash
update -kf file:///tmp/hubOS_3.0.1.034.bin
```

---

## Pipeline Stages

### 1. Download

The firmware image is downloaded to `/tmp/upload.bin`:

```bash
# HTTPS (primary method)
curl -s -k -u user:pass https://server/hubOS_3.0.1.034.bin -o /tmp/upload.bin

# TFTP (alternative)
tftp -g -r hubOS_3.0.1.034.bin -l /tmp/upload.bin server
```

Exit code `INSTALL_DOWNLOAD_ERR` (2) on failure.

### 2. Validation (`validate_image`)

The signed firmware image is verified and decrypted:

| Step | Action | Error Code |
|------|--------|------------|
| 1 | Extract 512-byte signed header | `INSTALL_DECRYPT_ERR` (5) |
| 2 | RSA-4096 signature verify against `/etc/.ssh/iris-fwsign.pub` | `INSTALL_HDR_SIG_ERR` (6) |
| 3 | Read decrypted header (version, model, customer, AES key/IV, SHA256) | `INSTALL_DECRYPT_ERR` (5) |
| 4 | Check firmware version against `/tmp/version` (skip with `-f`) | Same version = skip (not error) |
| 5 | Check model matches `/tmp/mfg/config/model` (first 4 chars) | `INSTALL_MODEL_ERR` (7) |
| 6 | Check customer matches `/tmp/mfg/config/customer` (or "ALL") | `INSTALL_CUSTOMER_ERR` (8) |
| 7 | Decrypt payload with AES-128-CBC using key/IV from header | `INSTALL_FW_DECRYPT_ERR` (9) |
| 8 | Verify SHA-256 checksum of decrypted image | `INSTALL_CKSUM_ERR` (10) |

Output: decrypted firmware archive at `/tmp/fw_image.bin`.

See [FIRMWARE-FORMAT.md](FIRMWARE-FORMAT.md) for details on the image format
and signing pipeline.

### 3. Installation (`fwinstall`)

`fwinstall` takes the decrypted archive and installs it to the inactive A/B
partition. A lock file (`/var/lock/fwinstall`) prevents parallel installs.

#### a. Unpack

Extracts `/tmp/fw_image.bin` (tar.gz) to `/tmp/update/` and verifies internal
SHA-256 checksums (`sha256sums.txt`).

#### b. Select Target Partition

```
Mount kernel partitions A and B
Read bootindex from each
Determine which partition is currently running (via /proc/cmdline)
Install to the OTHER partition
Set updateindex = max(index1, index2) + 1
```

#### c. Install Bootloader

| Platform | Method |
|----------|--------|
| Hub V2 | Mount FAT partition (mmcblk0p1), copy MLO + u-boot.img |
| Hub V3 | `dd if=u-boot.imx of=/dev/mmcblk2boot0 seek=2 bs=512` |

Only installs if different from current. Verified after write.

#### d. Install Kernel + Device Tree

Copies kernel image and DTB files to the target kernel partition. Only installs
if different.

#### e. Install Root Filesystem

Writes squashfs image to the target rootfs partition. Uses block-by-block
comparison to skip writing unchanged regions. Verified after write.

#### f. Install Radio Firmware

| Radio | Tool | Firmware File |
|-------|------|---------------|
| Z-Wave | `zwave_flash -w` | `zwave-firmware.bin` |
| Zigbee | `zigbee_flash -w` | `zigbee-firmware-hwflow.bin` |
| BLE | `ble_prog` | `ble-firmware-hwflow.bin` |

Radio firmware is stored in `/data/firmware/` and only re-flashed if the new
version differs. The `-k` flag kills the agent before radio flashing to release
the serial ports.

#### g. Update Bootindex

Writes `updateindex` to the target kernel partition's `bootindex` file. On next
boot, U-Boot reads both bootindex files and selects the partition with the
higher value — which will be the newly installed firmware.

---

## Error Handling and Rollback

### Soft Errors (< 20)

Non-fatal errors that leave the system bootable on the current partition:

| Code | Name | Cause |
|------|------|-------|
| 2 | `INSTALL_DOWNLOAD_ERR` | Download failure |
| 5 | `INSTALL_DECRYPT_ERR` | Header/file I/O error |
| 6 | `INSTALL_HDR_SIG_ERR` | RSA signature invalid |
| 7 | `INSTALL_MODEL_ERR` | Model mismatch |
| 8 | `INSTALL_CUSTOMER_ERR` | Customer mismatch |
| 9 | `INSTALL_FW_DECRYPT_ERR` | AES decryption failure |
| 10 | `INSTALL_CKSUM_ERR` | SHA-256 mismatch |
| 11 | `INSTALL_IN_PROCESS` | Another install already running |
| 12 | `INSTALL_UNPACK_ERR` | tar extraction failure |
| 13 | `INSTALL_ARCHIVE_CKSUM_ERR` | Archive checksum failure |
| 14 | `INSTALL_MOUNT_ERR` | Partition mount failure |
| 15 | `INSTALL_ZWAVE_ERR` | Z-Wave flash failure |
| 16 | `INSTALL_ZIGBEE_ERR` | Zigbee flash failure |
| 17 | `INSTALL_BLE_ERR` | BLE flash failure |

### Hard Errors (>= 20)

Fatal errors that trigger an automatic reboot. The bootindex is NOT updated,
so the hub reboots into the **previous (known-good) partition**:

| Code | Name | Cause |
|------|------|-------|
| 21 | `INSTALL_UBOOT_ERR` | U-Boot install/verify failure |
| 22 | `INSTALL_KERNEL_ERR` | Kernel install/verify failure |
| 23 | `INSTALL_ROOTFS_ERR` | Rootfs install/verify failure |

### Rollback Mechanism

If a hard error occurs during installation:
1. The bootindex file is NOT written to the target partition
2. The hub reboots automatically after a 10-second LED error display
3. U-Boot selects the previous partition (unchanged, higher bootindex)
4. The hub boots the old firmware successfully

If the new firmware boots but the agent crashes repeatedly, `irisagentd`
handles escalating recovery (see [HARDWARE.md](HARDWARE.md#watchdog-architecture)).

---

## LED Indicators During Update

| LED Mode | Stage |
|----------|-------|
| `upgrade-decrypt` | Validating and decrypting firmware |
| `upgrade-decrypt-err` | Validation or decryption failed |
| `upgrade-unpack` | Unpacking firmware archive |
| `upgrade-bootloader` | Installing U-Boot |
| `upgrade-bootloader-err` | U-Boot install failed |
| `upgrade-kernel` | Installing kernel |
| `upgrade-kernel-err` | Kernel install failed |
| `upgrade-rootfs-err` | Rootfs install failed |
| `upgrade-zwave` | Flashing Z-Wave radio |
| `upgrade-zwave-err` | Z-Wave flash failed |
| `upgrade-zigbee` | Flashing Zigbee radio |
| `upgrade-zigbee-err` | Zigbee flash failed |
| `upgrade-bte` | Flashing BLE radio |
| `upgrade-bte-err` | BLE flash failed |

---

## Disabling Automatic Updates

Create this file to skip automatic update checks:

```bash
touch /data/config/update_skip
```

---

## Temporary Files

All temporary files are in `/tmp` (tmpfs) and cleaned up after install:

| Path | Contents |
|------|----------|
| `/tmp/upload.bin` | Downloaded firmware image |
| `/tmp/header.signed` | Extracted signed header (512 bytes) |
| `/tmp/header.verified` | Decrypted header |
| `/tmp/firmware.encrypt` | Encrypted payload |
| `/tmp/fw_image.bin` | Decrypted firmware archive |
| `/tmp/update/` | Unpacked archive contents |
| `/tmp/kernel1/`, `/tmp/kernel2/` | Mounted kernel partitions |
| `/tmp/bootupdate/` | Mounted boot partition (V2 only) |
| `/var/lock/fwinstall` | Installation lock file |

---

## Key Source Files

| File | Purpose |
|------|---------|
| `meta-iris/recipes-core/iris-utils/files/update.c` | Download + orchestration |
| `meta-iris/recipes-core/iris-utils/files/validate_image.c` | RSA verify + AES decrypt + checksum |
| `meta-iris/recipes-core/iris-utils/files/fwinstall.c` | A/B partition installer |
| `meta-iris/recipes-core/iris-utils/files/build_image.c` | Build-time signing tool |
| `meta-iris/recipes-core/iris-lib/files/irisdefs.h` | Error codes, path constants |
