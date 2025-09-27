@echo off
chcp 65001 >nul
echo Pro Controller Visualizer Build Script

REM Visual Studio を検索
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio not found.
    echo Please install Visual Studio 2019 or later.
    pause
    exit /b 1
)

REM MSBuildのパスを取得
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VSPATH=%%i"
)

if not defined VSPATH (
    echo Visual Studio MSBuild not found.
    pause
    exit /b 1
)

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
    echo wxWidgets not found in common locations.
    echo Looking for wxWidgets folders...
    for /d %%D in ("C:\wxWidgets*") do (
        if exist "%%D\include\wx\wx.h" (
            set "WXWIDGETS_PATH=%%D"
            goto wxfound
        )
    )
    
    echo wxWidgets not found. Please:
    echo 1. Download wxWidgets from https://www.wxwidgets.org/downloads/
    echo 2. Extract to C:\wxWidgets or run build_wxwidgets.bat
    pause
    exit /b 1
)

:wxfound
echo Found wxWidgets at: %WXWIDGETS_PATH%

set "MSBUILD=%VSPATH%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
    set "MSBUILD=%VSPATH%\MSBuild\15.0\Bin\MSBuild.exe"
)

if not exist "%MSBUILD%" (
    echo MSBuild not found.
    pause  
    exit /b 1
)

echo Building with MSBuild...
set "WXWIDGETS_ROOT=%WXWIDGETS_PATH%"
"%MSBUILD%" ProConVisualizer.sln /p:Configuration=Release /p:Platform=x64 /p:CharacterSet=Unicode /p:WXWIDGETS_ROOT="%WXWIDGETS_PATH%"

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    echo Please check:
    echo 1. wxWidgets is installed at %WXWIDGETS_PATH%
    echo 2. wxWidgets is built (run build_wxwidgets.bat manually first)
    echo 3. Check if the following files exist:
    echo    - %WXWIDGETS_PATH%\lib\vc_x64_lib\wxmsw33u_core.lib
    echo    - %WXWIDGETS_PATH%\lib\vc_x64_lib\wxmsw33u_adv.lib
    echo    - %WXWIDGETS_PATH%\lib\vc_x64_lib\wxbase33u.lib
    pause
    exit /b 1
)

echo Build completed successfully!
echo Executable: x64\Release\ProConVisualizer.exe

pause