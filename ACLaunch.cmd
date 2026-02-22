@echo off
title AutoCountLaunch.exe
echo -------------------------------
echo Make sure that AutoCount is plugged into a kiosk.
echo Press any key to accept notification.
echo Press CTRL+C To Quit Application.
echo -------------------------------
pause
cls
:start
echo This file will automatically restart when crashed. Press CTRL + C To stop
echo AutoCount is OpenSource @ https://github.com/dxvC0des/AutoCount
cd AutoCount_Dependencies
C:\Windows\SysWOW64\cmd.exe /c cscript AutoLaunchWEB.vbs
AutoCountLaunch.exe
taskkill /f /im AutoCountLaunch.exe
cls
goto start
taskkill /f /im cmd.exe