# Installs the Leclaude overlay handler.
# The script copies the DLL, registers it, and restarts Explorer.
# Open an administrator PowerShell to start this script.
#
# Usage: .\install.ps1 [-DllPath <path>] [-NoRestart]

#Requires -RunAsAdministrator
param(
    [string]$DllPath,
    [switch]$NoRestart
)

$ErrorActionPreference = 'Stop'

if (-not $DllPath) {
    $candidates = @(
        (Join-Path $PSScriptRoot 'LeclaudeShell.dll'),
        (Join-Path $PSScriptRoot '..\build\x64-release\LeclaudeShell.dll'),
        (Join-Path $PSScriptRoot '..\build\arm64-release\LeclaudeShell.dll')
    )
    $DllPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $DllPath) {
        throw 'The script cannot find LeclaudeShell.dll. Give the path with -DllPath.'
    }
}
$DllPath = (Resolve-Path $DllPath).Path

$installDir = Join-Path $env:ProgramFiles 'Leclaude'
$target = Join-Path $installDir 'LeclaudeShell.dll'
New-Item -ItemType Directory -Force $installDir | Out-Null

try {
    Copy-Item $DllPath $target -Force
}
catch {
    # Explorer keeps a lock on an installed DLL. Stop Explorer, then copy again.
    Write-Host 'The installed DLL is locked. The script stops Explorer to release it.'
    Stop-Process -Name explorer -Force -Confirm:$false
    Start-Sleep -Seconds 2
    Copy-Item $DllPath $target -Force
}

$reg = Start-Process regsvr32.exe -ArgumentList '/s', "`"$target`"" -Wait -PassThru
if ($reg.ExitCode -ne 0) {
    throw "The registration failed with the code $($reg.ExitCode)."
}
Write-Host 'The registration is complete.'

if ($NoRestart) {
    Write-Host 'The installation is complete. Restart Explorer to see the badges.'
}
else {
    # Explorer reads the overlay list only when it starts.
    Write-Host 'The script restarts Explorer now.'
    Stop-Process -Name explorer -Force -Confirm:$false
    Start-Sleep -Seconds 2
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
        Start-Process explorer.exe
    }
    Write-Host 'The installation is complete. The badge shows on each folder that has Claude Code session history.'
}
