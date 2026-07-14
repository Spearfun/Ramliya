# Reference Materials

Stock Bitmain firmware forensics extracted from the Antminer L3+ control board.

## Contents

- `config/` — Extracted kernel configurations from the stock 3.8.13 kernel (`config.gz`, antBBB defconfig)
- `dt/` — Decompiled vendor device tree blobs (am335x-boneblack-bitmainer.dtb, beaglebone.dtb)
- `nand-dump/` — Original Bitmain boot files extracted from NAND (`MLO`, `u-boot.img`, `uImage.bin`, `am335x-boneblack-bitmainer.dtb`, `initramfs.bin.SD`, `uEnv.txt`). These are the factory firmware files with timestamps from 2014–2017.

These files are not part of the build — they exist as reference for understanding the original board configuration and for cross-referencing during device tree development.

Note: Raw NAND partition dumps (`mtd*_raw.bin`, ~50 MB) are not included. The vendor boot files above contain the meaningful content.
