# Claude Code data format

This document tells you how Claude Code records the folders that it opened.
The Leclaude shell extension reads this data to find the folders that it must badge.

## Where Claude Code keeps its data

Claude Code keeps its data in the user profile:

- `%USERPROFILE%\.claude\projects\` — one subfolder for each project folder that Claude Code opened.
- `%USERPROFILE%\.claude.json` — a JSON file with a `projects` object. The keys of this object are the full paths of the opened folders.

## The encoded folder name

Claude Code makes the name of each project subfolder from the full path of the project folder.
The rule is: each character that is not a letter or a digit becomes a hyphen (`-`).

Examples from a real system:

| Project folder path | Encoded folder name |
| --- | --- |
| `L:\Projects\Leclaude` | `L--Projects-Leclaude` |
| `C:\Users\robin\source\repos\Mover` | `C--Users-robin-source-repos-Mover` |

Note: The encoded name is not reversible. The path `C:\a\b-c` and the path `C:\a\b\c` give the same encoded name.
This is not a problem for Leclaude. Leclaude starts from a known folder path and computes the encoded name. It does not decode names.

We made sure that the rule is correct. We compared the 43 project paths in `.claude.json` with the real folder names in `projects\`:

- A space becomes a hyphen: `E:/Godot Projects/probe` → `E--Godot-Projects-probe`.
- A dot becomes a hyphen: `C:/Users/robin/.claude` → `C--Users-robin--claude`.
- A forward slash and a backslash both become a hyphen.
- The rule keeps the letter case. The path `L:/Projects/c2` and the path `L:/Projects/C2` give different encoded names.

Important: Windows paths are not case-sensitive, but the encoded names keep the case.
Leclaude must not compare encoded names as strings.
Leclaude must do a folder-existence check on the disk. On a standard NTFS volume, this check is not case-sensitive. Thus the check finds the folder for each case variant of the path.

## Contents of a project subfolder

A project subfolder can contain these items:

- `<session-id>.jsonl` — the transcript of one session. The session ID is a UUID.
- `sessions-index.json` — an index of the sessions. It has a `version` field and an `entries` array. Each entry has `sessionId`, `fullPath`, `fileMtime`, `firstPrompt`, `summary`, `messageCount`, and `created`.
- `memory\` — the persistent memory files for the project.

Important: Not all project subfolders contain `.jsonl` files.
Some subfolders contain only `sessions-index.json`. Some subfolders contain only a `memory\` folder.

## How Leclaude decides that a folder has session history

A folder has session history when all these conditions are true:

1. The encoded project subfolder exists in `%USERPROFILE%\.claude\projects\`.
2. The subfolder contains one or more `.jsonl` files, or it contains a `sessions-index.json` file with one or more entries.

A subfolder that contains only a `memory\` folder does not satisfy condition 2.
A configuration option can let the user select a different rule. For example, the user can select "badge each opened folder".

## Why Leclaude does not read `.claude.json`

The file `%USERPROFILE%\.claude.json` also lists the opened folders. But Leclaude does not use it in the icon-overlay path, for these reasons:

- The file is large, and Claude Code writes to it frequently.
- A JSON parse is too slow for the Explorer icon path. Explorer asks about many folders in a short time.
- A check for one folder on the disk is fast and does not need a parse.

Leclaude can use `.claude.json` in tools that are not speed-critical. An example is a diagnostic command.
