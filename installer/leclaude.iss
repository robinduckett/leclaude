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
; The robot image shows on the wizard pages. Each display scale has one
; file with an integer pixel scale. The wildcard lets the setup program
; select the file that matches the display scale.
WizardImageFile=..\assets\wizard-image-*.bmp
WizardSmallImageFile=..\assets\wizard-small-*.bmp
WizardImageStretch=no
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

[Code]
const
  MOVEFILE_DELAY_UNTIL_REBOOT = 4;
  { The class name of the Explorer file-operation window. }
  kFileOperationWindowClass = 'OperationStatusWindow';

var
  SavedFolders: array of String;
  UninstallExplorerStopped: Boolean;

function MoveFileExDelayDelete(lpExisting: String; lpNew: Cardinal; dwFlags: Cardinal): Boolean;
  external 'MoveFileExW@kernel32.dll stdcall';

{ The Explorer copy engine runs inside explorer.exe. A stop of Explorer   }
{ also stops an active copy or move. The test finds the progress window.  }
function ExplorerHasFileOperation: Boolean;
begin
  Result := FindWindowByClassName(kFileOperationWindowClass) <> 0;
end;

{ Returns True when the restart can continue. In a suppressed-message    }
{ installation, the answer is Ignore, and the behavior is the same as in }
{ the earlier versions of this installer.                                }
function ConfirmFileOperations: Boolean;
var
  Answer: Integer;
begin
  Result := True;
  while ExplorerHasFileOperation do
  begin
    Answer := SuppressibleMsgBox(
      'A file operation is in progress in Explorer. A restart of Explorer stops the operation.'#13#10#13#10 +
      'Wait for the end of the operation. Then select Retry.'#13#10 +
      'To restart Explorer now, select Ignore.'#13#10 +
      'To keep Explorer open, select Abort.',
      mbConfirmation, MB_ABORTRETRYIGNORE, IDIGNORE);
    if Answer = IDIGNORE then
      Exit;
    if Answer = IDABORT then
    begin
      Result := False;
      Exit;
    end;
    { The answer is Retry. The loop does the test again. }
  end;
end;

function ConfirmRestart(const Question: String): Boolean;
begin
  Result := SuppressibleMsgBox(Question, mbConfirmation, MB_YESNO, IDYES) = IDYES;
end;

{ Records the folder path of each open Explorer window. }
procedure SaveExplorerWindows;
var
  Shell, Windows, Window: Variant;
  I, Count: Integer;
  Path, FullName: String;
begin
  SetArrayLength(SavedFolders, 0);
  try
    Shell := CreateOleObject('Shell.Application');
    Windows := Shell.Windows;
    for I := 0 to Windows.Count - 1 do
    begin
      try
        Window := Windows.Item(I);
        FullName := Window.FullName;
        if Pos('explorer.exe', Lowercase(FullName)) > 0 then
        begin
          Path := Window.Document.Folder.Self.Path;
          if Path <> '' then
          begin
            Count := GetArrayLength(SavedFolders);
            SetArrayLength(SavedFolders, Count + 1);
            SavedFolders[Count] := Path;
          end;
        end;
      except
      end;
    end;
  except
  end;
end;

{ Stops Explorer and deletes the icon-cache files. The deletion prevents }
{ an old badge image at some sizes.                                      }
procedure StopExplorer;
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{cmd}'),
    '/c taskkill /f /im explorer.exe & del /q "' +
    ExpandConstant('{localappdata}') + '\Microsoft\Windows\Explorer\iconcache_*.db"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

{ Starts Explorer and opens the recorded folder windows again. }
procedure StartExplorerAndRestoreWindows;
var
  ResultCode, I: Integer;
begin
  Exec(ExpandConstant('{win}\explorer.exe'), '', '', SW_SHOW, ewNoWait, ResultCode);
  if GetArrayLength(SavedFolders) > 0 then
  begin
    { A short pause lets the shell start before the folder windows open. }
    Sleep(1500);
    for I := 0 to GetArrayLength(SavedFolders) - 1 do
      Exec(ExpandConstant('{win}\explorer.exe'), '"' + SavedFolders[I] + '"', '',
        SW_SHOW, ewNoWait, ResultCode);
  end;
end;

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

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then
    Exit;

  { Explorer reads the overlay list and the badge image only when it     }
  { starts. Thus the badge needs an Explorer restart. The menu commands  }
  { operate without the restart.                                        }
  if not ConfirmRestart(
    'The setup restarts Explorer now. The restart shows the badges.'#13#10#13#10 +
    'You can close the open Explorer windows first. The setup opens the open folder windows again after the restart.'#13#10#13#10 +
    'Restart Explorer now?') then
  begin
    SuppressibleMsgBox(
      'The menu commands operate now. The badges show after the next restart of Explorer or after the next logon.',
      mbInformation, MB_OK, IDOK);
    Exit;
  end;
  if not ConfirmFileOperations then
  begin
    SuppressibleMsgBox(
      'The menu commands operate now. The badges show after the next restart of Explorer or after the next logon.',
      mbInformation, MB_OK, IDOK);
    Exit;
  end;
  SaveExplorerWindows;
  StopExplorer;
  StartExplorerAndRestoreWindows;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Target, OldName: String;
  I: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    { An Explorer stop releases the lock on the DLL before the file      }
    { deletion. Without the stop, the removal schedules the deletion for }
    { the next start of the computer.                                    }
    UninstallExplorerStopped := False;
    if not ConfirmRestart(
      'The removal restarts Explorer now. The restart removes the badges.'#13#10#13#10 +
      'You can close the open Explorer windows first. The removal opens the open folder windows again after the restart.'#13#10#13#10 +
      'Restart Explorer now?') then
      Exit;
    if not ConfirmFileOperations then
      Exit;
    SaveExplorerWindows;
    StopExplorer;
    UninstallExplorerStopped := True;
    Exit;
  end;

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

  if UninstallExplorerStopped then
    StartExplorerAndRestoreWindows;
end;
