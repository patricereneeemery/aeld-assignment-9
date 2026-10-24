#!/bin/bash
# Automatically run finder-test.sh inside QEMU and exit

set -e

# Location of kernel and initramfs
KERNEL_IMAGE=/tmp/aeld/Image
INITRAMFS=/tmp/aeld/initramfs.cpio.gz

# Run QEMU and automatically execute finder-test.sh
qemu-system-arm \
    -M versatilepb \
    -m 128M \
    -nographic \
    -kernel ${KERNEL_IMAGE} \
    -append "console=ttyAMA0" \
    -initrd ${INITRAMFS} \
    -serial mon:stdio \
    -no-reboot \
    -fsdev local,id=fsdev0,path=/tmp/aeld/rootfs,security_model=none \
    -device virtio-9p-device,fsdev=fsdev0,mount_tag=hostfs \
<< 'EOF'
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev || true
cd /home
./finder-test.sh
poweroff -f
EOF
