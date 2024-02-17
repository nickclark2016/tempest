@REM @echo off

@REM .\dependencies\conjure\conjure.exe ninja

@REM for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
@REM     set InstallDir=%%i
@REM )

@REM if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
@REM     set "build_type=%~1"
@REM     if "%build_type%"=="debug" (
@REM         "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
@REM         ninja -f Tempest.ninja Debug_x64
@REM     ) else if "%build_type%"=="release" (
@REM         "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
@REM         ninja -f Tempest.ninja Release_x64
@REM     ) else (
@REM         "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
@REM         ninja -f Tempest.ninja Debug_x64
@REM         ninja -f Tempest.ninja Release_x64
@REM     )
@REM ) else (
@REM     echo "Faied to find vswhere"
@REM )

@echo off

.\dependencies\conjure\conjure.exe ninja

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
if "%~1"=="debug" (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Debug_x64
) else if "%~1"=="release" (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Release_x64
) else (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Debug_x64
    ninja -f Tempest.ninja Release_x64
)
exit /b