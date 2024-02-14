@echo off

setlocal

set "binDir=bin"
set "testPattern=*-tests.exe"

for /r "%binDir%" %%f in (%testPattern%) do (
    echo Running test: %%f
    %%f
)

endlocal
