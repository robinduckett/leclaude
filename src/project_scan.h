// Leclaude - the pure scan functions. The DLL and the tests use these functions.

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

// Converts a folder path to the encoded name that Claude Code uses.
// An ASCII letter or digit stays. Each other character becomes a hyphen.
// The result is uppercase, so that two case variants of one path compare as equal.
std::wstring EncodeProjectPath(std::wstring_view path);

// Tells whether one project subfolder contains session history.
// True when the subfolder has one or more ".jsonl" files, or a
// "sessions-index.json" file with one or more entries.
bool DirectoryHasSessionHistory(const std::wstring& subfolderPath);

// Reads the projects root and returns the set of encoded names (uppercase)
// for the subfolders that have session history.
std::unordered_set<std::wstring> BuildProjectSet(const std::wstring& projectsRoot);

// Returns "%USERPROFILE%\.claude\projects". Returns an empty string on failure.
std::wstring GetProjectsRootPath();
