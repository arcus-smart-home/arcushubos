/*
 * Copyright 2019 Arcus Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <irislib.h>


// Local Defines

#define INSTALL_LOCK_FILE     "/var/lock/fwinstall"
#define TMP_KERNEL1_DIR       "/tmp/kernel1"
#define TMP_KERNEL2_DIR       "/tmp/kernel2"
#define TMP_BOOT_DIR          "/tmp/bootupdate"
#define READ_BLOCK_SIZE       1024
#define IMAGE_VALIDATE_ERR    2

// These are set locally in the irisinit scripts
#define KERNEL1_DEV           "/dev/kern1"
#define KERNEL2_DEV           "/dev/kern2"
#define DEV_ROOTFS1           "/dev/fs1"
#define DEV_ROOTFS2           "/dev/fs2"

// Platform specific defines
#ifdef beaglebone_yocto
#define BOOT_DEV              "/dev/mmcblk0p1"
#define KERNEL_FSTYPE         "ext2"
#define BOOT_FSTYPE           "vfat"
#define UBOOT_MLO             "/tmp/update/MLO-beaglebone"
#define UBOOT_IMAGE           "/tmp/update/u-boot-beaglebone.img"
#define UBOOT_TMP_MLO         "/tmp/bootupdate/MLO"
#define UBOOT_TMP_IMAGE       "/tmp/bootupdate/u-boot.img"
#define KERNEL_IMAGE          "/tmp/update/uImage-beaglebone.bin"
#define KERNEL_TMP_IMAGE      "/boot/uImage"
#define KERNEL_DTB            "/tmp/update/uImage-am335x-boneblack.dtb"
#define KERNEL_TMP_DTB        "/boot/am335x-boneblack.dtb"
#define ROOTFS_FILE           "core-image-minimal-iris-beaglebone.squashfs"
#elif imxdimagic
#define KERNEL_FSTYPE         "ext2"
#define UBOOT_MLO             "/tmp/update/u-boot-imxdimagic.imx"
#define UBOOT_IMAGE           "/tmp/update/u-boot-imxdimagic.imx"
#define KERNEL_IMAGE          "/tmp/update/zImage-imxdimagic.bin"
#define KERNEL_TMP_IMAGE      "/zImage"
#define KERNEL_DTB            "/tmp/update/zImage-imx6dl-imagic.dtb"
#define KERNEL_TMP_DTB        "/imx6dl-imagic.dtb"
#define ROOTFS_FILE           "core-image-minimal-iris-imxdimagic.squashfs"
#else
 #error "Please specify platform!"
#endif

// Radio firmware files
#define ZWAVE_FIRMWARE           "zwave-firmware.bin"
#define ZIGBEE_FIRMWARE          "zigbee-firmware.bin"
#define ZIGBEE_FIRMWARE_HWFLOW   "zigbee-firmware-hwflow.bin"
#define BLE_FIRMWARE             "ble-firmware.bin"
#define BLE_FIRMWARE_HWFLOW      "ble-firmware-hwflow.bin"
#define FIRMWARE_DIR             "/data/firmware/"
#define TMP_CMP_FILE             "/tmp/flashData"


// Clean up temp directories, etc.
void cleanup_exit(int code)
{
    // Unmount directories
    umount2(TMP_KERNEL1_DIR, MNT_FORCE);
    umount2(TMP_KERNEL2_DIR, MNT_FORCE);
    umount2(TMP_BOOT_DIR, MNT_FORCE);

    // Remove tmp dirs
    sync(); sync();
    rmdir(TMP_BOOT_DIR);
    rmdir(TMP_KERNEL1_DIR);
    rmdir(TMP_KERNEL2_DIR);

    // Remove install files
    system("rm -rf /tmp/update");

    // Clear lock file
    unlink(INSTALL_LOCK_FILE);

    // Return error
    exit(code);
}


static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] filename\n"
            "  options:\n"
            "    -h              Print this message\n"
            "    -k              Kill agent before installing radio firmware\n"
            "    -n              No validation of image data writes\n"
            "    -s              Skip install of radio firmware\n"
            "                    Install firmware file\n"
            "\n", name);
}

/* Stop hub agent related processes */
static void stopHubAgent(void)
{
    fprintf(stdout, "Stopping hub agent before radio firmware install...\n");

    /* Stop irisagentd first */
    system("killall irisagentd");

    /* Then kill java processes */
    system("killall java");

    /* Wait a bit for processes to terminate */
    sleep(10);
}

#ifndef beaglebone_yocto
/* Compare file to flash data */
static int cmpFlashToFile(char *partition, int offset, int bs, char *filename)
{
    char cmd[256];
    int blockCount;
    struct stat st;
    int ret;

    /* Get size of file, in blocks */
    stat(filename, &st);
    blockCount = st.st_size / bs;

    /* Read data to tmp file */
    snprintf(cmd, sizeof(cmd), "dd if=%s bs=%d skip=%d count=%d of=%s",
	     partition, bs, offset, blockCount, TMP_CMP_FILE);
    system(cmd);

    /* Diff file */
    snprintf(cmd, sizeof(cmd), "diff -q %s %s", filename, TMP_CMP_FILE);
    ret = system(cmd);
    unlink(TMP_CMP_FILE);
    return ret;
}
#endif

/* Install firmware archive */
int main(int argc, char** argv)
{
    int  c, lock, verify = 1, killAgent = 0, skipradio = 0;
    char *filearg = NULL, *bootdir, *rootfsdev, *firmware = NULL;
    char cmd[256], filepath[256], origpath[256];
    FILE *f;
    int  res = 0, index1 = -1, index2 = -1, updateindex;

    /* Parse options... */
    opterr = 0;
    while ((c = getopt(argc, argv, "hkns")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 'k':
        killAgent = 1;
        break;
    case 's':
        skipradio = 1;
        break;
    case 'n':
        verify = 0; // Don't verify data writes
        break;
    case '?':
        fprintf(stderr, "Unknown option `\\x%x'.\n", optopt);
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    default:
        fprintf(stderr, "Error parsing options - exiting!\n");
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    }

    /* Must have the file as argument */
    filearg = argv[optind];
    if (filearg == NULL) {
        fprintf(stderr, "Incorrect number of arguments - exiting!\n");
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    }

    // Use /tmp as install work directory
    chdir("/tmp");

    // Get lock file to avoid parallel attemps to write firmware
    // Lock is set once - need to reboot to clear so we don't attempt
    //  to write firmware again without a reboot!
    if (access(INSTALL_LOCK_FILE, F_OK) != -1) {
        fprintf(stderr,
                "A firmware install is already in process! Reboot required!\n");
        exit(INSTALL_IN_PROCESS);
    }
    lock = open(INSTALL_LOCK_FILE, O_CREAT | O_RDWR, 0644);
    close(lock);

    // Untar file - just a .tgz file format for now, will add header later
    IRIS_setLedMode("upgrade-unpack");
    fprintf(stdout, "Unpacking firmware update archive...\n");
    pokeWatchdog();
    snprintf(cmd, sizeof(cmd), "tar xf %s", filearg);
    if (system(cmd)) {
        IRIS_setLedMode("upgrade-unpack-err");
        fprintf(stderr, "Error unpacking firmware archive!\n");
        unlink(INSTALL_LOCK_FILE);
        exit(INSTALL_UNPACK_ERR);
    }

    // Files will be in the update directory
    chdir("/tmp/update");
    fprintf(stdout, "Verifying file checksums...\n");
    pokeWatchdog();
    snprintf(cmd, sizeof(cmd), "sha256sum -c sha256sums.txt");
    if (system(cmd)) {
        IRIS_setLedMode("upgrade-unpack-err");
        fprintf(stderr, "Error checking SHA256 checksums!\n");
        unlink(INSTALL_LOCK_FILE);
        system("rm -rf /tmp/update");
        exit(INSTALL_ARCHIVE_CKSUM_ERR);
    }

    // Mount update partitions to determine which to use
    pokeWatchdog();
    mkdir(TMP_KERNEL1_DIR, 0777);
    mkdir(TMP_KERNEL2_DIR, 0777);
    fprintf(stdout, "Mounting kernel partitions...\n");
    if (mount(KERNEL1_DEV, TMP_KERNEL1_DIR, KERNEL_FSTYPE,
              MS_SYNCHRONOUS, NULL)) {
        IRIS_setLedMode("upgrade-unpack-err");
        fprintf(stderr, "Error mounting kernel1 partition!\n");
        cleanup_exit(INSTALL_MOUNT_ERR);
    }
    if (mount(KERNEL2_DEV, TMP_KERNEL2_DIR, KERNEL_FSTYPE,
              MS_SYNCHRONOUS, NULL)) {
        IRIS_setLedMode("upgrade-unpack-err");
        fprintf(stderr, "Error mounting kernel2 partition!\n");
        cleanup_exit(INSTALL_MOUNT_ERR);
    }

    // Look at bootindex files
    f = fopen(TMP_KERNEL1_DIR "/bootindex", "r");
    if (f) {
        fscanf(f, "%d", &index1);
        fclose(f);
    }
    f = fopen(TMP_KERNEL2_DIR "/bootindex", "r");
    if (f) {
        fscanf(f, "%d", &index2);
        fclose(f);
    }

    // Check for case where latest bootindex doesn't match our root
    //  indicates a corruption issue at boot.  The uboot code can't
    //  clear the index, so we need to work around here.
    f = popen("cat /proc/cmdline", "r");
    if (f) {
        char fs1[64];
        char fs2[64];
        int bytes;
        fs1[0] = fs2[0] = '\0';
        // We use links to partitions - get real partition for comparison
        bytes = readlink(DEV_ROOTFS1, fs1, sizeof(fs1)-1);
        if (bytes != -1) {
            fs1[bytes] = '\0';
        }
        bytes = readlink(DEV_ROOTFS2, fs2, sizeof(fs2)-1);
        if (bytes != -1) {
            fs2[bytes] = '\0';
        }
        if (fgets(cmd, sizeof(cmd), f)) {
            if ((index1 > index2) && (strstr(cmd, fs1) == NULL))  {
                index1 = -1;
                fprintf(stdout,
                        "Partition 1 must be corrupt - will use for install\n");
            } else if ((index2 > index1) &&
                       (strstr(cmd, fs2) == NULL))  {
                index2 = -1;
                fprintf(stdout,
                        "Partition 2 must be corrupt - will use for install\n");
            }
        }
        pclose(f);
    }
    fprintf(stdout, "Bootindex1 = %d\n", index1);
    fprintf(stdout, "Bootindex2 = %d\n", index2);

    if (index1 > index2) {
        fprintf(stdout, "Installing to second update partition.\n");
        bootdir = TMP_KERNEL2_DIR;
        rootfsdev = DEV_ROOTFS2;
        updateindex = index1 + 1;
    } else {
        fprintf(stdout, "Installing to first update partition.\n");
        bootdir = TMP_KERNEL1_DIR;
        rootfsdev = DEV_ROOTFS1;
        updateindex = index2 + 1;
    }

    // Make sure agent can access boot directory and firmware location
    snprintf(filepath, sizeof(filepath), "%s/%s", bootdir, "boot");
    chmod(filepath, S_IRWXU|S_IRWXG|S_IRWXO);
    chmod(FIRMWARE_DIR, S_IRWXU|S_IRWXG|S_IRWXO);

    // Install boot files if present
    IRIS_setLedMode("upgrade-bootloader");
    if ((access(UBOOT_MLO, F_OK) != -1) &&
        (access(UBOOT_IMAGE, F_OK) != -1)) {
        fprintf(stdout, "Installing u-boot files...\n");
        pokeWatchdog();

#ifdef beaglebone_yocto
        mkdir(TMP_BOOT_DIR, 0777);
        if (mount(BOOT_DEV, TMP_BOOT_DIR, BOOT_FSTYPE,
                  MS_SYNCHRONOUS, NULL)) {
            IRIS_setLedMode("upgrade-bootloader-err");
            fprintf(stderr, "Error mounting boot partition!\n");
            cleanup_exit(INSTALL_UBOOT_ERR);
        }

        // Only install if different
        snprintf(cmd, sizeof(cmd), "diff -q %s %s", UBOOT_MLO, UBOOT_TMP_MLO);
        if (system(cmd)) {
            fprintf(stdout, "Installing MLO...\n");
            chmod(UBOOT_TMP_MLO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            unlink(UBOOT_TMP_MLO);
            if (IRIS_copyFile(UBOOT_MLO, UBOOT_TMP_MLO)) {
                IRIS_setLedMode("upgrade-bootloader-err");
                fprintf(stderr, "Error installing u-boot MLO file!\n");
                cleanup_exit(INSTALL_UBOOT_ERR);
            }
            // Correct owner/group to root to be safe (shouldn't matter for FAT)
            chmod(UBOOT_TMP_MLO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(UBOOT_TMP_MLO, 0, 0);
            if (verify) {
                snprintf(cmd, sizeof(cmd), "diff -q %s %s", UBOOT_MLO,
                         UBOOT_TMP_MLO);
                if (system(cmd)) {
                    IRIS_setLedMode("upgrade-bootloader-err");
                    fprintf(stderr, "Error verifying u-boot MLO file!\n");
                    cleanup_exit(INSTALL_UBOOT_ERR);
                }
            }
        }

        // Only install if different
        snprintf(cmd, sizeof(cmd), "diff -q %s %s", UBOOT_IMAGE, UBOOT_TMP_IMAGE);
        if (system(cmd)) {
            fprintf(stdout, "Installing u-boot.img\n");
            chmod(UBOOT_TMP_IMAGE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            unlink(UBOOT_TMP_IMAGE);
            if (IRIS_copyFile(UBOOT_IMAGE, UBOOT_TMP_IMAGE)) {
                IRIS_setLedMode("upgrade-bootloader-err");
                fprintf(stderr, "Error installing u-boot image file!\n");
                cleanup_exit(INSTALL_UBOOT_ERR);
            }
            // Correct owner/group to root to be safe (shouldn't matter for FAT)
            chmod(UBOOT_TMP_IMAGE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(UBOOT_TMP_IMAGE, 0, 0);
            if (verify) {
                snprintf(cmd, sizeof(cmd), "diff -q %s %s", UBOOT_IMAGE,
                         UBOOT_TMP_IMAGE);
                if (system(cmd)) {
                    IRIS_setLedMode("upgrade-bootloader-err");
                    fprintf(stderr, "Error verifying u-boot image file!\n");
                    cleanup_exit(INSTALL_UBOOT_ERR);
                }
            }
        }
#elif defined(imxdimagic)
        // Compare data first
        if (cmpFlashToFile("/dev/mmcblk2boot0", 2, 512, UBOOT_IMAGE)) {
            fprintf(stdout, "Installing u-boot.imx\n");
            system("echo 0 > /sys/block/mmcblk2boot0/force_ro");
            snprintf(cmd, sizeof(cmd),
                     "dd if=%s of=/dev/mmcblk2boot0 seek=2 bs=512",
                     UBOOT_IMAGE);
            if (system(cmd)) {
                IRIS_setLedMode("upgrade-bootloader-err");
                fprintf(stderr, "Error installing u-boot image file!\n");
                cleanup_exit(INSTALL_UBOOT_ERR);
            }
            // Verify as well
            if (cmpFlashToFile("/dev/mmcblk2boot0", 2, 512, UBOOT_IMAGE)) {
                IRIS_setLedMode("upgrade-bootloader-err");
                fprintf(stderr, "Error verifying u-boot image file!\n");
                cleanup_exit(INSTALL_UBOOT_ERR);
            }
        }
#endif
    }

    // Install kernel files if present
    IRIS_setLedMode("upgrade-kernel");
    if (access(KERNEL_IMAGE, F_OK) != -1) {
        // Make sure agent can access file
        pokeWatchdog();
        snprintf(filepath, sizeof(filepath), "%s%s",bootdir, KERNEL_TMP_IMAGE);
        chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

        snprintf(cmd, sizeof(cmd), "diff -q %s %s", KERNEL_IMAGE, filepath);
        if (system(cmd)) {
            fprintf(stdout, "Installing kernel image file...\n");
            unlink(filepath);
            if (IRIS_copyFile(KERNEL_IMAGE, filepath)) {
                IRIS_setLedMode("upgrade-kernel-err");
                fprintf(stderr, "Error installing kernel image file!\n");
                cleanup_exit(INSTALL_KERNEL_ERR);
            }
            // Correct owner/group to root
            chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(filepath, 0, 0);
            if (verify) {
                snprintf(cmd, sizeof(cmd), "diff -q %s %s", KERNEL_IMAGE,
                         filepath);
                if (system(cmd)) {
                    IRIS_setLedMode("upgrade-kernel-err");
                    fprintf(stderr, "Error verifying kernel image file!\n");
                    cleanup_exit(INSTALL_KERNEL_ERR);
                }
            }
        }
    }

    // Install DTB file
    if (access(KERNEL_DTB, F_OK) != -1) {
        // Make sure agent can access file
        pokeWatchdog();
        snprintf(filepath, sizeof(filepath), "%s%s",bootdir, KERNEL_TMP_DTB);
        chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

        snprintf(cmd, sizeof(cmd), "diff -q %s %s", KERNEL_DTB, filepath);
        if (system(cmd)) {
            fprintf(stdout, "Installing device tree file...\n");
            unlink(filepath);
            if (IRIS_copyFile(KERNEL_DTB, filepath)) {
                IRIS_setLedMode("upgrade-kernel-err");
                fprintf(stderr,
                        "Error installing am335x-boneblack.dtb file!\n");
                cleanup_exit(INSTALL_KERNEL_ERR);
            }
            // Correct owner/group to root
            chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(filepath, 0, 0);
            if (verify) {
                snprintf(cmd, sizeof(cmd), "diff -q %s %s", KERNEL_DTB, filepath);
                if (system(cmd)) {
                    IRIS_setLedMode("upgrade-kernel-err");
                    fprintf(stderr,
                            "Error verifying am335x-boneblack.dtb file!\n");
                    cleanup_exit(INSTALL_KERNEL_ERR);
                }
            }
        }
    }

    // Install rootfs
    if (access(ROOTFS_FILE, F_OK) != -1) {
        char *readBuf, *compareBuf;
        int writefd, readfd, compare = 1;
        unsigned long remaining;
        struct stat fstat;
        int readSize;

        // Get file information, file descriptors and buffers
        res = stat(ROOTFS_FILE, &fstat);
        readfd = open(ROOTFS_FILE, O_RDONLY);
        writefd = open(rootfsdev, O_RDWR);
        readBuf = malloc(READ_BLOCK_SIZE + 16);
        compareBuf = malloc(READ_BLOCK_SIZE + 16);

        // Any errors?
        if (res || (readfd < 0) || (writefd < 0) || (readBuf == NULL) ||
            (compareBuf == NULL)) {
            res = 1;
        }

        fprintf(stdout, "Installing root filesystem...\n");
        pokeWatchdog();
        remaining = fstat.st_size;
        while (!res && remaining) {

            // How much to read?
            if (remaining > READ_BLOCK_SIZE) {
                readSize = READ_BLOCK_SIZE;
            } else {
                readSize = remaining;
            }

            // Get rootfs data
            if (read(readfd, readBuf, readSize) != readSize) {
                res = 1;
                break;
            }

            // Compare?
            if (compare) {
                // Get current data
                if (read(writefd, compareBuf, readSize) != readSize) {
                    res = 1;
                    break;
                }
                // Same?
                if (memcmp(readBuf, compareBuf, readSize) == 0) {
                    // On to next block
                    remaining -= readSize;
                    continue;
                }

                // Mismatch - need to write rest of buffer
                compare = 0;

                // Rewind file
                lseek(writefd, -readSize, SEEK_CUR);
            }

            // Write data
            if (write(writefd, readBuf, readSize) != readSize) {
                res = 1;
                break;
            }

            // Next block
            remaining -= readSize;
        }

        // Verify write?
        if (!res && verify) {
            // Rewind files
            pokeWatchdog();
            lseek(writefd, 0, SEEK_SET);
            lseek(readfd, 0, SEEK_SET);

            fprintf(stdout, "Verifying root filesystem...\n");
            remaining = fstat.st_size;
            while (!res && remaining) {

                // How much to read?
                if (remaining > READ_BLOCK_SIZE) {
                    readSize = READ_BLOCK_SIZE;
                } else {
                    readSize = remaining;
                }

                // Get rootfs data
                if (read(readfd, readBuf, readSize) != readSize) {
                    res = 1;
                    break;
                }

                // Get current data
                if (read(writefd, compareBuf, readSize) != readSize) {
                    res = 1;
                    break;
                }
                // Same?
                if (memcmp(readBuf, compareBuf, readSize) == 0) {
                    // On to next block
                    remaining -= readSize;
                    continue;
                }
                // Mismatch - error!
                res = IMAGE_VALIDATE_ERR;
            }
        }

        // Free resources
        if (readfd >= 0)
            close(readfd);
        if (writefd >= 0)
            close(writefd);
        if (readBuf)
            free(readBuf);
        if (compareBuf)
            free(compareBuf);

        // Did we hit an error?
        if (res) {
            IRIS_setLedMode("upgrade-rootfs-err");
            if (res == IMAGE_VALIDATE_ERR) {
                fprintf(stderr, "Error verifying root file system!\n");
            } else {
                fprintf(stderr, "Error installing root file system!\n");
            }
            cleanup_exit(INSTALL_ROOTFS_ERR);
        }
    }

    // Skip radio firmware update?
    if (skipradio) {
        fprintf(stdout, "Skipping radio firmware update...\n");
        goto wrapup;
    }

    // Update Zwave firmware
    if (access(ZWAVE_FIRMWARE, F_OK) != -1) {
        // Make sure agent can access firmware file
        pokeWatchdog();
        snprintf(origpath, sizeof(origpath), "%s%s", "/tmp/update/",
                 ZWAVE_FIRMWARE);
        snprintf(filepath, sizeof(filepath), "%s%s", FIRMWARE_DIR,
                 ZWAVE_FIRMWARE);
        chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

        snprintf(cmd, sizeof(cmd), "diff -q %s %s", ZWAVE_FIRMWARE, filepath);
        if (system(cmd)) {
            // To avoid installation issues, kill agent first
            if (killAgent) {
                stopHubAgent();
                killAgent = 0;  // No need to kill agent again
            }

            IRIS_setLedMode("upgrade-zwave");
            unlink(filepath);
            fprintf(stdout, "Installing Zwave firmware...\n");
            snprintf(cmd, sizeof(cmd), "zwave_flash -w %s", ZWAVE_FIRMWARE);
            if (system(cmd)) {
                IRIS_setLedMode("upgrade-zwave-err");
                fprintf(stderr, "Error installing Zwave firmware!\n");
                cleanup_exit(INSTALL_ZWAVE_ERR);
            }
            // Copy file to final location only upon success
            IRIS_copyFile(origpath, filepath);
            chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(filepath, 0, 0);
        }
    }

    // Update Zigbee firmware
    if (IRIS_isHwFlowControlSupported()) {
        fprintf(stdout,
                "Zigbee radio hardware supports hardware flow control\n");
        firmware = ZIGBEE_FIRMWARE_HWFLOW;
    } else {
        firmware = ZIGBEE_FIRMWARE;
    }
    if (access(firmware, F_OK) != -1) {
        // Make sure agent can access firmware file
        pokeWatchdog();
        snprintf(filepath, sizeof(filepath), "%s%s", FIRMWARE_DIR,
                 firmware);
        snprintf(origpath, sizeof(origpath), "%s%s", "/tmp/update/",
                 firmware);
        chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

        snprintf(cmd, sizeof(cmd), "diff -q %s %s", firmware, filepath);
        if (system(cmd)) {
            // To avoid installation issues, kill agent first
            if (killAgent) {
                stopHubAgent();
                killAgent = 0;  // No need to kill agent again
            }

            IRIS_setLedMode("upgrade-zigbee");
            unlink(filepath);
            fprintf(stdout, "Installing Zigbee firmware...\n");
            snprintf(cmd, sizeof(cmd), "zigbee_flash -w %s", firmware);
            if (system(cmd)) {
                IRIS_setLedMode("upgrade-zigbee-err");
                fprintf(stderr, "Error installing Zigbee firmware!\n");
                cleanup_exit(INSTALL_ZIGBEE_ERR);
            }
            // Copy file to final location only upon success
            IRIS_copyFile(origpath, filepath);
            chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(filepath, 0, 0);
        }
    }

    // Update BLE firmware
    if (IRIS_isHwFlowControlSupported()) {
        fprintf(stdout,
                "BLE radio hardware supports hardware flow control\n");
        firmware = BLE_FIRMWARE_HWFLOW;
    } else {
        firmware = BLE_FIRMWARE;
    }
    if (access(firmware, F_OK) != -1) {
        // Make sure agent can access firmware file
        pokeWatchdog();
        snprintf(origpath, sizeof(origpath), "%s%s", "/tmp/update/",
                 firmware);
        snprintf(filepath, sizeof(filepath), "%s%s", FIRMWARE_DIR,
                 firmware);
        chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

        snprintf(cmd, sizeof(cmd), "diff -q %s %s", firmware, filepath);
        if (system(cmd)) {
            // To avoid installation issues, kill agent first
            if (killAgent) {
                stopHubAgent();
                killAgent = 0;  // No need to kill agent again
            }

            IRIS_setLedMode("upgrade-bte");
            unlink(filepath);
            fprintf(stdout, "Installing BLE firmware...\n");
            // If we are attached, need to make sure to terminate first
            system("killall hciattach > /dev/null 2>&1");
            snprintf(cmd, sizeof(cmd), "ble_prog %s", firmware);
            if (system(cmd)) {
                IRIS_setLedMode("upgrade-bte-err");
                fprintf(stderr, "Error installing BLE firmware!\n");
                cleanup_exit(INSTALL_BLE_ERR);
            }
            // Copy file to final location only upon success
            IRIS_copyFile(origpath, filepath);
            chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(filepath, 0, 0);
        }
    }

 wrapup:
    // Last step - update bootindex value
    pokeWatchdog();
    snprintf(cmd, sizeof(cmd), "%s/bootindex", bootdir);
    f = fopen(cmd, "w");
    if (f == NULL) {
        fprintf(stderr, "Error updating bootindex!\n");
        cleanup_exit(INSTALL_KERNEL_ERR);
    }
    fprintf(f, "%d\n", updateindex);
    fclose(f);

    // Remove boot directory
    if (access(TMP_BOOT_DIR, F_OK) != -1) {
        umount2(TMP_BOOT_DIR, MNT_FORCE);
        sync(); sync();
        rmdir(TMP_BOOT_DIR);
    }

    // Kernel directories can be unmounted as well
    umount2(TMP_KERNEL1_DIR, MNT_FORCE);
    umount2(TMP_KERNEL2_DIR, MNT_FORCE);
    sync(); sync();
    rmdir(TMP_KERNEL1_DIR);
    rmdir(TMP_KERNEL2_DIR);

    // Remove archive directory
    system("rm -rf /tmp/update");

    // Done
    return 0;
}



