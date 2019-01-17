SciAps Environment Setup Instructions for Linux Kernel Driver Work

### Additional Resources
1. https://source.android.com/source/requirements
2. http://variwiki.com/index.php?title=DART-SD410

### Download and Install Ubuntu 16.04
https://www.ubuntu.com/download/alternative-downloads

### Download Packages
```bash
sudo add-apt-repository ppa:openjdk-r/ppa
```
```bash
sudo add-apt-repository ppa:nilarimogard/webupd8
```
```bash
sudo apt-get update
```
```bash
sudo apt-get -y install build-essential libc6:i386 libncurses5:i386 libstdc++6:i386 libbz2-1.0:i386 git-core gnupg zip zlib1g-dev gcc-multilib libc6-dev-i386 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache libgl1-mesa-dev xsltproc unzip libswitch-perl default-jre u-boot-tools mtd-utils lzop xorg-dev libopenal-dev libglew-dev libalut-dev xclip python ruby-dev openvpn minicom curl gperf bison android-tools-adb android-tools-fastboot android-tools-fsutils git g++-multilib lib32z1 libxml2-utils openjdk-7-jdk flex mkisofs bc
```

### Download Variscite Resources
```bash
mkdir ~/dart-sd410 \
&& cd ~/Downloads \
&& wget -m --user=dart-sd410 --password=varSD410 ftp://ftp.variscite.com \
&& mv ~/Downloads/ftp.variscite.com ~/dart-sd410
```

### Download Google Repo Tool
```bash
mkdir ~/bin \
&& export PATH=~/bin:$PATH \
&& curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo \
&& chmod a+x ~/bin/repo
```

### Download maxtouch driver configuration
```bash
curl https://s3.us-east-2.amazonaws.com/sciaps-firmware-dependencies/maxtouch-ts.raw > ~/dart-sd410/maxtouch-ts.raw
```

### Unless you have 16G of Ram, you will need swap memory
[more on this here](https://www.digitalocean.com/community/tutorials/how-to-add-swap-on-ubuntu-14-04)
```bash
sudo fallocate -l 16G /swapfile \
&& sudo chmod 600 /swapfile \
&& sudo mkswap /swapfile \
&& sudo swapon /swapfile
```

Make the Swap File Permanent:
```bash
sudo nano /etc/fstab
```

At the bottom of the file, you need to add a line that will tell the operating system to automatically use the file you created:
/swapfile   none    swap    sw    0   0

Hit CTRL+X and then Y to save and exit, and your swap is good to go.

### Initialize and repo sync the Variscite fork of the Android 5.1.1 Firmware
```bash
cd ~/dart-sd410 \
&& unzip ~/dart-sd410/Software/Android/Android_5/LL.1.2.4-01810-8x16.0-3/variscite_bsp_vla.br_.1.2.4-01810-8x16.0-3.zip \
&& cd source/ \
&& chmod +x SD410c_build.sh \
&& ./SD410c_build.sh
```
 
### Rebuilding Everything
```bash
cd ~/dart-sd410/source/APQ8016_410C_LA.BR.1.2.4-01810-8x16.0_5.1.1_Lollipop_P2 \
&& . build/envsetup.sh \
&& lunch msm8916_64-userdebug \
&& make -j14 WITH_DEXPREOPT=true WITH_DEXPREOPT_PIC=true DEX_PREOPT_DEFAULT=nostripping | tee log.txt
```

### Rebuilding the Kernel
```bash
cd ~/dart-sd410/source/APQ8016_410C_LA.BR.1.2.4-01810-8x16.0_5.1.1_Lollipop_P2 \
&& . build/envsetup.sh \
&& lunch msm8916_64-userdebug \
&& make kernel
```

### Build Notes
If you encounter this error:
"You have tried to change the API from what has been previously approved"
just run:
```bash
make update-api
```
as specified in the message.

If you encounter this error:
"error: unsupported reloc 43"
just run:
```bash
cp /usr/bin/ld.gold prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.11-4.6/x86_64-linux/bin/ld
```

### System Notes
If you see "Insufficient Permissions" when using adb, just run:
```bash
adb kill-server && sudo adb start-server
```

If you see "Read only file system" when attempting to push files onto the device, just *adb shell* in and then run:
```bash
mount -o rw,remount / \
&& mount -o rw,remount /system \
&& exit
```

### Flashing
Reboot the device into the bootloader (may already be in the bootloader):
```bash
adb reboot bootloader
```
Run this command until you see your device displayed:
```bash
sudo fastboot devices
```
Flash and Boot the Entire System!
```bash
RESCUE_IMAGES_ROOT=~/dart-sd410/Software/Android/Android_5/RescueImages \
&& cd $RESCUE_IMAGES_ROOT \
&& sudo fastboot flash partition gpt_both0.bin \
&& sudo fastboot flash hyp hyp.mbn \
&& sudo fastboot flash modem NON-HLOS.bin \
&& sudo fastboot flash rpm rpm.mbn \
&& sudo fastboot flash sbl1 sbl1.mbn \
&& sudo fastboot flash sec sec.dat \
&& sudo fastboot flash tz tz.mbn \
&& sudo fastboot flash sbl1bak sbl1.mbn \
&& sudo fastboot flash hypbak hyp.mbn \
&& sudo fastboot flash rpmbak rpm.mbn \
&& sudo fastboot flash tzbak tz.mbn \
&& sudo fastboot erase cache \
&& sudo fastboot erase devinfo \
&& AOSP_ROOT=~/dart-sd410/source/APQ8016_410C_LA.BR.1.2.4-01810-8x16.0_5.1.1_Lollipop_P2 \
&& cd $AOSP_ROOT/out/target/product/msm8916_64/ \
&& sudo fastboot flash aboot emmc_appsboot.mbn \
&& sudo fastboot flash abootbak emmc_appsboot.mbn \
&& sudo fastboot flash persist persist.img \
&& sudo fastboot flash userdata userdata.img \
&& sudo fastboot flash system system.img \
&& sudo fastboot flash recovery recovery.img \
&& sudo fastboot flash boot boot.img \
&& sudo fastboot reboot
```

Flash just the Linux Kernel and Boot!
```bash
AOSP_ROOT=~/dart-sd410/source/APQ8016_410C_LA.BR.1.2.4-01810-8x16.0_5.1.1_Lollipop_P2 \
&& cd $AOSP_ROOT/out/target/product/msm8916_64/ \
&& sudo fastboot flash boot boot.img \
&& sudo fastboot reboot
```

### Configuring Touch Screen
First, *adb shell* in and run the following to enable adb push:
```bash
mount -o rw,remount / \ 
&& mount -o rw,remount /system \
&& mkdir -p /system/lib/firmware/ \ 
&& exit
```
After exiting adb shell, push the touch controller driver:
```bash
adb push ~/dart-sd410/maxtouch-ts.raw /system/lib/firmware/
```
Enter *adb shell* again to map the touch controller to this configuration:
```bash
echo "maxtouch-ts.raw" > /sys/class/i2c-dev/i2c-6/device/6-004a/update_cfg \
&& exit
```

### Submitting Changes
Any changes you make to the kernel can be submitted as a Pull Request here:
https://github.com/SciAps/DART-SD410-kernel
