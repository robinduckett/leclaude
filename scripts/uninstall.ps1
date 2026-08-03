# Removes the Leclaude overlay handler.
# The script removes the registration, restarts Explorer, and deletes the files.
# Open an administrator PowerShell to start this script.
#
# Usage: .\uninstall.ps1 [-NoRestart]

#Requires -RunAsAdministrator
param(
    [switch]$NoRestart
)

$ErrorActionPreference = 'Stop'

$installDir = Join-Path $env:ProgramFiles 'Leclaude'
$target = Join-Path $installDir 'LeclaudeShell.dll'

if (Test-Path $target) {
    $reg = Start-Process regsvr32.exe -ArgumentList '/u', '/s', "`"$target`"" -Wait -PassThru
    if ($reg.ExitCode -ne 0) {
        Write-Warning "The deregistration returned the code $($reg.ExitCode). The script continues."
    }
    else {
        Write-Host 'The deregistration is complete.'
    }
}
else {
    Write-Host 'The DLL is not installed. The script only removes the registration.'
    # A partial installation can leave the registry entries. Regsvr32 needs the DLL,
    # so the script removes the known keys directly.
    $clsid = '{AEB0D999-FC8F-4FFA-B160-D4506164F0E7}'
    Remove-Item -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\ Leclaude" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-Item -Path "HKLM:\Software\Classes\CLSID\$clsid" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" -Name $clsid -Force -Confirm:$false -ErrorAction SilentlyContinue
}

if (-not $NoRestart) {
    # Explorer keeps a lock on the DLL. A restart releases it.
    Write-Host 'The script restarts Explorer now.'
    Stop-Process -Name explorer -Force -Confirm:$false
    Start-Sleep -Seconds 2
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
        Start-Process explorer.exe
    }
}

if (Test-Path $installDir) {
    try {
        Remove-Item -Path $installDir -Recurse -Force -Confirm:$false
        Write-Host 'The removal is complete.'
    }
    catch {
        # Another program can hold the lock, for example through an open file dialog.
        # Windows then deletes the file at the next start of the computer.
        Add-Type -Namespace Leclaude -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern bool MoveFileEx(string existing, string target, int flags);
'@
        [void][Leclaude.Native]::MoveFileEx($target, $null, 4)
        [void][Leclaude.Native]::MoveFileEx($installDir, $null, 4)
        Write-Host 'The DLL is locked. Windows deletes it at the next start of the computer.'
    }
}
