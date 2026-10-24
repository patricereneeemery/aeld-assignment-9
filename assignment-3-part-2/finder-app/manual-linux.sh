#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo. Modified by Patrice Emery

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

if [ $# -lt 1 ]
then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi

if [ ! -e "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # REQUIRED: enable initramfs + devtmpfs support
    ./scripts/config --enable BLK_DEV_INITRD
    ./scripts/config --enable DEVTMPFS
    ./scripts/config --enable DEVTMPFS_MOUNT
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} olddefconfig

    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

    cp arch/${ARCH}/boot/Image ${OUTDIR}/
fi

echo "Adding the Image in outdir"
echo "Creating the staging directory for the root filesystem"

cd "${OUTDIR}"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf "${OUTDIR}/rootfs"
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr usr/bin usr/lib usr/sbin var var/log

cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

sudo chown root:root ${OUTDIR}/rootfs/bin/busybox
sudo chmod 4755 ${OUTDIR}/rootfs/bin/busybox

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
cp -a /usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp -a /usr/aarch64-linux-gnu/lib/libm.so.6 ${OUTDIR}/rootfs/lib/
cp -a /usr/aarch64-linux-gnu/lib/libresolv.so.2 ${OUTDIR}/rootfs/lib/
cp -a /usr/aarch64-linux-gnu/lib/libc.so.6 ${OUTDIR}/rootfs/lib/

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/tty c 5 0

# TODO: Clean and build the writer utility
cd "${FINDER_APP_DIR}"
make CROSS_COMPILE=${CROSS_COMPILE} -f ${FINDER_APP_DIR}/Makefile

cp writer ${OUTDIR}/rootfs/home/

# REQUIRED: copy writer.sh and ensure it is executable
cp ${FINDER_APP_DIR}/writer.sh ${OUTDIR}/rootfs/home/
sudo chmod +x ${OUTDIR}/rootfs/home/writer.sh

# TODO: Copy the finder related scripts and executables to the /home directory
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/

# REQUIRED: copy real conf directory (not symlink)
cp -r ${FINDER_APP_DIR}/conf ${OUTDIR}/rootfs/home/

# Fix path inside finder-test.sh
sed -i 's|\.\./conf|conf|g' ${OUTDIR}/rootfs/home/finder-test.sh

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs

# REQUIRED: create /init with correct logic + guaranteed shutdown
sudo tee ${OUTDIR}/rootfs/init > /dev/null << 'EOF'
#!/bin/sh
# Mount essential pseudo‑filesystems needed by BusyBox and the kernel
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev

# TODO: Run the autorun script that triggers the finder tests
cd /home
sh autorun-qemu.sh
RET=$?

# Log the exit code to the kernel message buffer for debugging
echo "autorun-qemu.sh exited with code $RET" > /dev/kmsg

# TODO: Always power off so QEMU exits cleanly for the autograder
poweroff -f
EOF

sudo chmod +x ${OUTDIR}/rootfs/init

find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
