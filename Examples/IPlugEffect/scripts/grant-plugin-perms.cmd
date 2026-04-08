@echo off
setlocal

REM Usage: Run as Administrator (right-click -> Run as administrator).
REM Optional first arg: /full  -> grant Full control instead of Modify.

if /I "%~1"=="/full" (
  set "ACL=(OI)(CI)F"
) else (
  set "ACL=(OI)(CI)M"
)

REM check for elevation
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
  echo [ERROR] This script requires elevation. Open an elevated Command Prompt and rerun.
  exit /b 1
)

set "USER=%USERNAME%"

echo Granting permissions to %USER% with ACL %ACL% on common plugin folders...
echo.

for %%F in (
  "%ProgramFiles%\VstPlugins"
  "%ProgramFiles%\Common Files\VST3"
  "%ProgramFiles%\Common Files\Avid\Audio\Plug-Ins"
  "%ProgramFiles%\Common Files\CLAP"
) do (
  set "TARGET=%%~F"
  echo [POSTBUILD] Processing "%TARGET%"
  if not exist "%%~F" (
    echo [POSTBUILD] Creating "%TARGET%"...
    mkdir "%%~F" >nul 2>&1
    if exist "%%~F" (
      echo [POSTBUILD][OK] Created "%TARGET%"
    ) else (
      echo [POSTBUILD][WARNING] Could not create "%TARGET%" (permissions?)
    )
  )
  echo [POSTBUILD] icacls "%%~F" /grant "%USER%:%ACL%" /T
  icacls "%%~F" /grant "%USER%:%ACL%" /T
  if %ERRORLEVEL% neq 0 (
    echo [POSTBUILD][ERROR] icacls failed on "%%~F"
  ) else (
    echo [POSTBUILD][OK] Permissions set on "%%~F"
  )
  echo.
)

echo Done.
endlocal
exit /b 0