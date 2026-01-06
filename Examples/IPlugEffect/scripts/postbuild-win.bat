@echo off
setlocal enabledelayedexpansion

REM - CALL "$(SolutionDir)scripts\postbuild-win.bat" "$(TargetExt)" "$(BINARY_NAME)" "$(Platform)" "$(COPY_VST2)" "$(TargetPath)" "$(VST2_32_PATH)" "$(VST2_64_PATH)" "$(VST3_32_PATH)" "$(VST3_64_PATH)" "$(AAX_32_PATH)" "$(AAX_64_PATH)" "$(CLAP_PATH)" "$(BUILD_DIR)" "$(VST_ICON)" "$(AAX_ICON)" "$(CREATE_BUNDLE_SCRIPT)" "$(ICUDAT_PATH)"

REM Parse arguments
set FORMAT=%~1
set NAME=%~2
set PLATFORM=%~3
set COPY_VST2=%~4
set BUILT_BINARY=%~5
set VST2_32_PATH=%~6
set VST2_64_PATH=%~7 
set VST3_32_PATH=%~8
set VST3_64_PATH=%~9
shift
shift 
shift
shift
shift 
shift
shift
set AAX_32_PATH=%~3
set AAX_64_PATH=%~4
set CLAP_PATH=%~5
set BUILD_DIR=%~6
set VST_ICON=%~7
set AAX_ICON=%~8
set CREATE_BUNDLE_SCRIPT=%~9
shift
set ICUDAT_PATH=%~9

echo ============================================================
echo POSTBUILD: %NAME% [%PLATFORM% %FORMAT%]
echo ============================================================

REM Jump to main logic
goto :main

REM ============================================================
REM Helper: Ensure directory exists, create if needed
REM ============================================================
:EnsureDir
setlocal
set "DIR=%~1"
if exist "%DIR%\*" (
  endlocal
  exit /b 0
)
mkdir "%DIR%" >nul 2>&1
if exist "%DIR%\*" (
  endlocal
  exit /b 0
) else (
  echo [ERROR] Failed to create directory: %DIR%
  endlocal
  exit /b 1
)

REM ============================================================
REM Helper: Copy file with size verification
REM ============================================================
:CopyFile
setlocal
set "SRC=%~1"
set "DST=%~2"

if not exist "%SRC%" (
  echo [SKIP] Source not found: %SRC%
  endlocal
  exit /b 0
)

REM Get destination directory
for %%F in ("%DST%") do set "DST_DIR=%%~dpF"
call :EnsureDir "%DST_DIR%"
if errorlevel 1 (
  endlocal
  exit /b 0
)

REM Copy file
copy /B /Y "%SRC%" "%DST%" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Copy failed: %SRC% -^> %DST%
  endlocal
  exit /b 0
)

REM Verify size
for %%A in ("%SRC%") do set "SRC_SIZE=%%~zA"
for %%A in ("%DST%") do set "DST_SIZE=%%~zA"
if not "%SRC_SIZE%"=="%DST_SIZE%" (
  echo [ERROR] Size mismatch: %SRC% [%SRC_SIZE%] -^> %DST% [%DST_SIZE%]
) else (
  echo [OK] %SRC% -^> %DST%
)

endlocal
exit /b 0

REM ============================================================
REM Helper: Copy directory recursively
REM ============================================================
:CopyDir
setlocal
set "SRC=%~1"
set "DST=%~2"

if not exist "%SRC%\*" (
  echo [SKIP] Source directory not found: %SRC%
  endlocal
  exit /b 0
)

call :EnsureDir "%DST%"
if errorlevel 1 (
  endlocal
  exit /b 0
)

xcopy /E /H /Y "%SRC%\*" "%DST%\" >nul 2>&1
if errorlevel 1 (
  echo [ERROR] XCopy failed: %SRC% -^> %DST%
) else (
  echo [OK] %SRC% -^> %DST%
)

endlocal
exit /b 0

REM ============================================================
REM Helper: Create bundle and copy binary into it
REM ============================================================
:CreateAndCopyToBundle
setlocal
set "BINARY=%~1"
set "BUNDLE_PATH=%~2"
set "ICON=%~3"
set "BUNDLE_SUBDIR=%~4"

echo Creating bundle: %BUNDLE_PATH%
call %CREATE_BUNDLE_SCRIPT% "%BUNDLE_PATH%" "%ICON%" %FORMAT%

set "BUNDLE_BIN_DIR=%BUNDLE_PATH%\Contents\%BUNDLE_SUBDIR%"
call :CopyFile "%BINARY%" "%BUNDLE_BIN_DIR%\%NAME%%FORMAT%"

if exist "%ICUDAT_PATH%" (
  call :CopyFile "%ICUDAT_PATH%" "%BUNDLE_BIN_DIR%\icudtl.dat"
)

endlocal
exit /b 0

REM ============================================================
REM Main logic
REM ============================================================
:main

REM Copy icudtl.dat next to built binary if exists
if exist "%ICUDAT_PATH%" (
  for %%F in ("%BUILT_BINARY%") do (
    call :CopyFile "%ICUDAT_PATH%" "%%~dpFicudtl.dat"
  )
)

REM Handle .exe format
if "%FORMAT%"==".exe" (
  call :CopyFile "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%_%PLATFORM%.exe"
  if exist "%ICUDAT_PATH%" (
    call :CopyFile "%ICUDAT_PATH%" "%BUILD_DIR%\icudtl.dat"
  )
  goto :end
)

REM Handle .dll format (VST2)
if "%FORMAT%"==".dll" (
  call :CopyFile "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%_%PLATFORM%.dll"
  if exist "%ICUDAT_PATH%" (
    call :CopyFile "%ICUDAT_PATH%" "%BUILD_DIR%\icudtl.dat"
  )
  
  if "%COPY_VST2%"=="1" (
    if "%PLATFORM%"=="Win32" (
      echo Installing VST2 [32-bit]...
      call :CopyFile "%BUILT_BINARY%" "%VST2_32_PATH%\%NAME%.dll"
      if exist "%ICUDAT_PATH%" (
        call :CopyFile "%ICUDAT_PATH%" "%VST2_32_PATH%\icudtl.dat"
      )
    )
    if "%PLATFORM%"=="x64" (
      echo Installing VST2 [64-bit]...
      call :CopyFile "%BUILT_BINARY%" "%VST2_64_PATH%\%NAME%.dll"
      if exist "%ICUDAT_PATH%" (
        call :CopyFile "%ICUDAT_PATH%" "%VST2_64_PATH%\icudtl.dat"
      )
    )
  )
  goto :end
)

REM Handle .vst3 format
if "%FORMAT%"==".vst3" (
  if "%PLATFORM%"=="Win32" (
    echo Installing VST3 [32-bit]...
    call :CreateAndCopyToBundle "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.vst3" "%VST_ICON%" "x86-win"
    if exist "%VST3_32_PATH%\*" (
      call :CreateAndCopyToBundle "%BUILT_BINARY%" "%VST3_32_PATH%\%NAME%.vst3" "%VST_ICON%" "x86-win"
    )
  )
  if "%PLATFORM%"=="x64" (
    echo Installing VST3 [64-bit]...
    call :CreateAndCopyToBundle "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.vst3" "%VST_ICON%" "x86_64-win"
    if exist "%VST3_64_PATH%\*" (
      call :CreateAndCopyToBundle "%BUILT_BINARY%" "%VST3_64_PATH%\%NAME%.vst3" "%VST_ICON%" "x86_64-win"
    )
  )
  goto :end
)

REM Handle .aaxplugin format
if "%FORMAT%"==".aaxplugin" (
  if "%PLATFORM%"=="Win32" (
    echo Installing AAX [32-bit]...
    call :CreateAndCopyToBundle "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.aaxplugin" "%AAX_ICON%" "Win32"
    call :CopyDir "%BUILD_DIR%\%NAME%.aaxplugin\Contents" "%AAX_32_PATH%\%NAME%.aaxplugin\Contents"
  )
  if "%PLATFORM%"=="x64" (
    echo Installing AAX [64-bit]...
    call :CreateAndCopyToBundle "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.aaxplugin" "%AAX_ICON%" "x64"
    call :CopyDir "%BUILD_DIR%\%NAME%.aaxplugin\Contents" "%AAX_64_PATH%\%NAME%.aaxplugin\Contents"
  )
  goto :end
)

REM Handle .clap format
if "%FORMAT%"==".clap" (
  if "%PLATFORM%"=="x64" (
    echo Installing CLAP [64-bit]...
    call :CopyFile "%BUILT_BINARY%" "%CLAP_PATH%\%NAME%.clap"
    if exist "%ICUDAT_PATH%" (
      call :CopyFile "%ICUDAT_PATH%" "%CLAP_PATH%\icudtl.dat"
    )
  )
  goto :end
)

:end
echo ============================================================
echo POSTBUILD COMPLETE
echo ============================================================
exit /b 0