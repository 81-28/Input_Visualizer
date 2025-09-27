@echo off
chcp 65001 >nul
echo Building wxWidgets...

REM wxWidgetsフォルダを検索
set "WXWIDGETS_PATH="
if exist "C:\wxWidgets" (
    set "WXWIDGETS_PATH=C:\wxWidgets"
) else if exist "C:\wxWidgets-3.3.1" (
    set "WXWIDGETS_PATH=C:\wxWidgets-3.3.1"
) else if exist "C:\wxWidgets-3.3.0" (
    set "WXWIDGETS_PATH=C:\wxWidgets-3.3.0"
) else if exist "C:\wxWidgets-3.2.4" (
    set "WXWIDGETS_PATH=C:\wxWidgets-3.2.4"
) else (
    echo wxWidgets not found in common locations.
    echo Looking for wxWidgets folders...
    for /d %%D in ("C:\wxWidgets*") do (
        if exist "%%D\include\wx\wx.h" (
            set "WXWIDGETS_PATH=%%D"
            goto found
        )
    )
    
    echo wxWidgets not found. Please:
    echo 1. Download wxWidgets from https://www.wxwidgets.org/downloads/
    echo 2. Extract to C:\wxWidgets or C:\wxWidgets-x.x.x
    pause
    exit /b 1
)

:found
echo Found wxWidgets at: %WXWIDGETS_PATH%

if not exist "%WXWIDGETS_PATH%\include\wx\wx.h" (
    echo wxWidgets headers not found at %WXWIDGETS_PATH%
    echo Please check your wxWidgets installation
    pause
    exit /b 1
)

cd /d "%WXWIDGETS_PATH%\build\msw"

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

set "MSBUILD=%VSPATH%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
    set "MSBUILD=%VSPATH%\MSBuild\15.0\Bin\MSBuild.exe"
)

if not exist "%MSBUILD%" (
    echo MSBuild not found.
    pause  
    exit /b 1
)

echo Building wxWidgets Release x64...
"%MSBUILD%" wx_vc17.sln /p:Configuration=Release /p:Platform=x64

if %ERRORLEVEL% neq 0 (
    echo Trying wx_vc16.sln...
    "%MSBUILD%" wx_vc16.sln /p:Configuration=Release /p:Platform=x64
    
    if %ERRORLEVEL% neq 0 (
        echo wxWidgets build failed.
        pause
        exit /b 1
    )
)

echo Building wxWidgets Debug x64...
"%MSBUILD%" wx_vc17.sln /p:Configuration=Debug /p:Platform=x64

if %ERRORLEVEL% neq 0 (
    "%MSBUILD%" wx_vc16.sln /p:Configuration=Debug /p:Platform=x64
)

echo wxWidgets build completed!
echo You can now run build.bat to build the application.
pause