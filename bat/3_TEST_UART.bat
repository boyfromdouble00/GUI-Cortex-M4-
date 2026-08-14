@echo off
chcp 65001 >nul
cd /d "%~dp0"

set "TEST_PYTHON=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%TEST_PYTHON%" goto TEST_START
goto PYTHON_ERROR

:TEST_START
"%TEST_PYTHON%" "%~dp0PC_GUI\serial_test.py" COM5
echo.
pause
exit /b 0

:PYTHON_ERROR
echo ERROR: PlatformIO embedded Python was not found.
echo Run 1_UPLOAD_FIRMWARE.bat first.
pause
exit /b 1
