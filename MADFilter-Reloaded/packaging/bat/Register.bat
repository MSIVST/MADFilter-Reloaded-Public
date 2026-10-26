@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "RC=1"
set "NOPAUSE="
if /I "%~1"=="/nopause" (
    set "NOPAUSE=1"
    shift /1
)
if /I "%~2"=="/nopause" set "NOPAUSE=1"
if not exist "%~dp0MADFilterRegister.exe" (
    echo ERROR: Keep this script beside MADFilterRegister.exe.
    goto end
)
echo Use Run as administrator for this operation.
echo Registration affects compatible 32-bit apps until you unregister.
"%~dp0MADFilterRegister.exe" /fallout3
set "RC=%ERRORLEVEL%"

:end
if not "%RC%"=="0" echo The operation failed. Check the message above.
if not defined NOPAUSE pause
exit /b %RC%
