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

# Remove the files that an earlier upgrade left. A locked file stays, and that is not a problem.
Get-ChildItem -Path $installDir -Filter 'LeclaudeShell.dll.old-*' -ErrorAction SilentlyContinue |
    ForEach-Object {
        try { Remove-Item $_.FullName -Force -Confirm:$false } catch {}
    }

try {
    Copy-Item $DllPath $target -Force
}
catch {
    # Explorer and other programs keep a lock on the installed DLL.
    # Windows does not let a copy replace a locked file. But Windows lets a
    # rename move it. The script moves the locked DLL to a temporary name,
    # copies the new DLL to the correct name, and tells Windows to delete
    # the old file at the next start of the computer.
    Write-Host 'The installed DLL is locked. The script moves it to a temporary name.'
    $oldName = "$target.old-" + [guid]::NewGuid().ToString('N').Substring(0, 8)
    Move-Item -Path $target -Destination $oldName -Force
    Copy-Item $DllPath $target -Force

    Add-Type -Namespace Leclaude -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern bool MoveFileEx(string existing, string target, int flags);
'@
    [void][Leclaude.Native]::MoveFileEx($oldName, $null, 4)
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
    # Explorer reads the overlay list and the badge image only when it starts.
    # The deletion of the icon-cache files prevents an old badge image at some sizes.
    Write-Host 'The script restarts Explorer now.'
    Stop-Process -Name explorer -Force -Confirm:$false
    Remove-Item "$env:LOCALAPPDATA\Microsoft\Windows\Explorer\iconcache_*.db" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
        Start-Process explorer.exe
    }
    Write-Host 'The installation is complete. The badge shows on each folder that has Claude Code session history.'
}
