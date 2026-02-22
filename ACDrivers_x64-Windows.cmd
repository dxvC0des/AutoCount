:: Copylefted (L)2025 - dxvProjects, AutoCount_STEM
@echo off
title AutoCount Drivers Installer
echo Info:
echo This files is for the DRIVERS setup in order for the program to work.
echo all of these files are for x64 Windows OS.
echo Do you want to install these drivers?
CHOICE /C 12 /M "[Y = 1] [N = 2]"

:: Note - list ERRORLEVELS in decreasing order
IF ERRORLEVEL 2 GOTO Exit
IF ERRORLEVEL 1 GOTO Install_pckg

:Install_pckg
cls
echo Packages Installed:
echo Spacedesk Server Driver x64 Windows
echo Microsoft Visual C++ Redistributable
echo Apple Drivers (iTunes)
echo Confirm packages installed. Y/N
CHOICE /C 12 /M "[Y = 1] [N = 2]"

:: Note - list ERRORLEVELS in decreasing order
IF ERRORLEVEL 2 GOTO Exit
IF ERRORLEVEL 1 GOTO NoticeAccept

:NoticeAccept
cls 
mkdir DriverTMP
bitsadmin /transfer Microsoft_Visual_C++_Redistributable https://aka.ms/vc14/vc_redist.x64.exe "%~dp0DriverTMP\setupms.exe"
bitsadmin /transfer SpacedeskDriverInstall https://www.spacedesk.net/downloadidd64 "%~dp0DriverTMP\spacedesksetup.exe"
bitsadmin /transfer iTunes https://www.apple.com/itunes/download/win64 "%~dp0DriverTMP\win64_itunes.exe"
cd DriverTMP
echo Installing Microsoft Visual C++ Redistributable
setupms.exe /install /quiet /norestart
spacedesksetup.exe
win64_itunes.exe
cd..
rmdir DriverTMP
cls
echo Installation Complete!
pause
explorer .
del ACDrivers_x64-Windows.cmd
del ACDrivers_x64-Windows.bat
exit


:Exit
cls
echo Exiting Program In 5 Seconds.
timeout 5
exit
