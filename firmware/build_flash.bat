@echo off
REM ==========================================================================
REM Build -^> flash with OpenOCD -^> reset and run (no GDB, device runs free).
REM Call from CLion External Tool or command line.
REM   Program:           %ProjectFileDir%\build_flash.bat
REM   Working directory: %ProjectFileDir%
REM ==========================================================================
setlocal

cd /d "%~dp0"

set "BUILD_DIR=cmake-build-debug"
set "ELF=%BUILD_DIR%/ehhhkb2.elf"

set "NINJA=%USERPROFILE%\.pico-sdk\ninja\v1.12.1\ninja.exe"
set "OPENOCD=%USERPROFILE%\.pico-sdk\openocd\0.12.0+dev\openocd.exe"
set "OCD_SCRIPTS=%USERPROFILE%\.pico-sdk\openocd\0.12.0+dev\scripts"

echo === Build ===
"%NINJA%" -C "%BUILD_DIR%"
if errorlevel 1 (
    echo Build failed. Aborting flash.
    exit /b 1
)

echo === Flash ^& Run ===
"%OPENOCD%" -s "%OCD_SCRIPTS%" -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000; program \"%ELF%\" verify reset exit"
if errorlevel 1 (
    echo Flash failed.
    exit /b 1
)

echo === Done: device reset and running ===
endlocal
