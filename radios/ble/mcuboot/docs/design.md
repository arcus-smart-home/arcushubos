<!--
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing,
  software distributed under the License is distributed on an
  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
  KIND, either express or implied.  See the License for the
  specific language governing permissions and limitations
  under the License.
-->

# Boot Loader

## Summary

mcuboot comprises two packages:

* The bootutil library (boot/bootutil)
* The boot application (each port has its own at boot/<port>)

The bootutil library performs most of the functions of a boot loader.  In
particular, the piece that is missing is the final step of actually jumping to
the main image.  This last step is instead implemented by the boot application.
Boot loader functionality is separated in this manner to enable unit testing of
the boot loader.  A library can be unit tested, but an application can't.
Therefore, functionality is delegated to the bootutil library when possible.

## Limitations

The boot loader currently only supports images with the following
characteristics:
* Built to run from flash.
* Built to run from a fixed location (i.e., not position-independent).

## Image Format

The following definitions describe the image format.

``` c
#define IMAGE_MAGIC                 0x96f3b83d

#define IMAGE_HEADER_SIZE           32

struct image_version {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

/** Image header.  All fields are in little endian byte order. */
struct image_header {
    uint32_t ih_magic;
    uint32_t ih_load_addr;
    uint16_t ih_hdr_size; /* Size of image header (bytes). */
    uint16_t _pad2;
    uint32_t ih_img_size; /* Does not include header. */
    uint32_t ih_flags;    /* IMAGE_F_[...]. */
    struct image_version ih_ver;
    uint32_t _pad3;
};

/** Image TLV header.  All fields in little endian. */
struct image_tlv_info {
    uint16_t it_magic;
    uint16_t it_tlv_tot;  /* size of TLV area (including tlv_info header) */
};

/** Image trailer TLV format. All fields in little endian. */
struct image_tlv {
    uint8_t  it_type;   /* IMAGE_TLV_[...]. */
    uint8_t  _pad;
    uint16_t it_len;    /* Data length (not including TLV header). */
};

/*
 * Image header flags.
 */
#define IMAGE_F_PIC                      0x00000001 /* Not supported. */
#define IMAGE_F_NON_BOOTABLE             0x00000010 /* Split image app. */
#define IMAGE_F_RAM_LOAD                 0x00000020

/*
 * Image trailer TLV types.
 */
#define IMAGE_TLV_KEYHASH           0x01   /* hash of the public key */
#define IMAGE_TLV_SHA256            0x10   /* SHA256 of image hdr and body */
#define IMAGE_TLV_RSA2048_PSS       0x20   /* RSA2048 of hash output */
#define IMAGE_TLV_ECDSA224          0x21   /* ECDSA of hash output */
#define IMAGE_TLV_ECDSA256          0x22   /* ECDSA of hash output */
```

Optional type-length-value records (TLVs) containing image metadata are placed
after the end of the image.

The `ih_hdr_size` field indicates the length of the header, and therefore the
offset of the image itself.  This field provides for backwards compatibility in
case of changes to the format of the image header.

## Flash Map

A device's flash is partitioned according to its _flash map_.  At a high
level, the flash map maps numeric IDs to _flash areas_.  A flash area is a
region of disk with the following properties:
1. An area can be fully erased without affecting any other areas.
2. A write to one area does not restrict writes to other areas.

The boot loader uses the following flash area IDs:

``` c
#define FLASH_AREA_BOOTLOADER                    0
#define FLASH_AREA_IMAGE_0                       1
#define FLASH_AREA_IMAGE_1                       2
#define FLASH_AREA_IMAGE_SCRATCH                 3
```

The bootloader area contains the bootloader image itself. The other areas are
described in subsequent sections.

## Image Slots

A portion of the flash memory is partitioned into two image slots: a primary
slot (0) and a secondary slot (1).  The boot loader will only run an image from
the primary slot, so images must be built such that they can run from that
fixed location in flash.  If the boot loader needs to run the image resident in
the secondary slot, it must copy its contents into the primary slot before doing
so, either by swapping the two images or by overwriting the contents of the
primary slot. The bootloader supports either swap- or overwrite-based image
upgrades, but must be configured at build time to choose one of these two
strategies.

In addition to the two image slots, the boot loader requires a scratch area to
allow for reliable image swapping. The scratch area must have a size that is
enough to store at least the largest sector that is going to be swapped. Many
devices have small equally sized flash sectors, eg 4K, while others have variable
sized sectors where the largest sectors might be 128K or 256K, so the scratch
must be big enough to store that. The scratch is only ever used when swapping
firmware, which means only when doing an upgrade. Given that, the main reason
for using a larger size for the scratch is that flash wear will be more evenly
distributed, because a single sector would be written twice the number of times
than using two sectors, for example. To evaluate the ideal size of the scratch
for your use case the following parameters are relevant:

* the ratio of image size / scratch size
* the number of erase cycles supported by the flash hardware

The image size is used (instead of slot size) because only the slot's sectors
that are actually used for storing the image are copied. The image/scratch ratio
is the number of times the scratch will be erased on every upgrade. The number
of erase cycles divided by the image/scratch ratio will give you the number of
times an upgrade can be performed before the device goes out of spec.

```
num_upgrades = number_of_erase_cycles / (image_size / scratch_size)
```

Let's assume, for example, a device with 10000 erase cycles, an image size of
150K and a scratch of 4K (usual minimum size of 4K sector devices). This would
result in a total of:

`10000 / (150 / 4) ~ 267`

Increasing the scratch to 16K would give us:

`10000 / (150 / 16) ~ 1067`

There is no *best* ratio, as the right size is use-case dependent. Factors to
consider include the number of times a device will be upgraded both in the field
and during development, as well as any desired safety margin on the manufacturer's
specified number of erase cycles. In general, using a ratio that allows hundreds
to thousands of field upgrades in production is recommended.

The overwrite upgrade strategy is substantially simpler to implement than the
image swapping strategy, especially since the bootloader must work properly
even when it is reset during the middle of an image swap. For this reason, the
rest of the document describes its behavior when configured to swap images
during an upgrade.

## Boot Swap Types

When the device first boots under normal circumstances, there is an up-to-date
firmware image in slot 0, which mcuboot can validate and then chain-load. In
this case, no image swaps are necessary. During device upgrades, however, new
candidate images are present in slot 1, which mcuboot must swap into slot 0
before booting as discussed above.

Upgrading an old image with a new one by swapping can be a two-step process. In
this process, mcuboot performs a "test" swap of image data in flash and boots
the new image. The new image can then update the contents of flash at runtime
to mark itself "OK", and mcuboot will then still choose to run it during the
next boot. When this happens, the swap is made "permanent". If this doesn't
happen, mcuboot will perform a "revert" swap during the next boot by swapping
the images back into their original locations, and attempting to boot the old
image.

Depending on the use case, the first swap can also be made permanent directly.
In this case, mcuboot will never attempt to revert the images on the next reset.

Test swaps are supported to provide a rollback mechanism to prevent devices
from becoming "bricked" by bad firmware.  If the device crashes immediately
upon booting a new (bad) image, mcuboot will revert to the old (working) image
at the next device reset, rather than booting the bad image again. This allows
device firmware to make test swaps permanent only after performing a self-test
routine.

On startup, mcuboot inspects the contents of flash to decide which of these
"swap types" to perform; this decision determines how it proceeds.

The possible swap types, and their meanings, are:

- `BOOT_SWAP_TYPE_NONE`: The "usual" or "no upgrade" case; attempt to boot the
  contents of slot 0.

- `BOOT_SWAP_TYPE_TEST`: Boot the contents of slot 1 by swapping images. Unless
  the swap is made permanent, revert back on the next boot.

- `BOOT_SWAP_TYPE_PERM`: Permanently swap images, and boot the upgraded image
  firmware.

- `BOOT_SWAP_TYPE_REVERT`: A previous test swap was not made permanent; swap back
  to the old image whose data are now in slot 1.  If the old image marks itself
  "OK" when it boots, the next boot will have swap type `BOOT_SWAP_TYPE_NONE`.

- `BOOT_SWAP_TYPE_FAIL`: Swap failed because image to be run is not valid.

- `BOOT_SWAP_TYPE_PANIC`: Swapping encountered an unrecoverable error.

The "swap type" is a high-level representation of the outcome of the
boot. Subsequent sections describe how mcuboot determines the swap type from
the bit-level contents of flash.

## Image Trailer

For the bootloader to be able to determine the current state and what actions
should be taken during the current boot operation, it uses metadata stored in
the image flash areas. While swapping, some of this metadata is temporarily
copied into and out of the scratch area.

This metadata is located at the end of the image flash areas, and is called an
image trailer. An image trailer has the following structure:

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                                                               ~
    ~    Swap status (BOOT_MAX_IMG_SECTORS * min-write-size * 3)    ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                           Swap size                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   0xff padding (4 octets)                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Copy done   |           0xff padding (7 octets)             ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Image OK    |           0xff padding (7 octets)             ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                       MAGIC (16 octets)                       ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The offset immediately following such a record represents the start of the next
flash area.

Note: "min-write-size" is a property of the flash hardware.  If the hardware
allows individual bytes to be written at arbitrary addresses, then
min-write-size is 1.  If the hardware only allows writes at even addresses,
then min-write-size is 2, and so on.

An image trailer contains the following fields:

1. Swap status: A series of records which records the progress of an image
   swap.  To swap entire images, data are swapped between the two image areas one
   or more sectors at a time, like this:

   - sector data in slot 0 is copied into scratch, then erased
   - sector data in slot 1 is copied into slot 0, then erased
   - sector data in scratch is copied into slot 1

As it swaps images, the bootloader updates the swap status field in a way that
allows it to compute how far this swap operation has progressed for each
sector.  The swap status field can thus used to resume a swap operation if the
bootloader is halted while a swap operation is ongoing and later reset. The
`BOOT_MAX_IMG_SECTORS` value is the configurable maximum number of sectors mcuboot
supports for each image; its value defaults to 128, but allows for either
decreasing this size, to limit RAM usage, or to increase it in devices that have
massive amounts of Flash or very small sized sectors and thus require a bigger
configuration to allow for the handling of all slot's sectors. The factor of
min-write-sz is due to the behavior of flash hardware. The factor of 3 is
explained below.

2. Swap size: When beginning a new swap operation, the total size that needs
   to be swapped (based on the slot with largest image + tlvs) is written to this
   location for easier recovery in case of a reset while performing the swap.

3. Copy done: A single byte indicating whether the image in this slot is
   complete (0x01=done; 0xff=not done).

4. Image OK: A single byte indicating whether the image in this slot has been
   confirmed as good by the user (0x01=confirmed; 0xff=not confirmed).

5. MAGIC: The following 16 bytes, written in host-byte-order:

``` c
    const uint32_t boot_img_magic[4] = {
        0xf395c277,
        0x7fefd260,
        0x0f505235,
        0x8079b62c,
    };
```

## IMAGE TRAILERS

At startup, the boot loader determines the boot swap type by inspecting the
image trailers.  When using the term "image trailers" what is meant is the
aggregate information provided by both image slot's trailers.

The image trailers records are structured around the limitations imposed by flash
hardware.  As a consequence, they do not have a very intuitive design, and it
is difficult to get a sense of the state of the device just by looking at the
image trailers.  It is better to map all the possible trailer states to the swap
types described above via a set of tables.  These tables are reproduced below.

Note: An important caveat about the tables described below is that they must
be evaluated in the order presented here. Lower state numbers must have a
higher priority when testing the image trailers.

```
    State I
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Any    | Good   |
            image-ok | Any    | Unset  |
           copy-done | Any    | Any    |
    -----------------+--------+--------'
     result: BOOT_SWAP_TYPE_TEST       |
    -----------------------------------'


    State II
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Any    | Good   |
            image-ok | Any    | 0x01   |
           copy-done | Any    | Any    |
    -----------------+--------+--------'
     result: BOOT_SWAP_TYPE_PERM       |
    -----------------------------------'


    State III
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Good   | Unset  |
            image-ok | 0xff   | Any    |
           copy-done | 0x01   | Any    |
    -----------------+--------+--------'
     result: BOOT_SWAP_TYPE_REVERT     |
    -----------------------------------'
```

Any of the above three states results in mcuboot attempting to swap images.

Otherwise, mcuboot does not attempt to swap images, resulting in one of the
other three swap types, as illustrated by State IV.

```
    State IV
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Any    | Any    |
            image-ok | Any    | Any    |
           copy-done | Any    | Any    |
    -----------------+--------+--------'
     result: BOOT_SWAP_TYPE_NONE,      |
             BOOT_SWAP_TYPE_FAIL, or   |
             BOOT_SWAP_TYPE_PANIC      |
    -----------------------------------'
```

In State IV, when no errors occur, mcuboot will attempt to boot the contents of
slot 0 directly, and the result is `BOOT_SWAP_TYPE_NONE`. If the image in slot 0
is not valid, the result is `BOOT_SWAP_TYPE_FAIL`. If a fatal error occurs during
boot, the result is `BOOT_SWAP_TYPE_PANIC`. If the result is either
`BOOT_SWAP_TYPE_FAIL` or `BOOT_SWAP_TYPE_PANIC`, mcuboot hangs rather than booting
an invalid or compromised image.

Note: An important caveat to the above is the result when a swap is requested
      and the image in slot 1 fails to validate, due to a hashing or signing
      error. This state behaves as State IV with the extra action of marking
      the image in slot 0 as "OK", to prevent further attempts to swap.


## High-Level Operation

With the terms defined, we can now explore the boot loader's operation.  First,
a high-level overview of the boot process is presented.  Then, the following
sections describe each step of the process in more detail.

Procedure:

1. Inspect swap status region; is an interrupted swap being resumed?
    Yes: Complete the partial swap operation; skip to step 3.
    No: Proceed to step 2.

2. Inspect image trailers; is a swap requested?
    Yes.
        1. Is the requested image valid (integrity and security check)?
            Yes.
                a. Perform swap operation.
                b. Persist completion of swap procedure to image trailers.
                c. Proceed to step 3.
            No.
                a. Erase invalid image.
                b. Persist failure of swap procedure to image trailers.
                c. Proceed to step 3.
    No: Proceed to step 3.

3. Boot into image in slot 0.

## Image Swapping

The boot loader swaps the contents of the two image slots for two reasons:
    * User has issued a "set pending" operation; the image in slot-1 should be
      run once (state II) or repeatedly (state III), depending on whether a
      permanent swap was specified.
    * Test image rebooted without being confirmed; the boot loader should
      revert to the original image currently in slot-1 (state IV).

If the image trailers indicates that the image in the secondary slot should be
run, the boot loader needs to copy it to the primary slot.  The image currently
in the primary slot also needs to be retained in flash so that it can be used
later.  Furthermore, both images need to be recoverable if the boot loader
resets in the middle of the swap operation.  The two images are swapped
according to the following procedure:

<!-- Markdown doesn't do nested numbered lists.  It will do nested
bulletted lists, so maybe that is better. -->
    1. Determine how many flash sectors each image slot consists of.  This
       number must be the same for both slots.
    2. Iterate the list of sector indices in descending order (i.e., starting
       with the greatest index); current element = "index".
        b. Erase scratch area.
        c. Copy slot1[index] to scratch area.
            - If these are the last sectors (i.e., first swap being perfomed),
              copy the full sector *except* the image trailer.
            - Else, copy entire sector contents.
        d. Write updated swap status (i).

        e. Erase slot1[index]
        f. Copy slot0[index] to slot1[index]
            - If these are the last sectors (i.e., first swap being perfomed),
              copy the full sector *except* the image trailer.
            - Else, copy entire sector contents.
        g. Write updated swap status (ii).

        h. Erase slot0[index].
        i. Copy scratch area to slot0[index].
            - If these are the last sectors (i.e., first swap being perfomed),
              copy the full sector *except* the image trailer.
            - Else, copy entire sector contents.
        j. Write updated swap status (iii).

    3. Persist completion of swap procedure to slot 0 image trailer.

The additional caveats in step 2f are necessary so that the slot 1 image
trailer can be written by the user at a later time.  With the image trailer
unwritten, the user can test the image in slot 1 (i.e., transition to state
II).

Note1: If the sector being copied is the last sector, then swap status is
temporarily maintained on scratch for the duration of this operation, always
using slot0's area otherwise.

Note2: The bootloader tries to copy only used sectors (based on largest image
installed on any of the slots), minimizing the amount of sectors copied and
reducing the amount of time required for a swap operation.

The particulars of step 3 vary depending on whether an image is being tested,
permanently used, reverted or a validation failure of slot 1 happened when a
swap was requested:

    * test:
        o Write slot0.copy_done = 1
        (swap caused the following values to be written:
            slot0.magic = BOOT_MAGIC
            slot0.image_ok = Unset)

    * permanent:
        o Write slot0.copy_done = 1
        (swap caused the following values to be written:
            slot0.magic = BOOT_MAGIC
            slot0.image_ok = 0x01)

    * revert:
        o Write slot0.copy_done = 1
        o Write slot0.image_ok = 1
        (swap caused the following values to be written:
            slot0.magic = BOOT_MAGIC)

    * failure to validate slot 1:
        o Write slot0.image_ok = 1

After completing the operations as described above the image in slot 0 should
be booted.

## Swap Status

The swap status region allows the boot loader to recover in case it restarts in
the middle of an image swap operation.  The swap status region consists of a
series of single-byte records.  These records are written independently, and
therefore must be padded according to the minimum write size imposed by the
flash hardware.  In the below figure, a min-write-size of 1 is assumed for
simplicity.  The structure of the swap status region is illustrated below.  In
this figure, a min-write-size of 1 is assumed for simplicity.

```
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec127,state 0 |sec127,state 1 |sec127,state 2 |sec126,state 0 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec126,state 1 |sec126,state 2 |sec125,state 0 |sec125,state 1 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec125,state 2 |                                               |
    +-+-+-+-+-+-+-+-+                                               +
    ~                                                               ~
    ~               [Records for indices 124 through 1              ~
    ~                                                               ~
    ~               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~               |sec000,state 0 |sec000,state 1 |sec000,state 2 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

The above is probably not helpful at all; here is a description in English.

Each image slot is partitioned into a sequence of flash sectors.  If we were to
enumerate the sectors in a single slot, starting at 0, we would have a list of
sector indices.  Since there are two image slots, each sector index would
correspond to a pair of sectors.  For example, sector index 0 corresponds to
the first sector in slot 0 and the first sector in slot 1.  Finally, reverse
the list of indices such that the list starts with index `BOOT_MAX_IMG_SECTORS - 1`
and ends with 0.  The swap status region is a representation of this reversed list.

During a swap operation, each sector index transitions through four separate
states:
```
    0. slot 0: image 0,   slot 1: image 1,   scratch: N/A
    1. slot 0: image 0,   slot 1: N/A,       scratch: image 1 (1->s, erase 1)
    2. slot 0: N/A,       slot 1: image 0,   scratch: image 1 (0->1, erase 0)
    3. slot 0: image 1,   slot 1: image 0,   scratch: N/A     (s->0)
```

Each time a sector index transitions to a new state, the boot loader writes a
record to the swap status region.  Logically, the boot loader only needs one
record per sector index to keep track of the current swap state.  However, due
to limitations imposed by flash hardware, a record cannot be overwritten when
an index's state changes.  To solve this problem, the boot loader uses three
records per sector index rather than just one.

Each sector-state pair is represented as a set of three records.  The record
values map to the above four states as follows

```
            | rec0 | rec1 | rec2
    --------+------+------+------
    state 0 | 0xff | 0xff | 0xff
    state 1 | 0x01 | 0xff | 0xff
    state 2 | 0x01 | 0x02 | 0xff
    state 3 | 0x01 | 0x02 | 0x03
```

The swap status region can accommodate `BOOT_MAX_IMG_SECTORS` sector indices.
Hence, the size of the region, in bytes, is `BOOT_MAX_IMG_SECTORS * min-write-size * 3`.
The only requirement for the index count is that it is great enough to account for a
maximum-sized image (i.e., at least as great as the total sector count in an
image slot).  If a device's image slots have been configured with
`BOOT_MAX_IMG_SECTORS: 128` and use less than 128 sectors, the first
record that gets written will be somewhere in the middle of the region.  For
example, if a slot uses 64 sectors, the first sector index that gets swapped is
63, which corresponds to the exact halfway point within the region.

Note: since the scratch area only ever needs to record swapping of the last
sector, it uses at most min-write-size * 3 bytes for its own status area.

## Reset Recovery

If the boot loader resets in the middle of a swap operation, the two images may
be discontiguous in flash.  Bootutil recovers from this condition by using the
image trailers to determine how the image parts are distributed in flash.

The first step is determine where the relevant swap status region is located.
Because this region is embedded within the image slots, its location in flash
changes during a swap operation.  The below set of tables map image trailers
contents to swap status location.  In these tables, the "source" field
indicates where the swap status region is located.

```
              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Good       | Any        |
    copy-done | 0x01       | N/A        |
    ----------+------------+------------'
    source: none                        |
    ------------------------------------'

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Good       | Any        |
    copy-done | 0xff       | N/A        |
    ----------+------------+------------'
    source: slot 0                      |
    ------------------------------------'

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Any        | Good       |
    copy-done | Any        | N/A        |
    ----------+------------+------------'
    source: scratch                     |
    ------------------------------------'

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Unset      | Any        |
    copy-done | 0xff       | N/A        |
    ----------+------------+------------|
    source: slot 0                      |
    ------------------------------------+------------------------------+
    This represents one of two cases:                                  |
    o No swaps ever (no status to read, so no harm in checking).       |
    o Mid-revert; status in slot 0.                                    |
    For this reason we assume slot 0 as source, to trigger a check     |
    of the status area and find out if there was swapping under way.   |
    -------------------------------------------------------------------'
```

If the swap status region indicates that the images are not contiguous,
bootutil completes the swap operation that was in progress when the system was
reset.  In other words, it applies the procedure defined in the previous
section, moving image 1 into slot 0 and image 0 into slot 1.  If the boot
status file indicates that an image part is present in the scratch area, this
part is copied into the correct location by starting at step e or step h in the
area-swap procedure, depending on whether the part belongs to image 0 or image
1.

After the swap operation has been completed, the boot loader proceeds as though
it had just been started.

## Integrity Check

An image is checked for integrity immediately before it gets copied into the
primary slot.  If the boot loader doesn't perform an image swap, then it can
perform an optional integrity check of the image in slot0 if
`MCUBOOT_VALIDATE_SLOT0` is set, otherwise it doesn't perform an integrity check.

During the integrity check, the boot loader verifies the following aspects of
an image:
    * 32-bit magic number must be correct (0x96f3b83d).
    * Image must contain an `image_tlv_info` struct, identified by its magic
      (0x6907) exactly following the firmware (hdr_size + img_size).
    * Image must contain a SHA256 TLV.
    * Calculated SHA256 must match SHA256 TLV contents.
    * Image *may* contain a signature TLV.  If it does, it must also have a
      KEYHASH TLV with the hash of the key that was used to sign. The list of
      keys will then be iterated over looking for the matching key, which then
      will then be used to verify the image contents.

## Security

As indicated above, the final step of the integrity check is signature
verification.  The boot loader can have one or more public keys embedded in it
at build time.  During signature verification, the boot loader verifies that an
image was signed with a private key that corresponds to the embedded keyhash
TLV.

For information on embedding public keys in the boot loader, as well as
producing signed images, see: [signed_images](signed_images.md).
