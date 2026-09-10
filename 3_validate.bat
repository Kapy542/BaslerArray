@echo off
setlocal

REM Always work relative to this BAT file 
cd /d "%~dp0"

REM Recording directory
set "RECORDING_DIR=I:\JammerTestTestData\recordings"

echo.
echo ========================================
echo        Recording Validation
echo ========================================
echo.

REM Ask user for take name
set /p "TAKE_NAME=Enter take name: "

if "%TAKE_NAME%"=="" (
    echo.
    echo ERROR: No take name entered.
    pause
    exit /b 1
)

set "TAKE_DIR=%RECORDING_DIR%\%TAKE_NAME%"

REM Check that recording exists
if not exist "%TAKE_DIR%" (
    echo.
    echo ERROR: Recording not found:
    echo   %TAKE_DIR%
    pause
    exit /b 1
)

echo.
echo Recording:
echo   %TAKE_DIR%
echo.

echo ========================================
echo        Validating recording
echo ========================================
echo.

set "PYTHON=%~dp0.venv\Scripts\python.exe"

"%PYTHON%" ./Validation/validate_recording.py "%RECORDING_DIR%" "%TAKE_NAME%"

if errorlevel 1 (
    echo.
    echo ERROR: validate_recording.py failed.
    pause
    exit /b 1
)

echo.
echo ========================================
echo        Visualizing frames
echo ========================================
echo.

"%PYTHON%" ./Validation/vis_frames.py "%RECORDING_DIR%" "%TAKE_NAME%"

if errorlevel 1 (
    echo.
    echo ERROR: vis_frames.py failed.
    pause
    exit /b 1
)

echo.
echo ========================================
echo        Validation finished
echo ========================================
echo.

pause
endlocal
```
