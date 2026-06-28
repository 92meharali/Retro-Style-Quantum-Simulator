@echo off
REM Launcher for portable Quantum Circuit Lab (Windows)
cd /d "%~dp0"
start "" "%~dp0quantum-lab.exe" "%~dp0presets"
