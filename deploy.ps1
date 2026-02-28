<#
.SYNOPSIS
    VANGUARD DEPLOYMENT AUTOMATION
    Builds, Flashes, and Monitors the Firmware in one step.

.DESCRIPTION
    1. Checks for connected ESP32-S3.
    2. Runs clang-format to tidy code.
    3. Compiles firmware (PlatformIO).
    4. Uploads to device.
    5. Starts Serial Monitor.

.EXAMPLE
    .\deploy.ps1
#>

$ErrorActionPreference = "Stop"

# Use absolute path found in system check
$PIO = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"

function Write-Step { param($msg) Write-Host "`n[VANGUARD] $msg" -ForegroundColor Cyan }
function Write-Success { param($msg) Write-Host "SUCCESS: $msg" -ForegroundColor Green }
function Write-ErrorMsg { param($msg) Write-Host "ERROR: $msg" -ForegroundColor Red }

# 1. Check Device
Write-Step "Checking for Device..."
try {
    $ports = & $PIO device list --json | ConvertFrom-Json
    $esp = $ports | Where-Object { $_.description -match "CP210" -or $_.hwid -match "USB VID:PID" }

    if ($esp) {
        Write-Success "Found Device on $($esp[0].port)"
    } else {
        Write-Warning "No specific ESP32 port found. PlatformIO will attempt auto-detect."
    }
} catch {
    Write-Warning "Device check failed. Proceeding blindly."
}

# 2. Format Code
Write-Step "Formatting Code..."
try {
    # Check if clang-format exists (optional)
    if (Get-Command clang-format -ErrorAction SilentlyContinue) {
        Get-ChildItem -Path "src" -Recurse -Include *.cpp,*.h | ForEach-Object {
            clang-format -i $_.FullName
        }
        Write-Success "Code formatted."
    } else {
        Write-Warning "clang-format not found. Skipping."
    }
} catch {
    Write-Warning "Formatting failed. Continuing..."
}

# 3. Build & Flash
Write-Step "Compiling and Flashing..."
& $PIO run -t upload

if ($LASTEXITCODE -eq 0) {
    Write-Success "Firmware Flashed Successfully!"
} else {
    Write-ErrorMsg "Build/Flash Failed!"
    exit 1
}

# 4. Monitor
Write-Step "Starting Serial Monitor..."
# Wait a brief moment for reset
Start-Sleep -Seconds 2
& $PIO device monitor
