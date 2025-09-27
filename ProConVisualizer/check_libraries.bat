@echo off
chcp 65001 >nul
echo Checking wxWidgets libraries...

REM wxWidgetsフォルダを検索
set "WXWIDGETS_PATH="
if exist "C:\wxWidgets\include\wx\wx.h" (
    set "WXWIDGETS_PATH=C:\wxWidgets"
) else if exist "C:\wxWidgets-3.3.1\include\wx\wx.h" (
    set "WXWIDGETS_PATH=C:\wxWidgets-3.3.1"
) else if exist "C:\wxWidgets-3.3.0\include\wx\wx.h" (
    set "WXWIDGETS_PATH=C:\wxWidgets-3.3.0"
) else if exist "C:\wxWidgets-3.2.4\include\wx\wx.h" (
    set "WXWIDGETS_PATH=C:\wxWidgets-3.2.4"
) else (
    echo wxWidgets not found!
    pause
    exit /b 1
)

echo Found wxWidgets at: %WXWIDGETS_PATH%
echo.

set "LIB_DIR=%WXWIDGETS_PATH%\lib\vc_x64_lib"
echo Checking library directory: %LIB_DIR%

if not exist "%LIB_DIR%" (
    echo Library directory does not exist!
    echo Please build wxWidgets first using build_wxwidgets.bat
    pause
    exit /b 1
)

echo.
echo Checking for library files...

echo === Version 3.3 libraries ===
if exist "%LIB_DIR%\wxmsw33u_core.lib" (
    echo ✓ wxmsw33u_core.lib found
) else (
    echo ✗ wxmsw33u_core.lib NOT found
)

if exist "%LIB_DIR%\wxmsw33u_adv.lib" (
    echo ✓ wxmsw33u_adv.lib found
) else (
    echo ✗ wxmsw33u_adv.lib NOT found
)

if exist "%LIB_DIR%\wxbase33u.lib" (
    echo ✓ wxbase33u.lib found
) else (
    echo ✗ wxbase33u.lib NOT found
)

echo.
echo === Version 3.2 libraries ===
if exist "%LIB_DIR%\wxmsw32u_core.lib" (
    echo ✓ wxmsw32u_core.lib found
) else (
    echo ✗ wxmsw32u_core.lib NOT found
)

if exist "%LIB_DIR%\wxmsw32u_adv.lib" (
    echo ✓ wxmsw32u_adv.lib found
) else (
    echo ✗ wxmsw32u_adv.lib NOT found
)

if exist "%LIB_DIR%\wxbase32u.lib" (
    echo ✓ wxbase32u.lib found
) else (
    echo ✗ wxbase32u.lib NOT found
)

echo.
echo === All .lib files in directory ===
dir "%LIB_DIR%\*.lib" /b

echo.
echo If no libraries are found, please run build_wxwidgets.bat first.
pause