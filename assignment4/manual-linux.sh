#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
# Updated and corrected for Assignment 3 Part 2

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath "$(dirname "$0")")
ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-

if [ $# -lt 1 ]
then
        echo "Using default directory ${OUTDIR} for output"
else
        OUTDIR=$1
        echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"
cd "${OUTDIR}"

echo "Using output directory ${OUTDIR}"

############################################
# Clone and build the Linux kernel
############################################

if [ ! -d "${OUTDIR}/linux-stable" ]
then
        echo "Cloning kernel repo"
        git clone "${KERNEL_REPO}" --depth 1 --branch "${KERNEL_VERSION}"
fi

cd "${OUTDIR}/linux-stable"

echo "Checking out kernel version ${KERNEL_VERSION}"
git checkout "${KERNEL_VERSION}"

echo "Cleaning kernel tree"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

echo "Configuring kernel"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

echo "Building kernel image"
make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

echo "Building kernel modules"
make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

echo "Building device tree blobs"
make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

echo "Copying kernel image"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/zImage" "${OUTDIR}/Image"

############################################
# Create rootfs directory structure
############################################

cd "${OUTDIR}"
echo "Creating rootfs structure"

rm -rf "${OUTDIR}/rootfs"
mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"

mkdir -p bin dev etc home lib proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

############################################
# Clone and build BusyBox
############################################

cd "${OUTDIR}"

if [ ! -d "${OUTDIR}/busybox" ]
then
        echo "Cloning BusyBox"
        git clone git://busybox.net/busybox.git
fi

cd busybox
git checkout "${BUSYBOX_VERSION}"

echo "Configuring BusyBox for ARM"
make distclean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

echo "Building BusyBox for ARM"
make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

echo "Installing BusyBox into rootfs"
make CONFIG_PREFIX="${OUTDIR}/rootfs" install

############################################
# Add library dependencies
############################################

cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" || true
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" || true

echo "Adding library dependencies to rootfs"
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

cp -a "${SYSROOT}/lib/ld-linux-armhf.so.3" "${OUTDIR}/rootfs/lib/"
cp -a "${SYSROOT}/lib/arm-linux-gnueabihf/libc.so.6" "${OUTDIR}/rootfs/lib/"
cp -a "${SYSROOT}/lib/arm-linux-gnueabihf/libm.so.6" "${OUTDIR}/rootfs/lib/"
cp -a "${SYSROOT}/lib/arm-linux-gnueabihf/libresolv.so.2" "${OUTDIR}/rootfs/lib/" || true

############################################
# Create device nodes
############################################

echo "Creating device nodes"
sudo mknod -m 666 dev/null c 1 3 || true
sudo mknod -m 622 dev/console c 5 1 || true

############################################
# Create init script
############################################

echo "Creating init script"
cat << 'EOF' > "${OUTDIR}/rootfs/init"
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev || true
echo "Booting AELD rootfs"
cd /home
exec /bin/sh
EOF

chmod +x "${OUTDIR}/rootfs/init"

############################################
# Copy finder-app files into rootfs
############################################

echo "Copying finder-app files into rootfs"

cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/writer.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp -r ${FINDER_APP_DIR}/conf ${OUTDIR}/rootfs/home/

############################################
# Fix ownership
############################################

echo "Fixing rootfs ownership"
cd "${OUTDIR}/rootfs"
sudo chown -R root:root .

############################################
# Create initramfs
############################################

echo "Creating initramfs"
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
cd "${OUTDIR}"
gzip -f initramfs.cpio

echo "manual-linux.sh complete"
