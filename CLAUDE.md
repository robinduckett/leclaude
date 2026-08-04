# CLAUDE.md

This file guides Claude Code when it operates in this repository.

## The project

Leclaude is an open-source shell extension for Windows 10 and Windows 11.
It shows a small robot badge on each folder that has Claude Code session history.
The user can then find these folders quickly in Explorer.
The project is part of the portfolio and blog at robinduckett.com.

Project status: the implementation is complete. The tests pass.
The manual verification in Explorer is open: install the handler with administrator rights and look at the badges.

## Rule 1: All human-facing English must be ASD-STE100

This is the primary rule of the repository.
Before you write or change any human-facing text, load the skill `asd-ste100` if it is available.
The skill is a private user skill. It is not a part of the repository.
Without the skill, follow the official specification, Issue 9: https://www.asd-ste100.org/
The rule applies to:

- README files and all documents
- code comments
- commit messages
- PR titles and PR descriptions
- the GitHub repository description and the release notes
- error messages and UI strings

The rule does not apply to code identifiers, API names, or quoted text.

## Terminology

STE requires one name for one item. This project uses these names:

| Item | Name to use | Do not use |
| --- | --- | --- |
| A file-system container | folder | directory |
| The overlay icon on a folder | badge | overlay icon, emblem, decal |
| The COM object | handler | extension object |
| The Windows file manager | Explorer | File Explorer, the shell UI |
| The recorded Claude Code data for a folder | session history | transcripts, logs |

## Key documents

- `docs/architecture.md` — the design of the handler, the Windows facts, and the sources.
- `docs/claude-data-format.md` — how Claude Code records the opened folders. The facts in this document are verified against a real system.

## Key technical facts

- The handler supplies `IShellIconOverlayIdentifier`. This is the only supported mechanism for a badge on a folder.
- The registration is in HKLM only. The installation needs administrator rights.
- Windows loads a maximum of 15 overlay handlers, in alphabetical order of the subkey names. Leclaude uses the subkey name `" Leclaude"` (one space before the name).
- `IsMemberOf` must be very fast. It must not touch the disk. The design uses a hash set in memory. See `docs/architecture.md`.
- The detection rule: encode the folder path (each character that is not an ASCII letter or digit becomes a hyphen). Search the encoded name in the set of known project folders from `%USERPROFILE%\.claude\projects\`. Compare without case sensitivity.
- The stack is plain C++ without ATL, with a static C runtime, for x64 and ARM64.

## Build

The build uses CMake with the Ninja generator.
Visual Studio contains both tools in the component "C++ CMake tools for Windows".
The minimum is CMake 3.26. Visual Studio 2026 satisfies this.

1. Open a Developer PowerShell for Visual Studio.
2. Enter: `cmake --preset x64-release`
3. Enter: `cmake --build --preset x64-release`

The installers package the DLL from the build folder.
After each change of the source or the version, compile again before you make an installer.

The presets are `x64-debug`, `x64-release`, and `arm64-release`.
For the `arm64-release` preset, open the developer shell with the `x64_arm64` tools.
As an alternative, open the folder in Visual Studio. Visual Studio finds the presets and prepares the environment.

## GitHub metadata

Repository description (STE): "Leclaude shows a robot badge in Explorer on each folder that has Claude Code session history."

## Decided

- The license is MIT.
- The build system is CMake with Ninja, through the Visual Studio toolchain.
- The installer is an Inno Setup program: `installer/leclaude.iss`. The zip file gives the manual alternative: `scripts/install.cmd` starts `scripts/install.ps1` with administrator rights.
- There is no MSI package. A test on a real system showed the problem: the Restart Manager stopped Explorer and the installation then did not continue. The setup program gives the correct installation experience. Winget accepts a setup program from Inno Setup.
- The releases come from the GitHub workflow `.github/workflows/release.yml`. The workflow attests each zip file with GitHub artifact attestation.

## Release procedure

1. Make sure that the CI workflow is green.
2. Update the version numbers in `src/resources.rc`.
3. Make a tag with the form `vX.Y.Z` and push it.
4. The release workflow compiles both architectures, does the tests, packages the zip files, attests them, and publishes the release.
