@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ================================================
echo LOCAL LLM SETUP - OLLAMA + QWEN2.5 3B
echo ================================================
echo.

where ollama >nul 2>nul
if errorlevel 1 goto OLLAMA_NOT_FOUND

echo Ollama was found.
ollama --version
echo.
echo Downloading qwen2.5:3b (about 1.9 GB)...
ollama pull qwen2.5:3b
if errorlevel 1 goto MODEL_ERROR

echo.
echo ================================================
echo SUCCESS: LOCAL LLM IS READY
echo Run 2_RUN_GUI.bat and use the AI manager panel.
echo ================================================
pause
exit /b 0

:OLLAMA_NOT_FOUND
echo ERROR: Ollama was not found.
echo 1. Install Ollama for Windows from https://ollama.com/download/windows
echo 2. Start Ollama once from the Windows Start menu.
echo 3. Close this window and run 4_SETUP_LOCAL_LLM.bat again.
pause
exit /b 1

:MODEL_ERROR
echo ERROR: qwen2.5:3b download failed.
echo Check the internet connection and that Ollama is running.
pause
exit /b 1
