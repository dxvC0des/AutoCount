:: Copylefted (L)2025 - dxvProjects, AutoCount_STEM
@echo off
title AutoCount Setup Installer
echo Info:
echo The file you ran will install in the current directory. (%~dp0)
echo If you dont want to install in the directory, move it to the correct directory you want to install it in.
echo Confirm if you want to install in this directory.
CHOICE /C 12 /M "[Y = 1] [N = 2]"

:: Note - list ERRORLEVELS in decreasing order
IF ERRORLEVEL 2 GOTO Exit
IF ERRORLEVEL 1 GOTO Install_pckg

:Install_pckg
cls
echo Packages Installed:
echo AutoCount Web Service
echo AutoCount IDE Detection
echo AutoCount Kiosk Launch
echo AutoCount WebServer
echo All files are located at our CDN Service at 
echo https://distro.codingfr.com/autocount/setup/
echo Confirm packages installed. Y/N - 264MB Total
CHOICE /C 12 /M "[Y = 1] [N = 2]"

:: Note - list ERRORLEVELS in decreasing order
IF ERRORLEVEL 2 GOTO Exit
IF ERRORLEVEL 1 GOTO NoticeAccept

:NoticeAccept
cls 
mkdir TempFolder_DND
mkdir WebServer
bitsadmin /transfer Requirements https://distro.codingfr.com/autocount/setup/ACDependencies_MainFiles-U2D.zip "%~dp0TempFolder_DND\ACDependencies_MainFiles-U2D.zip"
bitsadmin /transfer ClientLauncher https://distro.codingfr.com/autocount/setup/ACSrcClient.zip "%~dp0TempFolder_DND\ACSrcClient.zip"
bitsadmin /transfer WebServer https://distro.codingfr.com/autocount/setup/ACWebServer_LocalHost.zip "%~dp0TempFolder_DND\ACWebServer_LocalHost.zip"
cd TempFolder_DND
move "ACDependencies_MainFiles-U2D.zip" "%~dp0\ACDependencies_MainFiles-U2D.zip"
move "ACSrcClient.zip" "%~dp0\ACSrcClient.zip"
move "ACWebServer_LocalHost.zip" "%~dp0\WebServer\ACWebServer_LocalHost.zip"
cd..
tar -xf ACDependencies_MainFiles-U2D.zip
tar -xf ACSrcClient.zip
del ACDependencies_MainFiles-U2D.zip
del ACSrcClient.zip
cd WebServer
tar -xf ACWebServer_LocalHost.zip
del ACWebServer_LocalHost.zip
cd..
rmdir TempFolder_DND
cls
echo Installation Complete!
pause
explorer .
del ACSetup_x86_x64-Windows.cmd
del ACSetup_x86_x64-Windows.cmd
exit


:Exit
cls
echo Exiting Program In 5 Seconds.
timeout 5
exit
