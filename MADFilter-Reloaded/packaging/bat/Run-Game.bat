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
set "GAME=%~1"
if not defined GAME set /p "GAME=Enter the full path to the game executable: "
if not defined GAME (
    echo ERROR: No executable specified.
    goto end
)
set "GAME=%GAME:"=%"
if not exist "%GAME%" (
    echo ERROR: The game executable was not found.
    goto end
)
echo Use Run as administrator for this operation.
echo Leave this window open until the game exits and cleanup completes.
echo Other compatible 32-bit applications may use the filter during this session.
"%~dp0MADFilterRegister.exe" /launch "%GAME%"
set "RC=%ERRORLEVEL%"

:end
if not "%RC%"=="0" echo The operation failed. Check the message above.
if not defined NOPAUSE pause
exit /b %RC%
