# pps-dmtimer-am335x — AM335x DMTimer Hardware PPS Capture Driver

This directory contains a standalone copy of the PPS driver source for reference.
The canonical integration path is via kernel patch `0009` in `kernel/patches/`.

## In-tree Integration

To integrate into a Linux 5.10 kernel source tree:

1. Copy `pps-dmtimer-am335x.c` to `drivers/pps/clients/`
2. Add to `drivers/pps/clients/Kconfig`:
   ```
   config PPS_CLIENT_DMTIMER
       bool "AM335x DMTimer hardware PPS capture"
       depends on PPS && SOC_AM33XX && OMAP_DM_TIMER
       help
         PPS client using TI AM335x DMTimer TCAR1 hardware edge capture.
         Provides sub-50ns timestamping precision independent of IRQ latency.
   ```
3. Add to `drivers/pps/clients/Makefile`:
   ```
   obj-$(CONFIG_PPS_CLIENT_DMTIMER) += pps-dmtimer-am335x.o
   ```
4. Enable `CONFIG_PPS_CLIENT_DMTIMER=y` in your kernel defconfig.

Patch `0009` in `kernel/patches/` performs these steps automatically.
