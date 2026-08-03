# Leclaude architecture

## Summary

Leclaude is a shell extension for Windows 10 and Windows 11.
It shows a badge on each folder that has Claude Code session history.
The badge is a small robot icon in a corner of the folder icon.
Explorer shows the badge in the folder views, on the desktop, and in the file dialogs.

## The mechanism: the icon-overlay handler

Windows has one supported mechanism for a badge on a folder: the icon-overlay handler.
An icon-overlay handler is a COM object in a DLL. Explorer loads the DLL into its process.
The COM object supplies the interface `IShellIconOverlayIdentifier`.
The interface has three functions:

- `GetOverlayInfo` — Explorer calls this function one time when it starts. The function returns the path of the icon file and the icon index. Explorer copies the icon into the system image list. A change of the icon is not possible without an Explorer restart.
- `GetPriority` — This function returns a priority from 0 to 100. The value only sets the order between our own handlers. Leclaude returns 0.
- `IsMemberOf` — Explorer calls this function for each item that it shows. The function gets the full path of the item and its attribute flags. Return `S_OK` to show the badge. Return `S_FALSE` to show no badge.

## Windows facts that control the design

These facts come from the Microsoft documentation and from the TortoiseGit project. The sources are at the end of this document.

1. The registration must be in `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers`. A registration in HKCU does not operate. Thus the installation needs administrator rights.
2. Windows loads a maximum of 15 overlay handlers. Windows itself uses approximately 4 of the 15 slots. Windows selects the first 15 subkey names in alphabetical order. Cloud-storage tools put a space before their names to win this selection. Leclaude registers one handler with the name `" Leclaude"` (one space before the name).
3. Explorer reads the handler list one time for each session. After the installation, an Explorer restart is necessary. The function `SHLoadNonloadedIconOverlayIdentifiers` is a polite alternative, but it only operates when fewer than 15 handlers are loaded.
4. The handler DLL also loads into each program that shows a file dialog. Thus the DLL must be small, must load quickly, and must have no dependencies. Link the C runtime statically.
5. The DLL bitness must be equal to the Explorer bitness. Leclaude compiles for x64 and ARM64.
6. On Windows 11, the Cloud Files API can hide third-party badges in a sync folder (a OneDrive folder or a Dropbox folder). This is a known limit.

## Speed rules for IsMemberOf

Explorer calls `IsMemberOf` on its UI thread, for each visible item.
A slow `IsMemberOf` makes Explorer slow or frozen.
Thus these rules apply:

- Return in microseconds. Do no network access. Do no disk access on this path.
- Reject non-folders first. The attribute flags contain `FILE_ATTRIBUTE_DIRECTORY`. This check is free.
- Do the folder test as a search in a hash set in memory.

## The detection design

The question is: "Does this folder have Claude Code session history?"
The document [claude-data-format.md](claude-data-format.md) gives the data facts.
The design is:

1. Lazy start. The handler does no work in `DllMain`. On the first `IsMemberOf` call, the handler reads the folder `%USERPROFILE%\.claude\projects\` one time.
2. The handler examines each subfolder. A subfolder counts when it contains one or more `.jsonl` files, or a `sessions-index.json` file with one or more entries.
3. The handler converts each applicable subfolder name to uppercase. It puts the names into a hash set.
4. In `IsMemberOf`, the handler encodes the given folder path with the Claude Code rule: an ASCII letter or digit stays, and each other character becomes a hyphen. Then the handler converts the result to uppercase and searches the hash set. This search is O(1) and touches no disk.
5. The uppercase conversion makes the comparison case-insensitive. This is necessary because Windows paths are not case-sensitive, but the encoded names keep the case.
6. A watcher thread monitors `%USERPROFILE%\.claude\projects\` with `ReadDirectoryChangesW`. When the folder changes, the thread assembles the hash set again. Then it calls `SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, path, NULL)` for each folder with a different result. Explorer then asks again and draws or removes the badge.

Note: The encode function must copy the Claude Code rule exactly. The rule is ASCII-only. A letter with an accent also becomes a hyphen.

## The implementation language

The reference stack for an icon-overlay handler is C++.
The interface is small, and a full handler is approximately 300 lines.
Leclaude uses plain C++ with a manual class factory. It does not use ATL.
This keeps the DLL free of dependencies.

We examined Rust with the windows-rs crate as an alternative.
Rust can supply COM interfaces, but no known open-source overlay handler in Rust exists.
C++ lets contributors compare our code with the reference projects.

## Installation and removal

The installation does these steps. It needs administrator rights.

1. Copy the DLL to a permanent location.
2. Register the COM class: `HKLM\Software\Classes\CLSID\{GUID}\InProcServer32` with the DLL path and `ThreadingModel = Apartment`.
3. Create the subkey `" Leclaude"` under `ShellIconOverlayIdentifiers`. Set its default value to the CLSID.
4. Add the CLSID to the "Approved" list: `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved`.
5. Restart Explorer, or ask the user for a restart.

The removal reverses these steps.
Explorer and other programs keep a lock on the DLL.
Thus the removal tool must restart Explorer, or it must schedule the file deletion for the next boot with `MoveFileEx` and `MOVEFILE_DELAY_UNTIL_REBOOT`.

## Debug procedure

1. In the Explorer options, enable "Launch folder windows in a separate process".
2. Compile the DLL. Stop Explorer, copy the DLL, and start Explorer again.
3. Use `OutputDebugString` and the DebugView tool for the log output.
4. Use the ShellExView tool to see the loaded handlers and the competition for the 15 slots.

## Test list

- The badge shows on a folder with session history.
- The badge does not show on a folder without session history.
- The badge shows on the desktop and in the file dialog of a 64-bit program.
- A change in `%USERPROFILE%\.claude\projects\` updates the badges.
- When the data folder is absent or not correct, `IsMemberOf` returns `S_FALSE` quickly. It must not hang.
- Explorer stays fast in a folder with thousands of items.

## The components

| Component | Function |
| --- | --- |
| `LeclaudeShell.dll` | The icon-overlay handler. C++, x64 and ARM64. |
| `leclaude.ico` | The badge icon, made from `assets/leclaudebot.png`. Overlay icons are small. The icon must stay clear at 10x10 pixels. |
| Installer | Registers the handler and restarts Explorer. |
| `leclaude doctor` (possible future tool) | A command-line tool that shows the detection result for a folder. This tool can read `.claude.json`, because it has no speed limits. |

## Reference projects

- TortoiseGit: `src/TortoiseShell/IconOverlay.cpp` and `src/TGitCache/`. The standard example of a fast handler with a cache.
- TortoiseOverlays: the shared handler set of the Tortoise tools. Their answer to the 15-slot limit.
- apriorit/IconOverlayHandler: a small and readable C++ example.

## Sources

- Microsoft: How to Implement Icon Overlay Handlers — https://learn.microsoft.com/en-us/windows/win32/shell/how-to-implement-icon-overlay-handlers
- Microsoft: How to Register Icon Overlay Handlers — https://learn.microsoft.com/en-us/windows/win32/shell/how-to-register-icon-overlay-handlers
- Microsoft: IShellIconOverlayIdentifier — https://learn.microsoft.com/en-us/windows/desktop/api/shobjidl_core/nn-shobjidl_core-ishelliconoverlayidentifier
- Raymond Chen: Why is there a limit of 15 shell icon overlays? — https://devblogs.microsoft.com/oldnewthing/20190313-00/?p=101094
- Microsoft: Guidance for Implementing In-Process Extensions — https://learn.microsoft.com/en-us/windows/win32/shell/shell-and-managed-code
- TortoiseGit internals — https://tortoisegit.org/docs/tortoisegit/tgit-app-internals.html
- The overlay icon battle (the leading-space competition) — https://cito.github.io/posts/2017-01-13-overlay-icon-battle/
- ShellExView — https://www.nirsoft.net/utils/shexview.html
