@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "OUT=%~dp0XP-System-Info.txt"
> "%OUT%" echo MADFilter Reloaded XP system preflight
>> "%OUT%" echo Collected: %DATE% %TIME%
>> "%OUT%" echo.
>> "%OUT%" ver
>> "%OUT%" echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
>> "%OUT%" echo PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432%
>> "%OUT%" echo NUMBER_OF_PROCESSORS=%NUMBER_OF_PROCESSORS%
>> "%OUT%" echo.
>> "%OUT%" reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /v ProductName
>> "%OUT%" reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /v CSDVersion
>> "%OUT%" reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /v CurrentVersion
>> "%OUT%" reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /v CurrentBuildNumber

echo Wrote "%OUT%"
echo Return this file with the test machine details.
exit /b 0
