#!/bin/sh
# Trim /usr/share bloat for embedded NTP image
set -e
TARGET_DIR="$1"
[ -d "$TARGET_DIR" ] || { echo "Bad TARGET_DIR: $TARGET_DIR" >&2; exit 1; }

# XDG desktop metadata — irrelevant headless
rm -rf "$TARGET_DIR/usr/share/applications"
rm -rf "$TARGET_DIR/usr/share/icons"
rm -rf "$TARGET_DIR/usr/share/pixmaps"

# Remove unneeded SNMP MIBs
rm -rf "$TARGET_DIR/usr/share/snmp"

# Documentation
rm -rf "$TARGET_DIR/usr/share/doc"
rm -rf "$TARGET_DIR/usr/share/man"
rm -rf "$TARGET_DIR/usr/share/info"

# Trim terminfo — only used term types
if [ -d "$TARGET_DIR/usr/share/terminfo" ]; then
    find "$TARGET_DIR/usr/share/terminfo" -mindepth 2 -type f \
        ! -name 'xterm' ! -name 'xterm-256color' \
        ! -name 'screen' ! -name 'screen-256color' \
        ! -name 'linux' ! -name 'vt100' ! -name 'vt102' ! -name 'vt220' \
        -delete
    find "$TARGET_DIR/usr/share/terminfo" -mindepth 1 -type d -empty -delete 2>/dev/null || true
fi

# udev hwdb (we not use udev — busybox mdev)
rm -rf "$TARGET_DIR/lib/udev/hwdb.d" 2>/dev/null || true

# finit-skeleton demo web page (unused; our docroot is /www, port 8080)
rm -rf "$TARGET_DIR/srv/www"
rm -f  "$TARGET_DIR/etc/default/httpd"

echo "post-build.sh: trim complete"
