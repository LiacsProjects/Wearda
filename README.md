# WEARDA

# Introduction

As part of the Dementia project at Liacs, Leiden University, we developed the WEARDA package: an app and back ground service to capture sensor data with the Samsung Gear Fit2 Pro wearable running on Tizen OS version 2.3.1:13. This OS is developed by Samsung and is based on the Linux OS.

For a nice introduction of the WEARDA package, please see the "WEARDA software paper.pdf" in this repository. 

For installation of the development environment, installation of the WEARDA package, remarks how to use the applications and remarks about the Samsung certificates, see below. This information can also be found in the UserManual.pdf added to this repository as well.

# Installation of Tizen Studio environment

The exact installation steps are (in a nutshell):

1. Install Tizen Studio 4.1. At the end of the installation the package manager will be prompted.
2. Install the Tizen extensions for native development for version 2.3.1 (tab: Main SDK)
3. Install the Tizen extension for Certificates (tab: Extension SDK)
4. Make a Samsung developer account: https://developer.samsung.com/sa/signIn.do
5. Run package manager (Tizen Studio -> tools), go through the wizard steps.
6. Set up the Mobile hotspot of Windows (part of Network & Internet settings), switch Wifi on.
7. Take a Watch, put it in debug mode, switch on Wifi and connect to the Hotspot (pass word is shown).
8. Run device manager (Tizen Studio -> tools), press middle button on right top ("remote device manager""), add a watch to the list (plus sign). You can either add the info manually or use a button to "scan" for devices. Switch connection to "on". If a watch is connected for the first time: confirm connection on the watch, too. 
9. After a while the device manager tool shows connection activity.

Notes:
Installation guide of the Tizen Studio development environment can be found in https://developer.samsung.com/tizen/Galaxy-Watch/certificate/creating-certificate.html and then hover up to > installation.

Tip Windows Installations: 
In case you cannot run the Device Manager and get a message that msvcr120dll is missing, you will have to load an extension for your OS:
https://answers.microsoft.com/en-us/windows/forum/windows_10-performance/msvcr120dll-is-missing-or-error/aafe820f-4dbb-4043-aba2-e4ac2dcf69c1

In case you cannot run the Device Manager and get a message that msvcp120dll is missing, you will have to load an extension for your OS:
https://answers.microsoft.com/en-us/windows/forum/windows_7-windows_install/missing-msvcp120dll-file/f0a14d55-73f0-4a21-879e-1cbacf05e906

The watches have version 2.3.1:13 of the Tizen OS, so choose to install the 2.3.1 SDK together with the certificate SDK.

# Installation of Liacs sensor app and sensor service

The exact installation steps are (in a nutshell):

1. Make a GitHub account.
2. Make a fork of this repository.
3. Download from the main branch the software project sensor application and sensor service. Click on "code" and download zip file.
4. Open the project files one by one with the Tizen Studio 4.1
5. Open the device manager, make connection to the watch (step 8 + 9 of installation Tizen Studio environment).
For the next steps, you will need to get certificates to run the app on the watches. Check "Certificates" for more information. 
6. Select the sensor application project, click on the right button of your mouse, choose run-as, choose native device. The software is now downloaded to the watch.
7. Select the sensor service project, click on the right button of your mouse, choose run-as, choose native device.
8. The sensor application is found at the bottom of all applications.
9. Put watch in debug mode, switch all app off with settings only switch on wifi.
10. Push - with the device manager - a configuration file with name "configuration.dat" on folder "/opt/var/tmp/".
Notes about the configuration file: 
Write timer can be set to a higher frequency than the data is collected to miss fewer signals
The privacy circle has a max range of 10000 mt, anything higher sets the privacy circle to 100 mt
11. Do a zero measurement (for calibration offline) for 15 minutes, upload the sensor + con files.

NOTE: You can also use the sdb (Smart Development Bridge) tool which come with Tizen Studio instead of the Device Manager. See the HOW-TO-USE-SDB.md.

# Information about Certificates

1. Requires a Samsung Developer Account
2. Connect watch(es) via device manager
3. Open Certificate Manager
4. Hover over the little plus to add a new certificate
5. Click Samsung Certificate
6. You will be asked to make an author certificate and a distributor certificate
7. If you want to add a watch: click on the plus sign as if adding a new certificate, click samsung certificate, select use existing certificate profile, click no when asked if you want to create a new author certificate, create new distributor certificate, you will be asked to chose a password and either add a DUID manually or connect to the new device. Alternatively, you can use a .txt file with a new DUID per line to create certificates. 

Tip: consider setting the date to one day in the future while setting up the watch to avoid certification date issues.

# Information about the sensor application / service

## The configuration file

The sensor service always measures based on a configuration file. You can stop the measurement only by shutting down the watch (press the smallest button on the side for a few seconds).

The sensor application starts the service if the watch is switched on from shut down. So the watch is not measuring immediately. The configuration file is read after clicking on the RESTART button of the sensor application (so at the beginning of each measurement). So it is possible to measure with different configurations while not shutting down the watch all the time.

## Possible workflow for measurement, sensor app + service already installed:

1. Switch on the watch after charging to 100% (press small button on side long).
2. Check that the date time is correct.
3. Check that the wifi is off and location off if you do not use GPS (prolongs the measurement time to 15 hours).
5. Activate the sensor app and "snip" the patient identifier.
6. Click on the RESTART button.
7. Put the watch around the wrist / upper arm / ankle of the patient.
8. Write down watch identifier (last 4 characters name of watch in Mobile hotspot, see picture below), patient identifier, date + time in Excel (shadow administration).
9. After 3 hours / 15 hours collect the watch and put another watch around the wrist of the patient which went through step 1-6.
10. Switch the collected watch off (power off) and charge to 100% (so charging time is very low).
11. After battery 100%, switch on the watch and wifi to make connection with the laptop.
12. Pull the sensor files (aag + bar + gps) and con file to the laptop from "/opt/usr/apps/liacs.sensorservice/data/". This can be done with the Device Manager but better with the sdb tool (see HOW-TO-USE-SDB.md).
13. Remove the sensor- and con files from the watch if it exceeds 500 MB by pressing the CLEAN button 3x (sensor app).
14. Switch the wifi off and continu with step 2. 

![image](https://user-images.githubusercontent.com/37830964/117474773-931d7c80-af5b-11eb-9624-5701e7d59c19.png)

# Device Manager

See below an example of the Device Manager tool. The list of files are containing the sensor data and copy of the configuration file used.

![image](https://user-images.githubusercontent.com/37830964/117474980-d37cfa80-af5b-11eb-8c27-2c5f91c4d288.png)

# Information about sensors

Here you can read about the sensors, scroll down until you passed the API description.

https://docs.tizen.org/application/native/guides/location-sensors/device-sensors/

# Related publications

https://www.universiteitleiden.nl/en/news/2021/11/improving-the-environment-of-people-with-dementia-with-the-help-of-new-software
