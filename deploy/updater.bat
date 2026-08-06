@echo off
REM updater.bat - hands off exe self-replacement from the running game.
REM The game has already downloaded the new exe in full before invoking this script; this script
REM does no networking of its own. It waits for the old process to exit (DMX.exe cannot delete or
REM overwrite its own running image), swaps in the new exe, records the applied release tag, and
REM relaunches. Unlike the ephemeral, server-downloaded data-update.bat, this file is a permanent,
REM tracked part of every install and must never delete itself.
REM
REM usage: updater.bat OldExeName.exe NewExeName.exe ReleaseTag

setlocal

set OLDEXE=%~1
set NEWEXE=%~2
set RELEASETAG=%~3

if "%OLDEXE%"=="" goto :usage
if "%NEWEXE%"=="" goto :usage

REM wait for the old process to exit - bounded to ~30 seconds, never infinite
set /a ATTEMPTS=0
:waitloop
tasklist /FI "IMAGENAME eq %OLDEXE%" 2>NUL | find /I "%OLDEXE%" >NUL
if errorlevel 1 goto :swap
set /a ATTEMPTS+=1
if %ATTEMPTS% GEQ 30 goto :swap
timeout /t 1 /nobreak >NUL
goto :waitloop

:swap
REM retry the move a couple times in case of a transient AV/file lock
set /a MOVEATTEMPTS=0
:moveloop
move /Y "%NEWEXE%" "%OLDEXE%" >NUL 2>&1
if exist "%NEWEXE%" (
	set /a MOVEATTEMPTS+=1
	if %MOVEATTEMPTS% GEQ 3 goto :relaunch
	timeout /t 1 /nobreak >NUL
	goto :moveloop
)

if not "%RELEASETAG%"=="" (
	echo %RELEASETAG%> exe_version.txt
)

:relaunch
start "" "%OLDEXE%"
goto :eof

:usage
echo Usage: updater.bat OldExeName.exe NewExeName.exe ReleaseTag
goto :eof
