# Leclaude

<img src="assets/leclaudebot.png" alt="The Leclaude robot" width="96" align="right">

Leclaude shows a robot badge in Windows Explorer on each folder that has [Claude Code](https://www.anthropic.com/claude-code) session history.
When you look through your disk, you can immediately see the folders where you did work with Claude Code.

Leclaude is a shell extension for Windows 10 and Windows 11.

## Status

Leclaude is in the design phase. The research is complete. The code is in development.

## How it operates

Claude Code records each opened folder in `%USERPROFILE%\.claude\projects\`.
Leclaude registers an icon-overlay handler with Explorer.
For each visible folder, the handler does a fast search in memory.
When the folder has session history, Explorer draws the robot badge on the folder icon.

The design documents give the full details:

- [The architecture](docs/architecture.md)
- [The Claude Code data format](docs/claude-data-format.md)

## Limits

- The installation needs administrator rights. Windows accepts overlay registrations in HKLM only.
- Windows shows a maximum of 15 overlay types for the full system. Cloud-storage tools use many of them. When too many tools compete, Windows can ignore the Leclaude badge. The ShellExView tool shows the competition on your system.

## Documentation language

All the text in this repository obeys [ASD-STE100](https://www.asd-ste100.org/) (Simplified Technical English), Issue 9.
STE is the international standard for clear technical documentation.
This project uses it as an experiment in readable open-source documentation.
