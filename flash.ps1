param(
    [string]$Port = "COM4",
    [switch]$BuildOnly,
    [switch]$FlashOnly,
    [switch]$Clean
)

if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    $profileCandidates = @()

    if ($env:IDF_PATH) {
        $profileCandidates += (Join-Path $env:IDF_PATH "export.ps1")
    }

    $profileCandidates += "C:\Espressif\tools\Microsoft.v5.5.3.PowerShell_profile.ps1"

    $loadedProfile = $false
    foreach ($profilePath in $profileCandidates) {
        if (Test-Path $profilePath) {
            . $profilePath
            $loadedProfile = $true
            break
        }
    }

    if ((-not $loadedProfile) -or (-not (Get-Command idf.py -ErrorAction SilentlyContinue))) {
        Write-Host "`nESP-IDF environment not found. Open an ESP-IDF shell or set IDF_PATH first." -ForegroundColor Red
        exit 1
    }
}

if ($Clean) {
    Write-Host "`n--- Clean build ---" -ForegroundColor Yellow
    idf.py fullclean
}

if ($FlashOnly) {
    Write-Host "`n--- Flashing to $Port ---" -ForegroundColor Cyan
    idf.py flash -p $Port
} elseif ($BuildOnly) {
    Write-Host "`n--- Building ---" -ForegroundColor Cyan
    idf.py build
} else {
    Write-Host "`n--- Build + Flash to $Port ---" -ForegroundColor Cyan
    idf.py build flash -p $Port
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nDone." -ForegroundColor Green
} else {
    Write-Host "`nFailed (exit code $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
}
