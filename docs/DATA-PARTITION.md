# Arcus Hub OS — /data Partition

The `/data` partition provides persistent storage that survives both firmware
updates and reboots. It is the only read-write partition besides `/tmp`.

---

## Device and Format

| | Hub V2 | Hub V3 |
|---|--------|--------|
| Device | `/dev/mmcblk0p8` | `/dev/mmcblk2p7` |
| Symlink | `/dev/data` | `/dev/data` |
| Filesystem | ext4 | ext4 |
| Format options | `stripe-width=4096` (TRIM support) | same |

At boot, `irisinit` runs `e2fsck -p` on `/data`. If the filesystem is badly
damaged (exit code > 2), it reformats with:

```bash
mkfs.ext4 -q -F -E stripe-width=4096 -L data /dev/data
```

A cron job periodically runs `fstrim -v /data` to issue TRIM commands to the
eMMC controller.

---

## Directory Layout

```
/data/
├── config/                         Persistent configuration
│   ├── timestamp                   Last system timestamp (symlinked to /etc/timestamp)
│   ├── timezone                    Timezone (default "UTC")
│   ├── localtime                   Symlink to timezone file
│   ├── wpa_supplicant.conf         WiFi config (V3 only)
│   ├── wifiCfg                     WiFi provisioning state (V3 only)
│   ├── provisioned                 Provisioning complete marker
│   ├── update_skip                 If present, disables automatic update checks
│   ├── max_voltage                 Battery voltage tracking
│   ├── disable_console             Toggles SSH console login (V2)
│   ├── enable_console              Toggles SSH console login (V3)
│   └── dropbear/
│       ├── dropbear_rsa_host_key   Generated at first boot
│       ├── ssh_enabled             If present, enables persistent SSH
│       └── authorized_keys         Public keys for key-only SSH auth
│
├── firmware/                       Radio firmware binaries
│   ├── zigbee-firmware-hwflow.bin
│   ├── zwave-firmware.bin
│   ├── ble-firmware.bin
│   └── ble-firmware-hwflow.bin
│
├── agent/                          Java agent runtime
│   ├── bin/
│   │   └── iris-agent              Agent launch script
│   ├── lib/                        Agent libraries
│   ├── conf/
│   │   ├── voice/                  Audio files (V3 only)
│   │   ├── sounds/                 Alert sounds
│   │   └── logrotate.conf          Agent log rotation
│   ├── .soft_reset                 Marker: agent does config reset on next start
│   ├── .factory_default            Marker: agent does factory default on next start
│   └── lastAgentCksum.txt          Agent binary integrity checksum
│
├── iris/                           Agent database and state
│   └── (device pairings, user settings, agent database)
│
├── log/                            System logs
│   ├── messages*                   Syslog (with rotation)
│   └── dmesg*                      Kernel log (with rotation)
│
└── jre/libs/                       Java runtime libraries (currently unused)
```

The `config/`, `firmware/`, and `log/` directories are created at boot by the
`irisinit` scripts (chmod 0777). The `agent/` directory is populated when the
agent is first installed from `/home/agent/iris-agent-hub`.

---

## Persistence Behavior

### Firmware Updates

`/data` **survives firmware updates** completely untouched. The A/B update
system only writes to kernel and rootfs partitions — `/data` is a separate
partition. Radio firmware in `/data/firmware/` is compared and updated only if
the new build includes different radio binaries.

### Factory Default

Factory default **reformats the entire partition** but preserves radio firmware:

1. Copy `/data/firmware/*` to `/tmp/firmware/`
2. Kill all processes accessing `/data` (up to 50 retries)
3. Unmount `/data`
4. `mkfs.ext4 -q -F -E stripe-width=4096 -L data /dev/data`
5. Remount `/data`
6. Restore `/tmp/firmware/*` back to `/data/firmware/`

| Data | Survives factory default? |
|------|---------------------------|
| `/data/firmware/` | Yes (saved and restored) |
| `/data/config/` | No |
| `/data/agent/` | No |
| `/data/iris/` | No |
| `/data/log/` | No |
| `/data/config/dropbear/` | No (SSH keys regenerated on next boot) |

### Reboot

Everything in `/data` survives a normal reboot.

---

## Key Files

| File | Purpose |
|------|---------|
| `meta-iris/recipes-core/iris-utils/files/irisinit-ti` | V2 mount, fsck, directory creation |
| `meta-iris/recipes-core/iris-utils/files/irisinit-imxdimagic` | V3 mount, fsck, directory creation |
| `meta-iris/recipes-core/iris-utils/files/factory_default.c` | Factory reset with firmware preservation |
| `meta-iris/recipes-core/iris-lib/files/irisdefs.h` | All `/data` path constants |
| `meta-iris/recipes-core/iris-agent/files/irisagent` | Agent installation to `/data/agent/` |
| `meta-iris/recipes-extended/dropbear/files/init` | SSH key generation in `/data/config/dropbear/` |
