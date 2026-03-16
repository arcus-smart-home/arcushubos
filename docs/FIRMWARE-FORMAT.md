# Arcus Hub OS — Firmware File Format

This document describes the binary format of signed firmware images, the
signing/encryption pipeline used at build time, and the validation/decryption
process used on the hub at install time.

---

## Overview

There are two distinct firmware file formats:

1. **Unsigned archive** (`i2hubos_update.bin` / `i2hubosv3_update.bin`) — a plain
   `.tar.gz` containing the kernel, rootfs, radio firmware, and checksums. Used
   directly by `fwinstall` during development.

2. **Signed image** (`hubOS_X.Y.Z.NNN.bin` / `hubOSv3_X.Y.Z.NNN.bin`) — an
   RSA-signed, AES-encrypted wrapper around the unsigned archive. Used for
   production updates via the `update` command.

---

## Unsigned Archive Format

The unsigned archive is a gzip-compressed tar file containing an `update/`
directory:

```
update/
├── sha256sums.txt              # SHA-256 checksums of all files below
├── <kernel image>              # uImage (V2) or zImage (V3)
├── <device tree blob>          # am335x-boneblack.dtb or imx6dl-imagic.dtb
├── <rootfs squashfs>           # core-image-minimal-iris-*.squashfs
├── <bootloader files>          # MLO + u-boot.img (V2) or u-boot.imx (V3)
├── zwave-firmware.bin          # Z-Wave radio firmware
├── zigbee-firmware.bin         # Zigbee firmware (software flow control, V2 only)
├── zigbee-firmware-hwflow.bin  # Zigbee firmware (hardware flow control, V2 only)
├── ble-firmware.bin            # BLE firmware (software flow control, V2 only)
└── ble-firmware-hwflow.bin     # BLE firmware (hardware flow control)
```

### Hub V2 Contents

| File | Description |
|------|-------------|
| `MLO-beaglebone` | First-stage bootloader (from `tools/uboot-debug/`) |
| `u-boot-beaglebone.img` | U-Boot image (from `tools/uboot-debug/`) |
| `uImage-beaglebone.bin` | Linux kernel (uImage format) |
| `uImage-am335x-boneblack.dtb` | Device tree blob |
| `core-image-minimal-iris-beaglebone.squashfs` | Root filesystem |
| `zwave-firmware.bin` | ZM5304 firmware (converted from Intel HEX via `srec_cat`) |
| `zigbee-firmware.bin` | EM3587 XON/XOFF firmware |
| `zigbee-firmware-hwflow.bin` | EM3587 RTS/CTS firmware |
| `ble-firmware.bin` | CC2541 firmware (software flow control) |
| `ble-firmware-hwflow.bin` | CC2541 firmware (hardware flow control) |

### Hub V3 Contents

| File | Description |
|------|-------------|
| `u-boot-imxdimagic.imx` | U-Boot image (from `tools/u-boot-release.imx`) |
| `zImage-imxdimagic.bin` | Linux kernel (zImage format) |
| `zImage-imx6dl-imagic.dtb` | Device tree blob |
| `core-image-minimal-iris-imxdimagic.squashfs` | Root filesystem |
| `zwave-firmware.bin` | ZW050x firmware (converted from Intel HEX via `srec_cat`) |
| `ble-firmware-hwflow.bin` | Zephyr/MCUboot BLE firmware |

### Integrity

Every file in the archive is listed in `sha256sums.txt`. The `fwinstall` tool
runs `sha256sum -c sha256sums.txt` immediately after unpacking and aborts if
any checksum fails.

---

## Signed Image Format

The signed image is a binary file consisting of two concatenated parts:

```
┌─────────────────────────────────────┐
│  Signed Header (512 bytes)          │  RSA-4096 signature of fw_header_t
├─────────────────────────────────────┤
│  Encrypted Payload (variable size)  │  AES-128-CBC encrypted archive
└─────────────────────────────────────┘
```

### Signed Header (512 bytes)

The first 512 bytes are the RSA-4096 signature of the `fw_header_t` structure.
This is produced by:

```
openssl rsautl -sign -inkey iris-fwsign-nopass.key -in header.bin -out header.signed
```

The RSA-4096 signature output is exactly 512 bytes (`SIGNED_HDR_LEN`), which
is the raw RSA signature of the 198-byte `fw_header_t` struct (zero-padded to
the RSA block size by OpenSSL).

### fw_header_t Structure (198 bytes)

Defined in `build_image.h`. All multi-byte integers are in **network byte
order** (big-endian).

```
Offset  Size  Type       Field           Description
──────  ────  ─────────  ──────────────  ────────────────────────────────────
  0       2   uint16_t   header_version  Always 0 (HEADER_VERSION_0)
  2       2   uint16_t   reserved        Zero-filled
  4       4   uint32_t   image_size      Size of the original unencrypted archive
  8      24   char[]     fw_version      "v3.0.1.034" (from irisversion.h)
 32      16   char[]     fw_model        "IH200" (V2) or "IH304" (V3)
 48      16   char[]     fw_customer     "IRIS" or "ALL"
 64      66   char[]     fw_cksum        SHA-256 hex digest of unencrypted archive
130      34   char[]     fw_key          AES-128 key (32 hex chars + padding)
164      34   char[]     fw_iv           AES-128 IV (32 hex chars + padding)
```

The AES key and IV are **randomly generated** for each build (`openssl rand -hex 16`).
They are embedded in the header so the hub can decrypt the payload after verifying
the RSA signature. The header itself is RSA-signed, so the key material is
protected by the signature.

### Encrypted Payload

Everything after the first 512 bytes is the AES-128-CBC encrypted firmware
archive. Produced by:

```
openssl aes-128-cbc -in archive.bin -out image.encrypted -e -K <key> -iv <iv>
```

The key and IV used for encryption are stored in the signed header.

### Output Filename

The `build_image` tool names the output file using the version fields:

```
<prefix>_<MAJOR>.<MINOR>.<POINT>.<BUILD><SUFFIX>.bin
```

For example: `hubOS_3.0.1.034.bin`, `hubOSv3_3.0.1.034.bin`, `hubBL_3.0.1.034.bin`

---

## Signing Pipeline (Build Time)

The `build_image` tool runs on the build host as part of `create_update_file`.
Source: `meta-iris/recipes-core/iris-utils/files/build_image.c`

### Steps

1. **Compute SHA-256 checksum** of the unsigned archive
2. **Generate random AES-128 key and IV** (`openssl rand -hex 16` for each)
3. **Populate `fw_header_t`** with version, model, customer, checksum, key, and IV
4. **RSA-sign the header** using the private key (`iris-fwsign-nopass.key`)
   ```
   openssl rsautl -sign -inkey iris-fwsign-nopass.key -in header.bin -out header.signed
   ```
5. **AES-encrypt the archive**
   ```
   openssl aes-128-cbc -in archive.bin -out image.encrypted -e -K <key> -iv <iv>
   ```
6. **Concatenate** signed header (512 bytes) + encrypted payload into final `.bin`
7. **Clean up** temporary files (header.bin, header.signed, image.encrypted)

### Key Files

| File | Location | Purpose |
|------|----------|---------|
| `iris-fwsign-nopass.key` | `build-{ti,fsl}/tools/` and `meta-iris/.../iris-utils/files/` | RSA-4096 private key (no passphrase) |
| `iris-fwsign.pub` | `meta-iris/.../iris-utils/files/` → installed to `/etc/.ssh/` | RSA-4096 public key (on hub) |

### build_image Usage

```
build_image [options] firmware_tar
  -p prefix   Output filename prefix (hubOS, hubOSv3, hubBL, hubBLv3)
  -m model    Model name (default: IH200 or IH304)
  -c customer Customer field (default: IRIS, or ALL for any hub)
```

The `create_update_file` scripts invoke it as:
```bash
# Hub V2
$BUILD_IMAGE -p hubOS -c ALL ../tmp/deploy/images/beaglebone-yocto/i2hubos_update.bin

# Hub V3
$BUILD_IMAGE -p hubOSv3 -c ALL ../tmp/deploy/images/imxdimagic/i2hubosv3_update.bin
```

---

## Validation Pipeline (Hub Side)

### Step 1: Download (`update`)

The `update` command (`meta-iris/recipes-core/iris-utils/files/update.c`)
downloads or symlinks the signed image:

```
update [-f] [-k] [-c user:pass] <URL>
  -f    Force install (skip version check)
  -k    Kill agent before radio firmware install
  -c    Credentials for HTTP download
```

Supported URL schemes: `file://`, `http://`, `https://` (via curl), `tftp://`,
`ftp://`.

For local files, `update` creates a symlink to `/tmp/upload.bin` rather than
copying.

### Step 2: Validate (`validate_image`)

The `validate_image` tool (`meta-iris/recipes-core/iris-utils/files/validate_image.c`)
performs the cryptographic validation:

1. **Extract signed header** — read the first 512 bytes from the file
2. **RSA-verify the signature** using the public key on the hub:
   ```
   openssl rsautl -verify -in header.signed -pubin -inkey /etc/.ssh/iris-fwsign.pub -out header.verified
   ```
3. **Parse `fw_header_t`** from the verified (decrypted) header
4. **Check version** — skip install if firmware version matches current
   (`/tmp/version`), unless `-f` (force) or `-s` (skip) is used
5. **Check model** — compare first 4 characters of `fw_model` against
   `/tmp/mfg/config/model` (e.g., "IH20" matches both "IH200" and "IH204")
6. **Check customer** — must match `/tmp/mfg/config/customer`, or header
   customer must be `"ALL"`
7. **Decrypt payload** — extract bytes after offset 512 and decrypt:
   ```
   openssl aes-128-cbc -in firmware.encrypt -out /tmp/fw_image.bin -d -K <key> -iv <iv>
   ```
8. **Verify SHA-256 checksum** — compute SHA-256 of decrypted archive, compare
   against `fw_cksum` from the header

If any step fails, temporary files are removed and an error code is returned.

### Step 3: Install (`fwinstall`)

If validation succeeds, `update` calls `fwinstall` with the decrypted archive
(`/tmp/fw_image.bin`). See `RELEASE.md` for the A/B partition install process.

### LED Feedback During Validation

| LED Mode | Phase |
|----------|-------|
| `upgrade-decrypt` | Decrypting and validating |
| `upgrade-decrypt-err` | Validation or decryption failed |

---

## Error Codes

Defined in `irisdefs.h`, returned by `validate_image` and `fwinstall`:

### Validation Errors (1-13)

| Code | Name | Meaning |
|------|------|---------|
| 1 | `INSTALL_ARG_ERR` | Bad command-line arguments |
| 2 | `INSTALL_DOWNLOAD_ERR` | Failed to download firmware file |
| 3 | `INSTALL_FILE_OPEN_ERR` | Cannot open firmware file |
| 4 | `INSTALL_MEMORY_ERR` | Memory mapping failed |
| 5 | `INSTALL_DECRYPT_ERR` | General decryption/header error |
| 6 | `INSTALL_HDR_SIG_ERR` | RSA signature verification failed |
| 7 | `INSTALL_MODEL_ERR` | Model mismatch |
| 8 | `INSTALL_CUSTOMER_ERR` | Customer mismatch |
| 9 | `INSTALL_FW_DECRYPT_ERR` | AES decryption failed |
| 10 | `INSTALL_CKSUM_ERR` | SHA-256 checksum mismatch after decryption |
| 11 | `INSTALL_IN_PROCESS` | Another install is already running |
| 12 | `INSTALL_UNPACK_ERR` | Failed to untar the archive |
| 13 | `INSTALL_ARCHIVE_CKSUM_ERR` | Archive internal sha256sums.txt check failed |

### Installation Errors (14-23)

| Code | Name | Meaning |
|------|------|---------|
| 14 | `INSTALL_MOUNT_ERR` | Failed to mount kernel partition |
| 15 | `INSTALL_ZWAVE_ERR` | Z-Wave firmware flash failed |
| 16 | `INSTALL_ZIGBEE_ERR` | Zigbee firmware flash failed |
| 17 | `INSTALL_BLE_ERR` | BLE firmware flash failed |
| 21 | `INSTALL_UBOOT_ERR` | U-Boot install/verify failed |
| 22 | `INSTALL_KERNEL_ERR` | Kernel install/verify failed |
| 23 | `INSTALL_ROOTFS_ERR` | Root filesystem install/verify failed |

Errors >= 20 (`INSTALL_HARD_ERRORS`) are considered hard errors — the hub
holds the error LED pattern for 10 seconds before rebooting.

---

## Security Model

### What is Protected

- **Authenticity:** The RSA-4096 signature ensures the firmware was produced by
  someone holding the private key. The hub verifies using the public key baked
  into the image at `/etc/.ssh/iris-fwsign.pub`.
- **Integrity:** The SHA-256 checksum in the signed header is verified after
  decryption, ensuring the archive was not tampered with.
- **Confidentiality:** AES-128-CBC encryption prevents casual inspection of
  the firmware contents in transit.

### Key Material

- **Private key** (`iris-fwsign-nopass.key`): 4096-bit RSA, no passphrase.
  Lives on the build host only. Used to sign headers at build time.
- **Public key** (`iris-fwsign.pub`): Installed to `/etc/.ssh/` on the hub
  at image build time. Used to verify signatures. Also used to verify debug
  dongle identity files.
- **AES key/IV**: Randomly generated per build, embedded in the signed header.
  Unique to each firmware image — compromising one image's key does not affect
  others.

### Dev vs Release Path

| Path | Signature | Encryption | Validation |
|------|-----------|------------|------------|
| `fwinstall` (dev) | None | None | SHA-256 checksums only (`sha256sums.txt`) |
| `update` → `validate_image` → `fwinstall` (release) | RSA-4096 | AES-128-CBC | Full: signature + model + customer + decrypt + SHA-256 |

Dev images bypass the entire signing/encryption layer. The `fwinstall` tool
only verifies the internal `sha256sums.txt` within the archive.

---

## Full Update Flow Diagram

```
Build Host                              Hub
──────────                              ───

1. bitbake builds image
2. create_update_file:
   - assemble update/ dir
   - sha256sum → sha256sums.txt
   - tar czf → i2hubos_update.bin
3. build_image:
   - sha256sum(archive)
   - openssl rand → key, iv
   - populate fw_header_t
   - openssl rsautl -sign → header
   - openssl aes-128-cbc -e → payload
   - cat header payload → hubOS_*.bin

                    ── scp/curl/tftp ──►

                                        4. update:
                                           - download/symlink file
                                        5. validate_image:
                                           - extract header (512 bytes)
                                           - openssl rsautl -verify
                                           - check version/model/customer
                                           - openssl aes-128-cbc -d
                                           - sha256sum verify
                                           - output: /tmp/fw_image.bin
                                        6. fwinstall:
                                           - untar /tmp/fw_image.bin
                                           - sha256sum -c sha256sums.txt
                                           - install to inactive A/B slot
                                           - update bootindex
                                           - reboot
```

---

## Source Files

| File | Purpose |
|------|---------|
| `meta-iris/recipes-core/iris-utils/files/build_image.h` | `fw_header_t` structure definition |
| `meta-iris/recipes-core/iris-utils/files/build_image.c` | Build-time signing/encryption tool |
| `meta-iris/recipes-core/iris-utils/files/validate_image.c` | Hub-side signature verification and decryption |
| `meta-iris/recipes-core/iris-utils/files/update.c` | Download and orchestrate validate → install |
| `meta-iris/recipes-core/iris-utils/files/fwinstall.c` | Archive unpacking and A/B partition installer |
| `meta-iris/recipes-core/iris-utils/files/firmware_install` | Shell helper that routes to fwinstall or update |
| `meta-iris/recipes-core/iris-lib/files/irisdefs.h` | Error codes, file paths, constants |
| `meta-iris/recipes-core/iris-lib/files/irisversion.h` | Version numbers (MAJOR/MINOR/POINT/BUILD) |
| `build-{ti,fsl}/tools/create_update_file` | Assembles the unsigned archive |
| `build-{ti,fsl}/tools/create_update_file_bootloader` | Same, but for bootloader-inclusive archives |
