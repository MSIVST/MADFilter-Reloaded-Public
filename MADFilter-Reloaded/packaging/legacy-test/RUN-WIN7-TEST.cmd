@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "LOG=%~dp0MADFilter-Test-Results.txt"
set "STATUS_TMP=%TEMP%\madfilter-status-%RANDOM%.txt"
set "REGISTERED=0"
set "TEST_EXIT=99"
set "UNREGISTER_EXIT=99"
set "CLEANUP_OK=0"

> "%LOG%" echo MADFilter Reloaded Windows 7 integration test
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo.
>> "%LOG%" ver
>> "%LOG%" echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
>> "%LOG%" echo PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432%
>> "%LOG%" echo.

ver | findstr /L /C:"5.1." /C:"5.2." >nul
if not errorlevel 1 goto unsupported_os

net session >nul 2>&1
if errorlevel 1 goto need_admin

"%~dp0MADFilterRegister.exe" /status > "%STATUS_TMP%" 2>&1
type "%STATUS_TMP%" >> "%LOG%"
findstr /C:"Status: NOT REGISTERED" "%STATUS_TMP%" >nul
if errorlevel 1 goto already_registered

>> "%LOG%" echo.
>> "%LOG%" echo Registering Fallout 3 merit...
"%~dp0MADFilterRegister.exe" /fallout3 >> "%LOG%" 2>&1
if errorlevel 1 goto registration_failed
set "REGISTERED=1"

>> "%LOG%" echo.
>> "%LOG%" echo Running DirectShow graph test...
"%~dp0filter_graph_test.exe" "%~dp0test-fixture-30s.mp3" >> "%LOG%" 2>&1
set "TEST_EXIT=%ERRORLEVEL%"
goto cleanup

:need_admin
>> "%LOG%" echo ERROR: Administrator rights are required.
echo ERROR: Open Command Prompt as Administrator, then run this script again.
set "TEST_EXIT=10"
goto finish

:unsupported_os
>> "%LOG%" echo ERROR: Windows XP and Server 2003 cannot run this Windows 6.0 build.
echo ERROR: This folder contains Windows 7 test binaries, not XP binaries.
echo Run the collector in the package XP folder instead.
set "TEST_EXIT=12"
goto finish

:already_registered
>> "%LOG%" echo ERROR: MADFilter was already registered before this test.
echo ERROR: Existing MADFilter registration detected. No changes were made.
echo Review "%LOG%".
set "TEST_EXIT=11"
goto finish

:registration_failed
>> "%LOG%" echo ERROR: Registration failed.
set "TEST_EXIT=20"
goto cleanup

:cleanup
>> "%LOG%" echo.
>> "%LOG%" echo Unregistering and checking cleanup...
"%~dp0MADFilterRegister.exe" /unregister >> "%LOG%" 2>&1
set "UNREGISTER_EXIT=%ERRORLEVEL%"
set "REGISTERED=0"

"%~dp0MADFilterRegister.exe" /status > "%STATUS_TMP%" 2>&1
type "%STATUS_TMP%" >> "%LOG%"
findstr /C:"Status: NOT REGISTERED" "%STATUS_TMP%" >nul
if errorlevel 1 goto cleanup_failed
set "CLEANUP_OK=1"
goto finish

:cleanup_failed
>> "%LOG%" echo ERROR: Registration cleanup could not be confirmed.
echo WARNING: Cleanup could not be confirmed.
echo Run MADFilterRegister.exe /unregister from an Administrator prompt.

:finish
if exist "%STATUS_TMP%" del /q "%STATUS_TMP%" >nul 2>&1
>> "%LOG%" echo.
>> "%LOG%" echo TEST_EXIT=%TEST_EXIT%
>> "%LOG%" echo UNREGISTER_EXIT=%UNREGISTER_EXIT%
>> "%LOG%" echo CLEANUP_OK=%CLEANUP_OK%

if not "%TEST_EXIT%"=="0" goto failed
if not "%UNREGISTER_EXIT%"=="0" goto failed
if not "%CLEANUP_OK%"=="1" goto failed

>> "%LOG%" echo RESULT=PASS
echo RESULT=PASS
echo Results: "%LOG%"
exit /b 0

:failed
>> "%LOG%" echo RESULT=FAIL
echo RESULT=FAIL
echo Results: "%LOG%"
exit /b 1
