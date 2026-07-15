# Buildroot Package Patches

Patches against **Buildroot 2025.02.x** stock package recipes. <br>
Each patch upgrades a package to a newer version and/or tunes build options for the Ramliya appliance profile.

Apply in numeric order from the Buildroot root directory:

```bash
cd /opt/l3plus/buildroot
for p in ~/Ramliya/buildroot/patches/*.patch; do
    patch -p1 < "$p"
done
```

## Patch Summary

| # | Package | Stock Version | Patched Version | Notes |
|:--|:--------|:-------------|:----------------|:------|
| 0001 | busybox | 1.37.0 | 1.38.0 | Core utilities, httpd, udhcpc |
| 0002 | chrony | 4.6.1 | 4.8 | NTP daemon; disabled seccomp/IPv6/TLS (not needed on appliance) |
| 0003 | gpsd | 3.25 | 3.27.5 | GPS daemon; stripped to timeservice-only build (no clients, fixed 9600 baud) |
| 0004 | ethtool | 6.14 | 7.0 | Network diagnostics |
| 0005 | htop | 3.3.0 | 3.5.1 | Process monitor |
| 0006 | lsof | 4.99.4 | 4.99.7 | Switched from `generic-package` to `autotools-package` (upstream migrated) |
| 0007 | zlib-ng | 2.1.6 | 2.3.3 | Compression library |
| 0008 | gperf | 3.1 | 3.3 | Hash function generator (host tool) |
| 0009 | uclibc | 1.0.57 | 1.0.58 | uClibc-ng (not used in current build — musl is the active C library) |

All SHA-256 hashes were recalculated from upstream release tarballs.
