@echo off
chcp 65001 >nul
cd /d "%~dp0STM32_Firmware"

echo ================================================
echo NUCLEO-F411RE FIRMWARE BUILD AND UPLOAD
echo ================================================
echo.

if exist "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" goto PIO_INSTALLED

echo PlatformIO executable was not found. Installing with Python 3.11...
py -3.11 --version
if errorlevel 1 goto NO_PYTHON
py -3.11 -m pip install platformio
if errorlevel 1 goto INSTALL_ERROR
set "PIO_COMMAND=py -3.11 -m platformio"
goto BUILD_START

:PIO_INSTALLED
set "PIO_COMMAND=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"

:BUILD_START

echo.
echo [1/2] Building firmware...
%PIO_COMMAND% run
if errorlevel 1 goto BUILD_ERROR

echo.
echo [2/2] Uploading firmware...
%PIO_COMMAND% run -t upload
if errorlevel 1 goto UPLOAD_ERROR

echo.
echo ================================================
echo SUCCESS: FIRMWARE UPLOAD COMPLETE
echo ================================================
pause
exit /b 0

:NO_PYTHON
echo.
echo ERROR: Windows Python 3.11 was not found.
echo Install Python 3.11 or check the py launcher.
pause
exit /b 1

:INSTALL_ERROR
echo.
echo ERROR: PlatformIO installation failed.
pause
exit /b 1

:BUILD_ERROR
echo.
echo ERROR: Firmware build failed.
pause
exit /b 1

:UPLOAD_ERROR
echo.
echo ERROR: Firmware upload failed.
echo Check the ST-LINK USB connection and close the GUI.
pause
exit /b 1
