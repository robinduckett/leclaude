# Removes the Leclaude handlers.
# The script removes the registration, restarts Explorer, and deletes the files.
# Open an administrator PowerShell to start this script.
#
# Usage: .\uninstall.ps1 [-NoRestart]

#Requires -RunAsAdministrator
param(
    [switch]$NoRestart,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

# The Explorer copy engine runs inside explorer.exe. A stop of Explorer
# also stops an active copy or move. The test finds the progress window.
function Test-ExplorerFileOperation {
    if (-not ('Leclaude.Window' -as [type])) {
        Add-Type -Namespace Leclaude -Name Window -MemberDefinition @'
[DllImport("user32.dll", CharSet = CharSet.Unicode)]
public static extern IntPtr FindWindowW(string className, string windowName);
'@
    }
    [Leclaude.Window]::FindWindowW('OperationStatusWindow', $null) -ne [IntPtr]::Zero
}

function Wait-ForFileOperations {
    while (Test-ExplorerFileOperation) {
        Write-Warning 'A file operation is in progress in Explorer. A restart of Explorer stops the operation.'
        $answer = Read-Host 'Wait for the end of the operation and press Enter. Or enter C to continue now'
        if ($answer -eq 'C') {
            return
        }
    }
}

# Records the folder path of each open Explorer window.
function Get-OpenFolderPaths {
    $paths = @()
    try {
        $shell = New-Object -ComObject Shell.Application
        foreach ($window in @($shell.Windows())) {
            try {
                if ($window.FullName -like '*\explorer.exe') {
                    $path = $window.Document.Folder.Self.Path
                    if ($path) { $paths += $path }
                }
            }
            catch {}
        }
    }
    catch {}
    , $paths
}

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
    $menuClsid = '{F00FE5BC-E333-4E6A-A271-817BA795CFEA}'
    Remove-Item -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers\ Leclaude" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-Item -Path "HKLM:\Software\Classes\Directory\shellex\ContextMenuHandlers\Leclaude" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-Item -Path "HKLM:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\Leclaude" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-Item -Path "HKLM:\Software\Classes\CLSID\$clsid" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-Item -Path "HKLM:\Software\Classes\CLSID\$menuClsid" -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" -Name $clsid -Force -Confirm:$false -ErrorAction SilentlyContinue
    Remove-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" -Name $menuClsid -Force -Confirm:$false -ErrorAction SilentlyContinue
}

if (-not $NoRestart) {
    if (-not $Force) {
        Wait-ForFileOperations
        Write-Host 'The script restarts Explorer now. You can close the open Explorer windows first.'
        Write-Host 'The script opens the open folder windows again after the restart.'
        Read-Host 'Press Enter to continue' | Out-Null
    }
    $openFolders = Get-OpenFolderPaths
    # Explorer keeps a lock on the DLL. A restart releases it.
    Stop-Process -Name explorer -Force -Confirm:$false
    Remove-Item "$env:LOCALAPPDATA\Microsoft\Windows\Explorer\iconcache_*.db" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
        Start-Process explorer.exe
    }
    foreach ($folder in $openFolders) {
        Start-Process explorer.exe -ArgumentList "`"$folder`""
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
