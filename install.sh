#! /bin/bash

echo 'Install script for muon timer.'
if [ $(whoami) == 'root' ]; then
	echo 'This should not be run as root -- certin steps do not work with root permissons. When sudo permissions are required, they will be requested.'
	echo 'Exiting'
	exit 0
else
	echo "muon_timer will be installed under user $(whoami)"
fi

echo 'Checking dependencies...'
dirName=/lib/modules/$(uname -r)/build
if [ ! -d $dirName ]; then
	echo "Missing dependency: linux-headers-$(uname -r)"
	exit 1
else
	echo "Kernel headers found"
fi
#pkgName=linux-headers-$(uname -r)
#dpkg-query -l $pkgName
#if [ $? != '0' ]; then
#	echo "Missing dependency: linux-headers-$(uname -r)"
#	echo 'Please install this package.'
#	exit 1
#else 
#	echo "linux-headers-$(uname -r) is installed"
#fi

initialDir=$(pwd)
nodename=$(uname -n)

futureUser=$(whoami)
baseDir=$(pwd)
loadOnBoot=yes
serverSub="/coincidences"
serverHome="$baseDir$serverSub"

ans='k'
echo "muon_timer source base directory is $baseDir. Is this correct? [y/n] "
read -e ans
while [[ "$ans" != "y" && "$ans" != "n" && "$ans" != "Y" && "$ans" != "N" ]]; do
	echo 'Enter y or n'
	read -e ans
done
if [[ "$ans" == "n" || "$ans" == "N" ]]; then
	echo "Enter the base directory where the muon_timer source currently is: "
	read -e baseDir
fi

ans='k'
echo "Would you like to automate loading the muon_timer module at startup? [y/n] "
read -e ans
while [[ "$ans" != "y" && "$ans" != "n" && "$ans" != "Y" && "$ans" != "N" ]]; do
	echo 'Enter y or n'
	read -e ans
done
if [[ "$ans" == "y" || "$ans" == "Y" ]]; then
	loadOnBoot=yes
else
	loadOnBoot=no
fi

echo "System is $nodename"
echo "Source base directory is $baseDir"
echo "Driver will be installed for use by user $futureUser"
if [ "$loadOnBoot" == "yes" ]; then
	echo "Module will automatically load when this machine boots up"
else
	echo "Module will have to be loaded manually when it needs to be used"
fi


#TODO: multiple options for udev_rules location?

echo "Creating group \'muons\', need sudo permissions:"
sudo groupadd muons
sudo usermod -a -G muons $futureUser

cd $baseDir
sudo cp udev_rules/* /etc/udev/rules.d/

if [ "$nodename" == "raspberrypi" ]; then
	# raspberry pi needs sudo permissions to do make
	cd RPI_Kernel_Driver
	makedir=$(pwd)
	sudo PWD=$makedir make
else
	# beagle bone will fail install if make is done with sudo permissions
	cd BBB_Kernel_Driver
	make
fi
if [ $? != '0' ];  then
	echo 'Error building module.'
	cd $initialDir
	exit 1
fi


echo 'Driver built. Testing loading now...'
sudo insmod muon_timer.ko

if [ $? != '0' ]; then
	echo 'Error loading new muon_timer module. Check that it installed correctly'
	cd $initialDir
	exit 1
fi

if [ "$loadOnBoot" == "yes" ]; then
	echo 'Automating module load on boot...'
	sudo mkdir /lib/modules/$(uname -r)/muon_timer
	sudo cp muon_timer.ko /lib/modules/$(uname -r)/muon_timer/muon_timer.ko
	sudo depmod
	cd ..
	sudo cp modules-load.d/* /etc/modules-load.d/
	echo 'muon_timer should now load automatically at boot. To check, reboot this machine and run lsmod'
fi

echo 'Setting up event data server...'
echo 'Creating log directory at /var/log/muon_timer'
sudo mkdir /var/log/muon_timer
sudo chown $futureUser /var/log/muon_timer

echo 'Creating pidfile directory at /opt/muon_timer'
sudo mkdir /opt/muon_timer
sudo chown $futureUser /opt/muon_timer

echo 'Finishing init script'
cd $serverHome
echo "Now in $(pwd)"
sedCmd="s^{{BASE_DIR}}^$baseDir^"
echo $sedCmd
sed -i.bak $sedCmd server_start.sh

echo 'Automating data server start on boot...'
sudo cp $serverHome/server_start.sh /etc/init.d/event_server
sudo chmod 755 /etc/init.d/event_server
sudo update-rc.d event_server defaults


cd $initialDir
