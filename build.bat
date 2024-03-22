@echo off

:: Check if any argument is --no-config
set "no_config=0"

for %%i in (%*) do (
    if /I "%%i"=="--no-config" (
        set "no_config=1"
    )
)

:: Check if any argument is debug or release
set "build_type=debug"

for %%i in (%*) do (
    if /I "%%i"=="debug" (
        set "build_type=debug"
    ) else if /I "%%i"=="release" (
        set "build_type=release"
    )
)

if %no_config%==0 (
    .\dependencies\conjure\conjure.exe ninja
)

call :findInstallDir

if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
    set "build_type=%~1"
    call :buildTempest %build_type%
)
exit /b

:findInstallDir
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set InstallDir=%%i
)
if not defined InstallDir (
    echo "Faied to find vswhere"
    exit /b
)
exit /b

:buildTempest
if "%1"=="debug" (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Debug_x64
) else if "%1"=="release" (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Release_x64
) else (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Debug_x64
    ninja -f Tempest.ninja Release_x64
)
exit /b