; The Inno Setup script for the Leclaude installer.
; The build gives these defines: AppVersion, Arch (x64 or arm64), DllPath.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef Arch
  #define Arch "x64"
#endif
#ifndef DllPath
  #define DllPath "..\build\" + Arch + "-release\LeclaudeShell.dll"
#endif

[Setup]
AppId={{A05D7966-3ABD-4530-9422-DBCBC758771F}
AppName=Leclaude
AppVersion={#AppVersion}
AppPublisher=Robin Duckett
AppPublisherURL=https://robinduckett.com
AppSupportURL=https://github.com/robinduckett/leclaude
DefaultDirName={autopf}\Leclaude
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
OutputDir=output
OutputBaseFilename=LeclaudeSetup-{#AppVersion}-{#Arch}
SetupIconFile=..\assets\leclaude.ico
UninstallDisplayIcon={app}\LeclaudeShell.dll
VersionInfoVersion={#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
#if Arch == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Files]
; The regserver flag runs DllRegisterServer at the installation and
; DllUnregisterServer at the removal.
Source: "{#DllPath}"; DestDir: "{app}"; Flags: ignoreversion regserver 64bit
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Run]
; Explorer reads the overlay list and the badge image only when it starts.
; The deletion of the icon-cache files prevents an old badge image at some sizes.
Filename: "{cmd}"; Parameters: "/c taskkill /f /im explorer.exe & del /q ""{localappdata}\Microsoft\Windows\Explorer\iconcache_*.db"" & start explorer.exe"; \
    Flags: runhidden; StatusMsg: "The installer restarts Explorer now."

[UninstallRun]
; An Explorer stop releases the lock on the DLL before the file deletion.
; The Code section starts Explorer again after the removal.
Filename: "{cmd}"; Parameters: "/c taskkill /f /im explorer.exe & del /q ""{localappdata}\Microsoft\Windows\Explorer\iconcache_*.db"""; \
    Flags: runhidden; RunOnceId: "StopExplorer"

[Code]
const
  MOVEFILE_DELAY_UNTIL_REBOOT = 4;

function MoveFileExDelayDelete(lpExisting: String; lpNew: Cardinal; dwFlags: Cardinal): Boolean;
  external 'MoveFileExW@kernel32.dll stdcall';

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Target, OldName: String;
  I: Integer;
begin
  Result := '';
  Target := ExpandConstant('{app}\LeclaudeShell.dll');
  if not FileExists(Target) then
    Exit;
  { Explorer and other programs keep a lock on the installed DLL.       }
  { Windows refuses a copy over a locked file, but it accepts a rename. }
  { The installer moves the old DLL to a temporary name, and Windows    }
  { deletes that file at the next start of the computer.                }
  OldName := Target + '.old';
  I := 0;
  while FileExists(OldName) and (I < 100) do
  begin
    I := I + 1;
    OldName := Target + '.old-' + IntToStr(I);
  end;
  if RenameFile(Target, OldName) then
    MoveFileExDelayDelete(OldName, 0, MOVEFILE_DELAY_UNTIL_REBOOT);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
  Target, OldName: String;
  I: Integer;
begin
  if CurUninstallStep <> usPostUninstall then
    Exit;

  { A program with an open file dialog can keep a lock on the DLL.       }
  { Then the deletion fails, and the file stays. The removal renames the }
  { file and tells Windows to delete it at the next start. Thus the      }
  { removal never asks for a restart of the computer.                    }
  Target := ExpandConstant('{app}\LeclaudeShell.dll');
  if FileExists(Target) then
  begin
    OldName := Target + '.old';
    I := 0;
    while FileExists(OldName) and (I < 100) do
    begin
      I := I + 1;
      OldName := Target + '.old-' + IntToStr(I);
    end;
    if RenameFile(Target, OldName) then
      MoveFileExDelayDelete(OldName, 0, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileExDelayDelete(ExpandConstant('{app}'), 0, MOVEFILE_DELAY_UNTIL_REBOOT);
  end;

  { The removal stops Explorer. This starts it again. }
  Exec(ExpandConstant('{win}\explorer.exe'), '', '', SW_SHOW, ewNoWait, ResultCode);
end;
