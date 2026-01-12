# VANGUARD DEVELOPMENT LOOP AUTOMATION
# Description: Formats, Tests, Builds, Commits, and Pushes.

$ErrorActionPreference = "Stop"
$PIO = "C:\Users\User\.platformio\penv\Scripts\pio.exe"

function Write-Step { param($msg) Write-Host "`n[VANGUARD-LOOP] $msg" -ForegroundColor Cyan }
function Write-Success { param($msg) Write-Host "SUCCESS: $msg" -ForegroundColor Green }
function Write-ErrorMsg { param($msg) Write-Host "ERROR: $msg" -ForegroundColor Red }

# 1. Format Code
Write-Step "Formatting Source Code..."
if (Get-Command clang-format -ErrorAction SilentlyContinue) {
    Get-ChildItem -Path "src" -Recurse -Include *.cpp,*.h | ForEach-Object {
        clang-format -i $_.FullName
    }
    Write-Success "All files formatted."
} else {
    Write-Warning "clang-format not found. Skipping formatting."
}

# 2. Run Unit Tests (Native) - Skip if compiler missing
Write-Step "Checking for Native Test Capability..."
if (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Step "Running Native Unit Tests..."
    & $PIO test -e native
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorMsg "Tests Failed! Aborting push."
        exit 1
    }
    Write-Success "All tests passed."
} else {
    Write-Warning "Native compiler (g++) not found. Skipping local unit tests. (CI will handle this)"
}


# 3. Build Verification
Write-Step "Verifying Firmware Build..."
& $PIO run -e m5stack-cardputer
if ($LASTEXITCODE -ne 0) {
    Write-ErrorMsg "Build Failed! Aborting push."
    exit 1
}
Write-Success "Build successful."

# 4. Commit and Push
Write-Step "Syncing with Command & Control (Git)..."
$status = git status --porcelain
if ($status) {
    git add .
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    git commit -m "[AUTO] Cycle Progress: $timestamp"
    git push
    Write-Success "Changes pushed to main branch."
} else {
    Write-Success "No changes detected. Skipping git sync."
}

Write-Step "LOOP COMPLETE. Standing by for next deployment."
