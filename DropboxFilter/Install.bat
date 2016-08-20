@echo off

REM the DLL should be in the same directory as this script
set "dllpath=%~dp0"
set "dllpath=%dllpath%DropboxFilter.dll"

REM determine OS type
if %PROCESSOR_ARCHITECTURE% == x86 (
	echo Installing for x86 system
	set "key=HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Windows"
) else (
	echo Installing for x64 system
	set "key=HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\Windows"
)

echo.

REM backup registry
echo Backing up registry
reg export "%key%" backup.reg

echo.

REM add the required entries
echo Adding entry AppInit_DLLs
reg add "%key%" /v AppInit_DLLs /t REG_SZ /d "%dllpath%"

echo Adding entry LoadAppInit_DLLs
reg add "%key%" /v LoadAppInit_DLLs /t REG_DWORD /d 1

echo Adding entry RequireSignedAppInit_DLLs
reg add "%key%" /v RequireSignedAppInit_DLLs /t REG_DWORD /d 0

echo.

REM done
echo Finished!
echo If there were no errors, you can now restart Dropbox to activate the filter!

echo.

pause
