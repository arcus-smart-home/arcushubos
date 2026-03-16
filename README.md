# Arcus Hub OS

Yocto-based Linux distribution for the Arcus smart home hub, supporting two
hardware platforms:

- **Hub V2** — TI AM335x (BeagleBone Black), ARM Cortex-A8, 512 MiB RAM
- **Hub V3** — NXP i.MX6DualLite, ARM Cortex-A9 dual-core, 1 GiB RAM

Currently built on **Yocto 5.0 "Scarthgap"** (LTS) with kernel 6.6.

## Quick Start

### Prerequisites

Use a Yocto Scarthgap supported host (Ubuntu 22.04+, Debian 12+, Fedora 38+,
or equivalent). Install the required packages:

```bash
sudo apt-get install gawk wget git diffstat unzip texinfo gcc-multilib \
    build-essential chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping libsdl1.2-dev xterm lz4 zstd \
    openjdk-8-jdk srecord
```

Create the output directory (build scripts copy firmware images here):

```bash
sudo mkdir -p /tftpboot
sudo chmod 777 /tftpboot
```

Optionally, create a shared sstate-cache to speed up rebuilds:

```bash
sudo mkdir -p /build/sstate-cache
sudo chmod 777 /build/sstate-cache
```

### Clone and Initialize

```bash
git clone <repo-url> arcushubos
cd arcushubos
git submodule update --init --recursive
```

### Build

```bash
make hubv2       # Release image for Hub V2
make hubv2-dev   # Dev image for Hub V2 (includes gcc, python3, etc.)
make hubv3       # Release image for Hub V3
make hubv3-dev   # Dev image for Hub V3
```

First builds take an hour or more (cross-compiler toolchains + all packages).
Subsequent builds with sstate-cache are much faster.

### Build Outputs

| File | Description |
|------|-------------|
| `i2hubos_update.bin` | V2 unsigned dev archive |
| `hubOS_X.Y.Z.NNN.bin` | V2 signed release image |
| `hubBL_X.Y.Z.NNN.bin` | V2 signed image with bootloader |
| `i2hubosv3_update.bin` | V3 unsigned dev archive |
| `hubOSv3_X.Y.Z.NNN.bin` | V3 signed release image |
| `hubBLv3_X.Y.Z.NNN.bin` | V3 signed image with bootloader |

Outputs are in `build-ti/tmp/deploy/images/` (V2), `build-fsl/tmp/deploy/images/`
(V3), and copied to `/tftpboot/`.

## Installing Firmware

### Dev workflow (unsigned, faster)

```bash
scp build-ti/tmp/deploy/images/beaglebone-yocto/i2hubos_update.bin root@<hub-ip>:/tmp/
ssh root@<hub-ip> "fwinstall -k /tmp/i2hubos_update.bin"
```

### Release workflow (signed, validated)

```bash
scp build-ti/tools/hubOS_3.0.1.034.bin root@<hub-ip>:/tmp/
ssh root@<hub-ip> "update -kf file:///tmp/hubOS_3.0.1.034.bin"
```

Reboot the hub after install to boot into the new firmware.

## Common Tasks

### Bumping the firmware version

Edit `VERSION_BUILD` in
[meta-iris/recipes-core/iris-lib/files/irisversion.h](meta-iris/recipes-core/iris-lib/files/irisversion.h).

### Updating the agent

Change `AGENT_VERSION` and `SRC_URI[sha256sum]` in
[meta-iris/recipes-core/iris-agent/iris-agent.bb](meta-iris/recipes-core/iris-agent/iris-agent.bb).

### Clean builds

```bash
make hubv2-clean      # Remove build-ti/tmp, cache, sstate-cache
make hubv2-distclean  # Above + remove downloads
```

## SSH Access

Dev images have SSH (dropbear) always enabled. Release images require either a
debug dongle or persistent key-only SSH — see [docs/RELEASE.md](docs/RELEASE.md)
for setup instructions.

## Documentation

Detailed documentation is in the [docs/](docs/) directory:

- [**RELEASE.md**](docs/RELEASE.md) — Release process: version bumping, building, signing, installing, testing
- [**HARDWARE.md**](docs/HARDWARE.md) — Hardware reference for both platforms: SoC, radios, GPIOs, peripherals, power management
- [**UBOOT.md**](docs/UBOOT.md) — U-Boot bootloader: A/B boot selection, environment, patches, installation
- [**SYSTEM.md**](docs/SYSTEM.md) — System daemons, utilities, radio flash tools, key source files
- [**OTA-UPDATES.md**](docs/OTA-UPDATES.md) — OTA update flow: download, validation, A/B installation, rollback
- [**DATA-PARTITION.md**](docs/DATA-PARTITION.md) — /data partition layout, persistence behavior, factory default
- [**FIRMWARE-FORMAT.md**](docs/FIRMWARE-FORMAT.md) — Firmware file format, signing/encryption pipeline, validation flow
- [**UPGRADE.md**](docs/UPGRADE.md) — Yocto Scarthgap migration notes

## Repository Structure

```
arcushubos/
├── meta-iris/          # Arcus-specific Yocto layer (recipes, patches, configs)
├── poky/               # Yocto core (submodule)
├── meta-openembedded/  # OE community layers (submodule)
├── meta-freescale/     # NXP/Freescale BSP layer (submodule)
├── build-ti/           # Hub V2 build directory
├── build-fsl/          # Hub V3 build directory
├── radios/             # Radio firmware binaries (Z-Wave, Zigbee, BLE)
├── docs/               # Project documentation
└── Makefile            # Top-level build targets
```

## Default Passwords

The current SSH root password for images built from these sources is `3XSgE27w5VJ3qvxK33dn`.

The default root password for Iris hubOS 3.x releases, regardless of hardware revision or platform is `zm{[*f6gB5X($]R9`.

The earlier 2.x releases used a default password of `kz58!~Eb.RZ?+bqb`.

## License

Licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.
