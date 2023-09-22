@echo off

.\dependencies\conjure\conjure.exe ninja

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set InstallDir=%%i
)

if exist "%InstallDir%\Common7\Tools\vsdevcmd.bat" (
    "%InstallDir%\Common7\Tools\vsdevcmd.bat" -arch=x64 -host_arch=x64
    ninja -f Tempest.ninja Debug_x64
    ninja -f Tempest.ninja Release_x64
) else (
    echo "Faied to find vswhere"
)
