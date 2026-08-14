@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ================================================
echo TRAFFIC MOTOR PWM GUI
echo ================================================
echo.

set "GUI_PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%GUI_PYTHON%" goto PYTHON_READY
goto NO_PYTHON

:PYTHON_READY
"%GUI_PYTHON%" --version
if errorlevel 1 goto NO_PYTHON

echo.
echo [1/2] Installing GUI packages...
"%GUI_PYTHON%" -m pip install -r requirements.txt
if errorlevel 1 goto INSTALL_ERROR

echo.
echo [2/2] Starting GUI...
"%GUI_PYTHON%" main.py
if errorlevel 1 goto GUI_ERROR

exit /b 0

:NO_PYTHON
echo.
echo ERROR: PlatformIO embedded Python was not found.
echo Run 1_UPLOAD_FIRMWARE.bat first.
pause
exit /b 1

:INSTALL_ERROR
echo.
echo ERROR: PyQt5 or pyserial installation failed.
pause
exit /b 1

:GUI_ERROR
echo.
echo ERROR: GUI failed to start.
pause
exit /b 1
