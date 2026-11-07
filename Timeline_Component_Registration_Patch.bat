@echo off
echo Timeline Component Registration
echo ================================
echo This must be run as Administrator!
echo.

cd /d "%~dp0"
echo Working directory: %CD%
echo.

echo Registering ActiveScriptor.ocx...
regsvr32 /s ActiveScriptor.ocx
if errorlevel 1 echo   FAILED! else echo   OK

echo.
echo Registering Component DLLs...
cd Components
for %%f in (*.dll) do (
    echo   Registering %%f...
    regsvr32 /s "%%f"
    if errorlevel 1 echo     FAILED! else echo     OK
)
cd ..

echo.
echo Verifying registration...
reg query "HKCR\TLS.EventMaster" >nul 2>&1
if errorlevel 1 (
    echo   FAILED: TLS.EventMaster not found in registry!
    echo   Registration did not work. Try running this as Administrator.
) else (
    echo   SUCCESS: Components registered correctly!
)

echo.
pause
