#!/sbin/sh

if [ ! -d "/data/.kernel.d" ]; then
    echo "Creating default configuration"
    mkdir /data/.kernel.d
    mkdir /data/boot
    cp /tmp/default /data/.kernel.d/android
    ln -sf .kernel.d/android /data/.kernel
    cp /tmp/android_kernel /data/boot
    cp /tmp/android_initrd /data/boot
fi
