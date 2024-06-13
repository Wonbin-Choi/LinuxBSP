#!/bin/bash

echo "configure build output path"

KERNEL_TOP_PATH="$( cd "$(dirname "$0")" ; pwd -P)"
OUTPUT="$KERNEL_TOP_PATH/out"
echo "$OUTPUT"

KERNEL=kernel8

cd out

make 0=$OUTPUT modules_install
cp $OUTPUT/arch/arm64/boot/dts/broadcom/*.dtb /boot/firmware
cp $OUTPUT/arch/arm64/boot/dts/overlays/*.dtb* /boot/firmware/overlays/
#cp $OUTPUT/arch/arm64/boot/dtts/overlays/README /boot/overlays/
cp $OUTPUT/arch/arm64/boot/Image /boot/firmware/$KERNEL.img
