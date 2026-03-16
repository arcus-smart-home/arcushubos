# Arcus Hub OS — Release Process

End-to-end guide for building, signing, and installing hub firmware.

## Prerequisites

- Linux build host (x86_64) with standard Yocto Scarthgap host packages
- `/tftpboot/` directory (build scripts copy outputs here)
- Submodules initialized:
  ```
  git submodule update --init --recursive
  ```
- Signing key `iris-fwsign-nopass.key` in `build-{ti,fsl}/tools/` (required for signed release builds)
- Optional: preserved `sstate-cache` from a prior build to speed up rebuilds

## Version Bumping

Firmware version is defined in `meta-iris/recipes-core/iris-lib/files/irisversion.h`:

```c
#define VERSION_MAJOR 3
#define VERSION_MINOR 0
#define VERSION_POINT 1
#define VERSION_BUILD    34
#define VERSION_SUFFIX ""
```

Increment `VERSION_BUILD` for each release. The signed output filename is derived from these fields:
`hubOS_<MAJOR>.<MINOR>.<POINT>.<BUILD>.bin`

Agent version is defined in `meta-iris/recipes-core/iris-agent/iris-agent.bb`:

```
AGENT_VERSION="2.13.26-prod"
SRC_URI[sha256sum] = "66685ff6..."
```

When updating the agent, change `AGENT_VERSION` and the corresponding `sha256sum`.

## Building

### Make Targets

| Target      | Platform            | Image Type | Build Dir    |
|-------------|---------------------|------------|--------------|
| `hubv2`     | Hub V2 (AM335x)     | Release    | `build-ti/`  |
| `hubv2-dev` | Hub V2 (AM335x)     | Dev        | `build-ti/`  |
| `hubv3`     | Hub V3 (i.MX6DL)    | Release    | `build-fsl/` |
| `hubv3-dev` | Hub V3 (i.MX6DL)    | Dev        | `build-fsl/` |

Build with:

```
make hubv2        # Release build for Hub V2
make hubv3-dev    # Dev build for Hub V3
```

### What Happens Under the Hood

1. `oe-init-build-env` sets up the Yocto environment using the appropriate `BDIR`
2. `bitbake core-image-minimal-iris` (or `-dev`) builds the image
3. `create_update_file` assembles the update archive:
   - Copies kernel, DTB, squashfs rootfs, and radio firmware into an `update/` directory
   - Generates `sha256sums.txt` for integrity checking
   - Tars everything into `i2hubos_update.bin` (V2) or `i2hubosv3_update.bin` (V3)
   - Runs `build_image` to create the signed release file
4. `create_update_file_bootloader` does the same but includes the bootloader (for bootloader-only updates)

### Signing Pipeline (`build_image`)

The `build_image` tool (built from `meta-iris/recipes-core/iris-utils/files/build_image.c`):

1. Creates a firmware header containing version, model (`IH200` for V2, `IH304` for V3), customer, SHA-256 checksum
2. Generates a random AES-128-CBC key and IV
3. RSA-signs the header using `iris-fwsign-nopass.key`
4. AES-encrypts the firmware tarball
5. Concatenates signed header + encrypted image into the final `.bin` file

## Build Outputs

### Hub V2 (`build-ti/`)

| File | Description |
|------|-------------|
| `build-ti/tmp/deploy/images/beaglebone-yocto/i2hubos_update.bin` | Unsigned dev archive |
| `build-ti/tools/hubOS_X.Y.Z.NNN.bin` | Signed release image |
| `build-ti/tools/hubBL_X.Y.Z.NNN.bin` | Signed bootloader image |

### Hub V3 (`build-fsl/`)

| File | Description |
|------|-------------|
| `build-fsl/tmp/deploy/images/imxdimagic/i2hubosv3_update.bin` | Unsigned dev archive |
| `build-fsl/tools/hubOSv3_X.Y.Z.NNN.bin` | Signed release image |
| `build-fsl/tools/hubBLv3_X.Y.Z.NNN.bin` | Signed bootloader image |

All outputs are also copied to `/tftpboot/`.

### Update Archive Contents

**Hub V2** (`i2hubos_update.bin`):
- `MLO-beaglebone`, `u-boot-beaglebone.img` — U-Boot (from `build-ti/tools/uboot-debug/`)
- `uImage-beaglebone.bin`, `uImage-am335x-boneblack.dtb` — kernel + DTB
- `core-image-minimal-iris-beaglebone.squashfs` — root filesystem
- `zwave-firmware.bin` — Z-Wave (ZM5304)
- `zigbee-firmware.bin`, `zigbee-firmware-hwflow.bin` — Zigbee (EM3587)
- `ble-firmware.bin`, `ble-firmware-hwflow.bin` — BLE (CC2541)
- `sha256sums.txt`

**Hub V3** (`i2hubosv3_update.bin`):
- `u-boot-imxdimagic.imx` — U-Boot (from `build-fsl/tools/u-boot-release.imx`)
- `zImage-imxdimagic.bin`, `zImage-imx6dl-imagic.dtb` — kernel + DTB
- `core-image-minimal-iris-imxdimagic.squashfs` — root filesystem
- `zwave-firmware.bin` — Z-Wave (ZW050x)
- `ble-firmware-hwflow.bin` — BLE (Zephyr/MCUboot)
- `sha256sums.txt`

## Installing on Hardware

### Dev Workflow (Unsigned)

Use `fwinstall` directly with the unsigned archive. This skips signature validation:

```bash
# From build host:
scp build-ti/tmp/deploy/images/beaglebone-yocto/i2hubos_update.bin root@<hub-ip>:/tmp/

# On the hub:
fwinstall -k /tmp/i2hubos_update.bin
```

Options: `-k` kills the agent before radio firmware install, `-s` skips radio firmware, `-n` skips write verification.

### Release Workflow (Signed)

Use the `update` command which validates the signed header before passing to `fwinstall`:

```bash
# From build host:
scp build-ti/tools/hubOS_3.0.1.034.bin root@<hub-ip>:/tmp/

# On the hub:
update -kf file:///tmp/hubOS_3.0.1.034.bin
```

Or use the `firmware_install` helper script which auto-detects the file type:

```bash
firmware_install /tmp/hubOS_3.0.1.034.bin
```

### A/B Partition Scheme

The hub uses an A/B update scheme with two kernel+rootfs partition pairs:

- `/dev/kern1` + `/dev/fs1` — Slot A
- `/dev/kern2` + `/dev/fs2` — Slot B

Each slot has a `bootindex` file. `fwinstall` always writes to the **older** slot (lower bootindex) and increments the bootindex on success, so the next boot uses the new image.

**Automatic rollback:** If the newly written partition is corrupt at boot, U-Boot falls back to the other slot. `fwinstall` also detects if the current boot partition doesn't match the highest bootindex (indicating the last update was corrupt) and uses that slot for the next install.

## Release vs Dev Images

Both images are defined in `meta-iris/recipes-core/images/`:

| Feature | Release (`core-image-minimal-iris`) | Dev (`core-image-minimal-iris-dev`) |
|---------|-------------------------------------|-------------------------------------|
| Base packages | dropbear, iris-agent, radios, OpenJDK, etc. | Same as release |
| Dev tools | None | gcc, g++, make, cmake, python3, git, go, perl, etc. |
| SSH access | Debug dongle or `ssh_enabled` only | Always available |
| Root password | Set (see below) | Same password |
| Rootfs | Read-only squashfs | Read-only squashfs |

Both images use a read-only squashfs rootfs. Persistent data lives on `/data/`.

## SSH Access

### Dev Images

SSH (dropbear) is always enabled with password authentication.

### Release Images

SSH is **disabled by default**. It can be enabled by:

1. **Debug dongle** — inserting a USB debug dongle starts SSH with password auth, and sets the debug mode flag.

2. **Persistent key-only access** — create the enable file and add your public key:
   ```bash
   mkdir -p /data/config/dropbear
   touch /data/config/dropbear/ssh_enabled
   cat > /data/config/dropbear/authorized_keys << 'EOF'
   ssh-ed25519 AAAA... user@host
   EOF
   ```
   This starts SSH in key-only mode (no password auth). If `ssh_enabled` exists without `authorized_keys`, password auth is used instead.

3. **Mode transitions** — the `irisinitd` daemon monitors for dongle insertion/removal and `ssh_enabled` file changes, automatically restarting SSH with the appropriate mode.

### Default Root Password

Both release and dev images use the same root password, set in the image recipe via `openssl passwd -1`.

## Agent Updates

The hub agent (Java) is separate from the firmware and managed independently:

- **Agent version** is defined in `iris-agent.bb` and bundled into the firmware image at build time
- **On first boot** (or after `agent_reinstall`), the agent tarball from `/home/agent/iris-agent-hub` is extracted to `/data/agent/`
- **`fwinstall` does NOT update the agent** — it only updates the OS, kernel, and radio firmware
- **To force an agent reinstall** after a firmware update:
  ```bash
  agent_reinstall    # Removes /data/agent, reboots, agent re-extracts from image
  ```
- Other agent management scripts in `/home/root/bin/`:
  - `agent_start` / `agent_stop` — start/stop the agent
  - `agent_install` — install agent from a file
  - `agent_reset` — reset agent data

## Testing Checklist

After building and installing a release, verify:

- [ ] Hub boots successfully and agent starts
- [ ] LED patterns are correct during boot sequence
- [ ] Z-Wave radio responds (check with `zwave_flash` or pair a device)
- [ ] Zigbee radio responds (check with `zigbee_flash` or pair a device)
- [ ] BLE radio responds
- [ ] SSH access works as expected for the image type
- [ ] A/B failover works (install, verify boot to new partition, verify old partition still bootable)
- [ ] Firmware version reported correctly (`cat /tmp/version` or check agent logs)
- [ ] Hub V3 only: WiFi connects, audio/buzzer works, 4G modem if applicable

See `UPGRADE.md` for additional Scarthgap-specific testing notes.
