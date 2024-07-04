#!/bin/sh -e

export PATH=$PATH

[ ! -z "$VIRTUALIZATION" ] && exit 0

LIBMOUNT_FORCE_MOUNT2=always mount -o remount,ro /

if [ -x /sbin/dmraid -o -x /bin/dmraid ]; then
    dmraid -i -ay
fi

if [ -x /bin/mdadm ]; then
    mdadm -As
fi

if [ -x /bin/btrfs ]; then
    btrfs device scan
fi

if [ -x /sbin/vgchange -o -x /bin/vgchange ]; then
    vgchange --sysinit -a ay
fi

if [ -e /etc/crypttab ]; then
    awk -f $UNITD_DATA_PATH/functions/crypt.awk /etc/crypttab
    if [ -x /sbin/vgchange -o -x /bin/vgchange ]; then
        vgchange --sysinit -a ay
    fi
fi

if [ -x /usr/bin/zpool -a -x /usr/bin/zfs ]; then
    if [ -e /etc/zfs/zpool.cache ]; then
        zpool import -N -a -c /etc/zfs/zpool.cache
    else
        zpool import -N -a -o cachefile=none
    fi
    zfs mount -a -l
    zfs share -a
    # NOTE(dh): ZFS has ZVOLs, block devices on top of storage pools.
    # In theory, it would be possible to use these as devices in
    # dmraid, btrfs, LVM and so on. In practice it's unlikely that
    # anybody is doing that, so we aren't supporting it for now.
fi

[ -f /fastboot ] && FASTBOOT=1
[ -f /forcefsck ] && FORCEFSCK="-f"
for arg in $(cat /proc/cmdline); do
    case $arg in
    fastboot) FASTBOOT=1 ;;
    forcefsck) FORCEFSCK="-f" ;;
    esac
done

if [ -z "$FASTBOOT" ]; then
    fsck -A -T -a -t noopts=_netdev $FORCEFSCK >/dev/null
fi

LIBMOUNT_FORCE_MOUNT2=always mount -o remount,rw /
mount -a -t "nosysfs,nonfs,nonfs4,nosmbfs,nocifs" -O no_netdev
