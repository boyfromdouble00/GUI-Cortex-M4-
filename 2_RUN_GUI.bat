@echo off
call "%~dp0PC_GUI\run_gui.bat"
if errorlevel 1 goto GUI_ERROR
exit /b 0

:GUI_ERROR
echo.
echo GUI execution failed. Check the error above.
pause
exit /b 1
