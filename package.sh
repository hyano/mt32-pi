#!/bin/sh

SDCARD=build-sdcard
BOOT=external/circle-stdlib/libs/circle/boot
WLAN=external/circle-stdlib/libs/circle/addon/wlan

rm -rf $SDCARD
mkdir $SDCARD
cp -r sdcard $SDCARD/
cp -pr ../sdcard/roms/* $SDCARD/sdcard/roms/
mkdir $SDCARD/sdcard/sc55_roms
cp -pr ../sdcard/sc55_roms/sc55/* $SDCARD/sdcard/sc55_roms/

for b in pi2 pi3-64 pi4-64; do
  make mrproper
  make -j BOARD=$b
  cp kernel*.img $SDCARD/sdcard/
done

mkdir $SDCARD/sdcard/hdmi
for b in pi2 pi3-64 pi4-64; do
  make mrproper
  make -j BOARD=$b HDMI_CONSOLE=1
  cp kernel*.img $SDCARD/sdcard/hdmi/
done

make -C $BOOT firmware armstub64
cp  $BOOT/armstub8-rpi4.bin \
    $BOOT/bcm2711-rpi-4-b.dtb \
    $BOOT/bcm2711-rpi-400.dtb \
    $BOOT/bcm2711-rpi-cm4.dtb \
    $BOOT/bootcode.bin \
    $BOOT/COPYING.linux \
    $BOOT/fixup_cd.dat \
    $BOOT/fixup4cd.dat \
    $BOOT/LICENCE.broadcom \
    $BOOT/start_cd.elf \
    $BOOT/start4cd.elf \
    $SDCARD/sdcard/

mkdir $SDCARD/sdcard/firmware
make -C $WLAN/firmware
cp  $WLAN/firmware/LICENCE.broadcom_bcm43xx \
    $WLAN/firmware/brcmfmac43430-sdio.bin \
    $WLAN/firmware/brcmfmac43430-sdio.txt \
    $WLAN/firmware/brcmfmac43436-sdio.bin \
    $WLAN/firmware/brcmfmac43436-sdio.txt \
    $WLAN/firmware/brcmfmac43436-sdio.clm_blob \
    $WLAN/firmware/brcmfmac43455-sdio.bin \
    $WLAN/firmware/brcmfmac43455-sdio.txt \
    $WLAN/firmware/brcmfmac43455-sdio.clm_blob \
    $WLAN/firmware/brcmfmac43456-sdio.bin \
    $WLAN/firmware/brcmfmac43456-sdio.txt \
    $WLAN/firmware/brcmfmac43456-sdio.clm_blob \
    $SDCARD/sdcard/firmware/

mkdir $SDCARD/sdcard/docs
cp LICENSE README.md $SDCARD/sdcard/docs/

DATE=`date +%Y%m%d`
cd $SDCARD/sdcard
zip -9r ../mt32-pi-sc55-${DATE}.zip *
cd ../..
