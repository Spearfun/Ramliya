#!/bin/sh
set -e

cd "${BINARIES_DIR}"

if [ -f "rootfs.cpio" ]; then
    echo -e "post-image.sh: \033[0;33mCompressing rootfs.cpio...\033[0m"
    gzip -9 -c rootfs.cpio > rootfs.cpio.gz
    
    echo -e "post-image.sh: \033[0;33mCreating initramfs.bin.SD over mkimage...\033[0m"
    mkimage -A arm -O linux -T ramdisk -C gzip -n "initramfs" -d rootfs.cpio.gz initramfs.bin.SD
    
    rm -f rootfs.cpio.gz
    echo -e "post-image.sh: \033[0;32mFile initramfs.bin.SD build successfully\033[0m"
else
    echo -e "\033[0;31mpost-image.sh Error: File rootfs.cpio not found in ${BINARIES_DIR}\033[0m" >&2
    exit 1
fi
