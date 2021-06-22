# How to use SDB

The Tizen Studio contains a command prompt tool called "sdb.exe", the Smart Development Bridge tool. This tool can install packages on the target, pull and push files from and to the target.

https://docs.tizen.org/application/tizen-studio/common-tools/smart-development-bridge/

First, after installation of the Tizen Studio environment, copy the sdb.exe from c:/TizenStudio/tools/sdb.exe to c:/Windows/System32 folder.

Open a git bash command terminal or windows command prompt.

Try the following:

## Get the version number of the smart development bridge
### $ sdb version

## Get the list of all devices connected
### $ sdb devices

![image](https://user-images.githubusercontent.com/37830964/117293380-976d6b00-ae71-11eb-9281-35819f7b54a5.png)

## Connect to a device
### $ sdb connect 192.168.137.95:26101

## Get the ip address
### $ sdb get-serialno

![image](https://user-images.githubusercontent.com/37830964/117410919-4d3cc600-af13-11eb-9de3-ac4ddbcbaab4.png)

## Install a target package on a device
### $ sdb install "package name.tpk"

![image](https://user-images.githubusercontent.com/37830964/117409383-6b092b80-af11-11eb-979f-47898874e26a.png)

## Execute a Linux command on the target
### $ sdb shell ls -l opt/var/tmp

![image](https://user-images.githubusercontent.com/37830964/117294028-722d2c80-ae72-11eb-8e5e-14512d02bb8c.png)

## Pull the configuration.dat file from the target
### $ sdb pull opt/var/tmp/configuration.dat

![image](https://user-images.githubusercontent.com/37830964/117293682-ffbc4c80-ae71-11eb-8847-0ae0703bffa4.png)

## Push the configuration.dat file from the target
### $ sdb push opt/var/tmp/configuration.dat

![image](https://user-images.githubusercontent.com/37830964/117293962-5de92f80-ae72-11eb-87f2-1814cae6114c.png)

## Pull all sensor files from the target
### $ sdb pull opt/usr/apps/liacs.sensorservice/data

![image](https://user-images.githubusercontent.com/37830964/117293750-1793d080-ae72-11eb-800e-d22ddea57910.png)

## Remove all sensor files from the target
### $ sdb shell rm opt/usr/apps/liacs.sensorservice/data/*.dat

## Disconnect the device
### $ sdb disconnect

