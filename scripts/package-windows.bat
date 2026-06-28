@echo off
setlocal enabledelayedexpansion
REM Build a portable Windows x64 zip from Linux via MinGW cross-compiler.
REM Requires: mingw-w64 (x86_64-w64-mingw32-g++), cmake, zip

set "ROOT=%~dp0.."
set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=0.1.0"

set "BUILD=%ROOT%\build-win"
set "NAME=QuantumCircuitLab-%VERSION%-win64"
set "DIST=%ROOT%\dist\%NAME%"
set "ARCHIVE=%ROOT%\dist\%NAME%.zip"

where x86_64-w64-mingw32-g++ >nul 2>&1
if errorlevel 1 (
    echo ERROR: Install MinGW-w64 cross compiler first.
    echo   Debian/Kali: sudo apt install mingw-w64 zip
    exit /b 1
)

echo ==^> Configuring Windows cross-build...
cmake -B "%BUILD%" -DCMAKE_BUILD_TYPE=Release -DQSIM_BUILD_TESTS=OFF ^
    -DCMAKE_TOOLCHAIN_FILE="%ROOT%\cmake\mingw-w64-x86_64.cmake"
if errorlevel 1 exit /b 1

echo ==^> Building quantum-lab.exe...
cmake --build "%BUILD%" --target quantum-lab -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1

set "BIN=%BUILD%\ui\quantum-lab.exe"
if not exist "%BIN%" (
    echo ERROR: Build output not found: %BIN%
    exit /b 1
)

echo ==^> Assembling portable bundle...
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%\presets"
copy /y "%BIN%" "%DIST%\quantum-lab.exe" >nul
xcopy /e /i /y "%ROOT%\presets\*" "%DIST%\presets\" >nul
copy /y "%ROOT%\packaging\run-quantum-lab.bat" "%DIST%\run.bat" >nul
copy /y "%ROOT%\packaging\README-portable-windows.txt" "%DIST%\README.txt" >nul

if not exist "%ROOT%\dist" mkdir "%ROOT%\dist"
if exist "%ARCHIVE%" del /f "%ARCHIVE%"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST%\*' -DestinationPath '%ARCHIVE%' -Force"

echo.
echo Done.
echo   Folder:  %DIST%
echo   Archive: %ARCHIVE%
echo.
echo Share the .zip file. Recipients extract and run run.bat
endlocal
