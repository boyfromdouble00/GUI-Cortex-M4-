@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ================================================
echo STEP 1: FIRMWARE
echo ================================================

cd STM32_Firmware
if exist "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" goto RUN_PIO_INSTALLED

py -3.11 --version
if errorlevel 1 goto NO_PYTHON
py -3.11 -m pip install platformio
if errorlevel 1 goto FIRMWARE_ERROR
set "PIO_COMMAND=py -3.11 -m platformio"
goto RUN_BUILD

:RUN_PIO_INSTALLED
set "PIO_COMMAND=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"

:RUN_BUILD
%PIO_COMMAND% run
if errorlevel 1 goto FIRMWARE_ERROR

%PIO_COMMAND% run -t upload
if errorlevel 1 goto FIRMWARE_ERROR

echo.
echo ================================================
echo STEP 2: GUI
echo ================================================

cd ..\PC_GUI
set "GUI_PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%GUI_PYTHON%" goto RUN_GUI
goto GUI_ERROR

:RUN_GUI
"%GUI_PYTHON%" -m pip install -r requirements.txt
if errorlevel 1 goto GUI_ERROR

"%GUI_PYTHON%" main.py
if errorlevel 1 goto GUI_ERROR
exit /b 0

:NO_PYTHON
echo.
echo ERROR: Windows Python 3.11 was not found.
echo The MSYS2 Python cannot be used for this project setup.
pause
exit /b 1

:FIRMWARE_ERROR
echo.
echo ERROR: Firmware build or upload failed.
pause
exit /b 1

:GUI_ERROR
echo.
echo ERROR: GUI package installation or execution failed.
pause
exit /b 1
