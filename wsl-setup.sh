# First you should install Windows subsystem For Linux (WSL) by typing
# "wsl --install" in a cmd or powershell window.
#
# Once WSL is installed, you can start a terminal by invoking 'wsl' from the run 
# menu, or from cmd or powershell.
#
# An alternative for programming firmware on windows is to use WebSerial: 
#   https://learn.adafruit.com/adafruit-metro-esp32-s2/web-serial-esptool
 
set -e
SRCDIR=$PWD
#
# install esp-idf
mkdir -p ~/Arduino/hardware/espressif
cd ~/Arduino/hardware/espressif 
git clone https://github.com/espressif/arduino-esp32.git esp32 
cd esp32/tools
[ -e /usr/bin/python ] || sudo ln -s /usr/bin/python3 /usr/bin/python
python get.py
#
# install arduino-cli
cd $HOME
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
#
# install make
sudo apt -y install make

#
# Install project dependencies
cd $SRCDIR
make libs
make extralibs

