#
# This installation script should work on a fresh Raspberry pi os
#
set -e
SRCDIR=$PWD
#
# install arduino-esp32 (latest version from git, not released version)
mkdir -p ~/.arduino15/hardware/espressif
cd ~/.arduino15/hardware/espressif
git clone https://github.com/espressif/arduino-esp32.git esp32 
cd esp32/tools
python get.py
sudo ln -s ~/.arduino15/hardware/espressif/tools/esptool/esptool.py /usr/local/bin/esptool
sudo ln -s /usr/bin/pyserial-miniterm /usr/local/bin/miniterm
#
# install arduino-cli
cd $HOME
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
#
# OPTIONAL: install OpenOCD
sudo apt-get -y install autoconf libtool libusb-1.0-0 libusb-1.0-0-dev
git clone http://openocd.zylin.com/openocd
cd openocd
./bootstrap
./configure --enable-sysfsgpio --enable-bcm2835gpio
make
sudo make install

#
# OPTIONAL: install esp-idf


#
# Install project dependencies
cd $SRCDIR
make libs
make extralibs

