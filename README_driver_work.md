SciAps Environment Setup Instructions for Linux Kernel Driver Work

### Additional Resources
1. https://source.android.com/source/requirements
2. http://variwiki.com/index.php?title=DART-SD410

### Download and Install Ubuntu 16.04
https://www.ubuntu.com/download/alternative-downloads

### Download Packages
```bash
sudo apt-get update \
&& sudo add-apt-repository ppa:openjdk-r/ppa \
&& sudo add-apt-repository ppa:nilarimogard/webupd8 \
&& sudo apt-get update \
&& sudo apt-get -y install build-essential libc6:i386 libncurses5:i386 libstdc++6:i386 libbz2-1.0:i386 git-core gnupg zip zlib1g-dev gcc-multilib libc6-dev-i386 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache libgl1-mesa-dev xsltproc unzip libswitch-perl default-jre u-boot-tools mtd-utils lzop xorg-dev libopenal-dev libglew-dev libalut-dev xclip python ruby-dev openvpn minicom curl gperf bison android-tools-adb android-tools-fastboot android-tools-fsutils git g++-multilib lib32z1 libxml2-utils openjdk-7-jdk flex mkisofs bc
```

### Download Variscite Resources
```bash
cd ~/Downloads/ \
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
 
### Rebuilding
```bash
cd ~/dart-sd410/source/APQ8016_410C_LA.BR.1.2.4-01810-8x16.0_5.1.1_Lollipop_P2 \
&& . build/envsetup.sh \
&& lunch msm8916_64-userdebug \
&& make -j14 WITH_DEXPREOPT=true WITH_DEXPREOPT_PIC=true DEX_PREOPT_DEFAULT=nostripping | tee log.txt
```

### Notes
If you encounter this error:
"You have tried to change the API from what has been previously approved"
just run:
```bash
make update-api
```
as specified in the message.

### Flashing
First, reboot the device into the bootloader:
```bash
adb reboot bootloader
```
Wait for fastboot, run this command until you see the device displayed:
```bash
sudo fastboot devices
```
Flash and Boot!
```bash
cd ~/dart-sd410/source/APQ8016_410C_LA.BR.1.2.4-01810-8x16.0_5.1.1_Lollipop_P2/out/target/product/msm8916_64/ \
&& sudo fastboot flash boot boot.img \
&& sudo fastboot reboot
```

### Submitting Changes
Any changes you make to the kernel can be submitted as a Pull Request here:
https://github.com/SciAps/DART-SD410-kernel
