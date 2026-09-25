param([string]$Device = 'All')
$ErrorActionPreference = 'Stop'
$env:PYTHONIOENCODING = 'utf-8'
$pio = (Get-Command pio -ErrorAction SilentlyContinue).Source
if (-not $pio) { $pio = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/platformio.exe' }
if (-not (Test-Path -LiteralPath $pio)) { throw 'Install the PlatformIO IDE extension in VS Code first.' }
$projects = @(Get-ChildItem -LiteralPath $PSScriptRoot -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'platformio.ini') })
if ($Device -ne 'All') { $projects = @($projects | Where-Object Name -eq $Device) }
if ($projects.Count -eq 0) { throw "Unknown device project: $Device" }
$logs = Join-Path $PSScriptRoot 'build-logs'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$failed = @()
foreach ($project in $projects) {
    Write-Host "Building $($project.Name)..."
    $log = Join-Path $logs ($project.Name + '.log')
    & $pio run --project-dir $project.FullName *> $log
    if ($LASTEXITCODE -ne 0) { $failed += $project.Name; Get-Content -LiteralPath $log -Tail 25 }
    else { Write-Host "OK: $($project.FullName)/.pio/build/<environment>/firmware.bin (environment name from platformio.ini)" }
}
if ($failed.Count) { throw "Build failed: $($failed -join ', '). See build-logs." }
Write-Host 'All requested builds succeeded. No devices were flashed.'
