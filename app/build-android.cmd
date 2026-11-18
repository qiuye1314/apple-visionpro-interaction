@echo off
setlocal
where py.exe >nul 2>nul
if not errorlevel 1 (
    py -3 "%~dp0..\build_android.py" %*
    exit /b
)
python "%~dp0..\build_android.py" %*
