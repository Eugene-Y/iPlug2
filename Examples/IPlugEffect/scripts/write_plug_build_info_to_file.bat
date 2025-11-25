@echo off
setlocal enabledelayedexpansion

for /f %%i in ('git rev-parse --verify HEAD') do set commit=%%i
set commit=%commit:~0,7%

for /f %%i in ('git rev-parse --abbrev-ref HEAD') do set branch_name=%%i

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format 'yyyy.MM.dd'"') do set datestr=%%i

for /f "tokens=1-2 delims=: " %%a in ("%time%") do (
    set hh=%%a
    set min=%%b
)
for /f "tokens=3 delims=:." %%a in ("%time%") do set ss=%%a
set timestr=%hh%:%min%:%ss%

REM ARCHS expected to be after the first 2 arguments
set archs=
set s=
set n=1

for %%i in (%*) do (
    if !n! gtr 2 (
        set archs=!archs!!s!%%i
        set s= 
    )
    set /a n+=1
)

set FILE_PATH=%1plug_build_info.hpp

(
echo //
echo //  plug_build_info.hpp
echo //
echo //  Created automatically by Xcode on %datestr% at %timestr%.
echo //
echo.
echo #ifndef plug_build_info_hpp
echo #define plug_build_info_hpp
echo.
echo #define PLUG_GIT_BRANCH_NAME "%branch_name%"
echo #define PLUG_GIT_COMMIT_SHA  "%commit%"
echo #define PLUG_BUILD_DATE      "%datestr%"
echo #define PLUG_BUILD_TIME      "%timestr%"
echo #define PLUG_PRODUCT         "%~2"
echo #define PLUG_ARCHS           %archs%
echo.
echo #endif
) > "%FILE_PATH%"

endlocal