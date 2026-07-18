#!/bin/sh
# Trim /usr/share bloat for embedded NTP image
set -e
TARGET_DIR="$1"
[ -d "$TARGET_DIR" ] || { echo "Bad TARGET_DIR: $TARGET_DIR" >&2; exit 1; }

# XDG desktop metadata — irrelevant headless
rm -rf "$TARGET_DIR/usr/share/applications"
rm -rf "$TARGET_DIR/usr/share/icons"
rm -rf "$TARGET_DIR/usr/share/pixmaps"

# SNMP MIBs — не используем net-snmp
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

# udev hwdb (udev not used — busybox mdev)
rm -rf "$TARGET_DIR/lib/udev/hwdb.d" 2>/dev/null || true

# finit-skeleton demo web page (unused; our docroot is /www, port 8080)
rm -rf "$TARGET_DIR/srv/www"
rm -f  "$TARGET_DIR/etc/default/httpd"

# i2c-tools perl decode scripts (no DIMMs/EDID on this board, no perl anyway)
rm -f "$TARGET_DIR/usr/bin/decode-dimms" "$TARGET_DIR/usr/bin/decode-edid" \
      "$TARGET_DIR/usr/bin/decode-vaio" "$TARGET_DIR/usr/bin/ddcmon"

# stale busybox applet symlinks (noclobber leftovers after applet removal)
for a in insmod rmmod lsmod modprobe lspci lsscsi eject uudecode uuencode uuidgen unix2dos dos2unix; do
    for d in sbin usr/sbin bin usr/bin; do
        [ -L "$TARGET_DIR/$d/$a" ] && rm -f "$TARGET_DIR/$d/$a"
    done
done

# dead-on-arrival scripts: perl (no perl) and bash (no bash) interpreters
rm -f "$TARGET_DIR/usr/sbin/i2c-stub-from-dump" "$TARGET_DIR/usr/sbin/phc.sh"

# vlock if left behind by screen/kbd removal
rm -f "$TARGET_DIR/usr/bin/vlock"


# empty/unused dirs
rmdir "$TARGET_DIR/media" "$TARGET_DIR/srv" "$TARGET_DIR/opt" \
      "$TARGET_DIR/var/log/chrony" "$TARGET_DIR/etc/sysctl.d" 2>/dev/null || true

rm -rf "$TARGET_DIR/srv"

rm -f "$TARGET_DIR/etc/inetd.conf" 

# "$TARGET_DIR/etc/default/dropbear"

echo "post-build.sh: Ramliya trim & cleanup complete"
