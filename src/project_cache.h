// Leclaude - the in-memory cache of the known project folders.

#pragma once

#include <windows.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

// The cache holds the encoded names of the folders that have session history.
// IsMemberOf does one hash-set search here and touches no disk.
class ProjectCache
{
public:
    static ProjectCache& Instance();

    // Returns true when the folder at "path" has Claude Code session history.
    // Also records the path, so that a later change can refresh the badge.
    bool IsProjectFolder(const wchar_t* path);

private:
    ProjectCache() = default;

    void EnsureStarted();
    void TryStartWatcher();
    void RebuildAndNotify();
    void WatcherLoop();
    static DWORD WINAPI WatcherThreadProc(void* param);

    SRWLOCK m_lock = SRWLOCK_INIT;
    std::unordered_set<std::wstring> m_projects;              // encoded names, uppercase
    std::unordered_map<std::wstring, std::wstring> m_queried; // encoded name -> a real path that Explorer gave
    std::wstring m_root;
    bool m_watcherRunning = false;
    ULONGLONG m_lastRetryTick = 0;
    INIT_ONCE m_initOnce = INIT_ONCE_STATIC_INIT;
};
