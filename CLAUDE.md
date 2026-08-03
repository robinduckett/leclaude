# CLAUDE.md

This file guides Claude Code when it operates in this repository.

## The project

Leclaude is an open-source shell extension for Windows 10 and Windows 11.
It shows a small robot badge on each folder that has Claude Code session history.
The user can then find these folders quickly in Explorer.
The project is part of the portfolio and blog at robinduckett.com.

Project status: design phase. The research is complete. The code does not exist yet.

## Rule 1: All human-facing English must be ASD-STE100

This is the primary rule of the repository.
Before you write or change any human-facing text, load the skill `asd-ste100` from `.claude/skills/asd-ste100/SKILL.md`.
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
- `.claude/skills/asd-ste100/SKILL.md` — the writing rules.

## Key technical facts

- The handler supplies `IShellIconOverlayIdentifier`. This is the only supported mechanism for a badge on a folder.
- The registration is in HKLM only. The installation needs administrator rights.
- Windows loads a maximum of 15 overlay handlers, in alphabetical order of the subkey names. Leclaude uses the subkey name `" Leclaude"` (one space before the name).
- `IsMemberOf` must be very fast. It must not touch the disk. The design uses a hash set in memory. See `docs/architecture.md`.
- The detection rule: encode the folder path (each character that is not an ASCII letter or digit becomes a hyphen). Search the encoded name in the set of known project folders from `%USERPROFILE%\.claude\projects\`. Compare without case sensitivity.
- The stack is plain C++ without ATL, with a static C runtime, for x64 and ARM64.

## GitHub metadata

Repository description (STE): "Leclaude shows a robot badge in Explorer on each folder that has Claude Code session history."

## Not decided yet

- The license. Ask the user before you add a license file.
- The build system (CMake or MSBuild).
- The installer technology (MSI, Inno Setup, or a script).
