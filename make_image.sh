#!/bin/sh
# exit if a command fails
set -e

# Variables
IMG="$1"
KERNEL_BIN="$2"
GRUB_CFG="$3"

# create an image and partition it (to make a virtual hard drive for the OS)
# there should be 32768 sectors on the image 
# for the filesystem we use ext2 (type 0x83)
# the filesystem starts at sector 2048 (0x800) and is defined up to sector 30720
# total number of sectors in the filesystem is 28673
dd if=/dev/zero of="$IMG" bs=512 count=32768
parted "$IMG" mklabel msdos
parted "$IMG" mkpart primary ext2 2048s 30720s
parted "$IMG" set 1 boot on

# map the image to a loop device
LOOP_DISK=$(losetup -f)
LOOP_NUM=$(echo "$LOOP_DISK" | sed 's/[^0-9]//g')
LOOP_PART=/dev/loop$((LOOP_NUM + 1))
sudo losetup "$LOOP_DISK" "$IMG"
sudo losetup "$LOOP_PART" "$IMG" -o 1048576 # 2048 sectors * 512 bytes per sector

# make an ext2 filesystem
sudo mkfs.ext2 "$LOOP_PART"

# mount the image at /mnt/osfiles
sudo mkdir -p /mnt/osfiles
sudo mount "$LOOP_PART" /mnt/osfiles
sudo mkdir -p /mnt/osfiles/boot/grub
sudo cp "$KERNEL_BIN" /mnt/osfiles/boot/kernel.img
sudo cp "$GRUB_CFG" /mnt/osfiles/boot/grub/grub.cfg

echo "test file to verify the filesystem driver" | sudo tee /mnt/osfiles/test.txt > /dev/null

# output some md5 checksums for filesystem read checking
echo "MD5 checksums for some files"
sudo md5sum /mnt/osfiles/test.txt
sudo md5sum /mnt/osfiles/boot/kernel.img
sudo md5sum /mnt/osfiles/boot/grub/grub.cfg
echo "
    "

# install grub bootloader
sudo grub-install --root-directory=/mnt/osfiles --no-floppy \
    --modules="normal part_msdos ext2 multiboot" "$LOOP_DISK"

# unmount and cleanup
sudo umount /mnt/osfiles
sudo losetup -d "$LOOP_DISK"
sudo losetup -d "$LOOP_PART"

echo "done

    
    "
