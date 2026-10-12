
@echo off
setlocal
echo TEST 1: Batch execution works.
pause

echo TEST 2: Checking Visual Studio path.
set "VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
echo VS_ROOT is "%VS_ROOT%"
pause

echo TEST 3: Checking compiler environment script.
if exist "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" (
    echo Compiler environment script exists.
) else (
    echo Compiler environment script is missing.
)
pause

echo TEST 4: Finished.
