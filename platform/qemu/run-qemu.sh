#!/bin/sh
set -eu
: "${PINE_QEMU_KERNEL:?Set PINE_QEMU_KERNEL to an ARM64 Linux Image}"
: "${PINE_QEMU_ROOTFS:?Set PINE_QEMU_ROOTFS to an ARM64 root filesystem image}"
command -v qemu-system-aarch64 >/dev/null 2>&1 || { echo "qemu-system-aarch64 is required" >&2; exit 1; }
exec qemu-system-aarch64 -M virt -cpu cortex-a72 -m 1024 -smp 4 -kernel "$PINE_QEMU_KERNEL" \
  -drive "file=$PINE_QEMU_ROOTFS,format=raw,if=virtio" -append "root=/dev/vda console=ttyAMA0 rw" \
  -netdev user,id=net0,hostfwd=tcp::4173-:4173 -device virtio-net-pci,netdev=net0 -nographic
