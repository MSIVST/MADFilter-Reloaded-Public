@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "LOG=%~dp0MADFilter-XP-Test-Results.txt"
set "STATUS_TMP=%TEMP%\madfilter-xp-status-%RANDOM%.txt"
set "TEST_EXIT=99"
set "UNREGISTER_EXIT=99"
set "CLEANUP_OK=0"

> "%LOG%" echo MADFilter Reloaded Windows XP integration test
>> "%LOG%" echo Started: %DATE% %TIME%
>> "%LOG%" echo.
>> "%LOG%" ver
>> "%LOG%" echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%
>> "%LOG%" echo PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432%
>> "%LOG%" echo.

ver | findstr /L /C:"5.1." /C:"5.2." >nul
if errorlevel 1 goto unsupported_os

net session >nul 2>&1
if errorlevel 1 goto need_admin

"%~dp0MADFilterRegister.exe" /status > "%STATUS_TMP%" 2>&1
type "%STATUS_TMP%" >> "%LOG%"
findstr /C:"Status: NOT REGISTERED" "%STATUS_TMP%" >nul
if errorlevel 1 goto already_registered

>> "%LOG%" echo.
>> "%LOG%" echo Running boundary tests...
"%~dp0boundary_test.exe" >> "%LOG%" 2>&1
if errorlevel 1 goto boundary_failed

>> "%LOG%" echo.
>> "%LOG%" echo Running filter load test...
"%~dp0filter_load_test.exe" "%~dp0MADFilterReloaded.ax" >> "%LOG%" 2>&1
if errorlevel 1 goto load_failed

>> "%LOG%" echo.
>> "%LOG%" echo Running direct filter sample test...
"%~dp0filter_sample_test.exe" >> "%LOG%" 2>&1
if errorlevel 1 goto sample_failed

>> "%LOG%" echo.
>> "%LOG%" echo Running decoder offset and fragmentation suite...
"%~dp0maddec_test.exe" --test "%~dp0test-fixture-30s.mp3" >> "%LOG%" 2>&1
if errorlevel 1 goto decoder_failed

>> "%LOG%" echo.
>> "%LOG%" echo Replaying MP3 through fuzz harness...
"%~dp0madfuzz.exe" "%~dp0test-fixture-30s.mp3" >> "%LOG%" 2>&1
if errorlevel 1 goto fuzz_failed

>> "%LOG%" echo.
>> "%LOG%" echo Registering Fallout 3 merit...
"%~dp0MADFilterRegister.exe" /fallout3 >> "%LOG%" 2>&1
if errorlevel 1 goto registration_failed

>> "%LOG%" echo.
>> "%LOG%" echo Running registered DirectShow graph test...
"%~dp0filter_graph_test.exe" "%~dp0test-fixture-30s.mp3" >> "%LOG%" 2>&1
set "TEST_EXIT=%ERRORLEVEL%"
goto cleanup

:need_admin
>> "%LOG%" echo ERROR: Administrator rights are required.
echo ERROR: Open Command Prompt as Administrator, then run this script again.
set "TEST_EXIT=10"
goto finish

:unsupported_os
>> "%LOG%" echo ERROR: This script requires Windows XP or Windows Server 2003.
echo ERROR: This folder contains XP-targeted binaries.
echo Use the Win7 folder on Windows 7.
set "TEST_EXIT=12"
goto finish

:already_registered
>> "%LOG%" echo ERROR: MADFilter was already registered before this test.
echo ERROR: Existing MADFilter registration detected. No changes were made.
echo Review "%LOG%".
set "TEST_EXIT=11"
goto finish

:boundary_failed
set "TEST_EXIT=31"
goto offline_failed

:load_failed
set "TEST_EXIT=32"
goto offline_failed

:sample_failed
set "TEST_EXIT=33"
goto offline_failed

:decoder_failed
set "TEST_EXIT=34"
goto offline_failed

:fuzz_failed
set "TEST_EXIT=35"

:offline_failed
>> "%LOG%" echo ERROR: A pre-registration test failed.
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
