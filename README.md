<p align="center">
  <img src="assets/leclaude.svg" alt="The Leclaude robot" width="128" height="128">
</p>

# Leclaude

Leclaude shows a robot badge in Windows Explorer on each folder that has [Claude Code](https://www.anthropic.com/claude-code) session history.
When you look through your disk, you can immediately see the folders where you did work with Claude Code.
A right-click on a project folder shows two menu commands: "Open Claude Code in Terminal" and "Open Claude Code in PowerShell".

Leclaude is a shell extension for Windows 10 and Windows 11, on x64 and ARM64.

## <img src="assets/preview-16px.png" width="20" alt=""> How it operates

Claude Code records each opened folder in `%USERPROFILE%\.claude\projects\`.
Leclaude registers an icon-overlay handler with Explorer.
For each visible folder, the handler does one fast search in a set in memory.
When the folder has session history, Explorer draws the robot badge on the folder icon.
A watcher thread follows the Claude Code data and refreshes the badges.

Leclaude also registers a menu handler.
When you right-click a project folder, or the background of its open window, the context menu shows two menu commands.
"Open Claude Code in Terminal" starts Claude Code in Windows Terminal.
"Open Claude Code in PowerShell" starts Claude Code in PowerShell.
The menu texts are available in six languages: English, German, French, Spanish, Japanese, and Simplified Chinese.

![Explorer with the Leclaude badges. Two of the four folders have Claude Code session history.](assets/leclaude-screenshot.png)

The design documents give the full details:

- [The architecture](docs/architecture.md)
- [The Claude Code data format](docs/claude-data-format.md)

## <img src="assets/preview-16px.png" width="20" alt=""> Installation

The [releases page](../../releases) gives a setup program and a zip file for each processor: `x64` or `arm64`.

The setup program is the recommended installation:

1. Download `LeclaudeSetup-<version>-<processor>.exe`.
2. Start it and give permission. The installer registers the handlers. It asks before it restarts Explorer, waits for your file operations, and opens the open folder windows again.

To remove Leclaude, use "Installed apps" in the Windows settings.

The zip file is the manual alternative:

1. Download and unpack `Leclaude-<tag>-<processor>.zip`.
2. Do a double click on `install.cmd` and give permission.

To remove this installation, do a double click on `uninstall.cmd`.

Windows SmartScreen can show a warning, because the files have no signature.
If Windows shows the warning, select "More info", then "Run anyway".
The attestation check below is the correct test that a file is authentic.

### Make sure that a release is authentic

The release workflow attests each zip file with [GitHub artifact attestation](https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations).
Sigstore signs the attestation. To do a check on a setup program or a zip file, enter:

```
gh attestation verify <file> --owner robinduckett
```

The command shows the workflow and the commit that made the file.

## <img src="assets/preview-16px.png" width="20" alt=""> Compilation from the source

The build uses CMake with the Ninja generator. Visual Studio contains both tools.

1. Open a Developer PowerShell for Visual Studio.
2. Enter: `cmake --preset x64-release`
3. Enter: `cmake --build --preset x64-release`
4. To do the tests, enter: `ctest --test-dir build/x64-release --output-on-failure`

## <img src="assets/preview-16px.png" width="20" alt=""> Limits

- The installation needs administrator rights. Windows accepts overlay registrations in HKLM only.
- Windows shows a maximum of 15 overlay types for the full system. Cloud-storage tools use many of them. When too many tools compete, Windows can ignore the Leclaude badge. The [ShellExView](https://www.nirsoft.net/utils/shexview.html) tool shows the competition on your system.
- In a cloud sync folder (for example, a OneDrive folder), Windows 11 can hide third-party badges.
- On Windows 11, the two menu commands are in "Show more options". The compact menu accepts only entries from an app package, and Leclaude has no app package.
- Explorer reads the PATH variable at the logon. If you installed the `claude` command after the logon, the menu commands can open a window that does not find `claude`. An Explorer restart corrects this.
- For a project folder on a network path, cmd cannot use the path as its start folder. It shows a warning and starts in the Windows folder.

## <img src="assets/preview-16px.png" width="20" alt=""> Problems and solutions

The badge shows an old icon after an update, but only at some sizes:
Explorer keeps the icons in cache files on the disk, with one file for each size.
An Explorer restart does not delete these files. To delete them, enter these commands in PowerShell:

```powershell
Stop-Process -Name explorer -Force
Remove-Item "$env:LOCALAPPDATA\Microsoft\Windows\Explorer\iconcache_*.db" -Force -ErrorAction SilentlyContinue
if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }
```

A restart of the computer also corrects this.

The badge does not show at all:
Windows shows a maximum of 15 overlay types. Use the ShellExView tool to see the loaded handlers.
Make sure that the name " Leclaude" is in the first 15 names under the registry key `ShellIconOverlayIdentifiers`.

The menu commands do not show:
On Windows 11, the commands are in "Show more options" at the end of the context menu.
The commands show only on a folder that has session history. The badge on the folder shows this.
The commands also do not show when you select more than one folder.

## <img src="assets/preview-16px.png" width="20" alt=""> License

MIT. See [LICENSE](LICENSE).
