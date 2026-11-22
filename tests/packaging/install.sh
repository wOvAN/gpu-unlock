#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Install the packages .github/ci/package.sh built, check what they put on
# disk, then remove them again. Run as root from the repository root.
set -eu
sh tests/packaging/source.sh
version=$(cat VERSION)
src=/usr/src/gpu-unlock-$version
fw=/usr/lib/firmware/gpu-unlock/610.43.03
dracut=/usr/lib/dracut/dracut.conf.d/gpu-unlock.conf
. /etc/os-release
case "$ID" in
debian|ubuntu)
	install="apt-get install -qy --no-install-recommends ./build/deb/*.deb"
	remove="apt-get purge -qy"
	# The init-top script loads the module before udev's coldplug would
	# load nvidia, and the overrides hook carries the override helpers and
	# the modprobe drop-in into the image, so the deb has to ship both.
	boot="/usr/share/initramfs-tools/scripts/init-top/gpu-unlock
	      /usr/share/initramfs-tools/hooks/gpu-unlock-overrides"
	# Ubuntu 26.04 ships a dracut initrd and no mkinitramfs, so the deb has
	# to rebuild the images on its own there.
	test -x /usr/bin/dracut -o -x /bin/dracut && boot=$dracut ;;
fedora)
	install="dnf install -qy build/rpm/RPMS/noarch/*.rpm"
	remove="dnf remove -qy"
	boot=$dracut ;;
opensuse*)
	install="zypper -n install --allow-unsigned-rpm build/rpm/RPMS/noarch/*.rpm"
	remove="zypper -n remove"
	boot=$dracut ;;
arch)
	install="pacman -U --noconfirm packaging/arch/*.pkg.tar.zst"
	remove="pacman -Rns --noconfirm"
	boot=/etc/mkinitcpio.conf.d/zz-gpu-unlock.conf ;;
*)
	echo "unknown distribution: $ID" >&2
	exit 1 ;;
esac

$install
test -f "$src/dkms.conf"
test -f "$src/main.c"
test -f "$src/ecc.c"
test -f "$src/hbm.c"
test -f "$src/overrides.c"
test -x /usr/lib/gpu-unlock/prepare-overrides
test -x /usr/lib/gpu-unlock/apply-procfs-override
test -f /usr/lib/modprobe.d/gpu-unlock.conf
test "$(readlink /usr/lib/firmware/nvidia/gpu-unlock)" = /sys/kernel/gpu_unlock/overrides
test -f /usr/lib/systemd/system/gpu-unlock-overrides.service
test -f "$fw/gsp_ga10x.bin"
test -f "$fw/booter_load_ga102.bin"
test -f "$dracut"
test -x /usr/lib/dracut/modules.d/91gpu-unlock-overrides/prepare.sh
for dropin in $boot; do
	test -f "$dropin"
done

$remove gpu-unlock-dkms gpu-unlock-firmware
test ! -e "$src"
test ! -e "$fw"
test ! -e "$dracut"
test ! -e /usr/lib/dracut/modules.d/91gpu-unlock-overrides/prepare.sh
test ! -e /usr/lib/gpu-unlock/prepare-overrides
test ! -e /usr/lib/gpu-unlock/apply-procfs-override
test ! -e /usr/lib/modprobe.d/gpu-unlock.conf
test ! -L /usr/lib/firmware/nvidia/gpu-unlock
for dropin in $boot; do
	test ! -e "$dropin"
done
echo "packages install and remove cleanly"
