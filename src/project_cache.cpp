#include "project_cache.h"

#include "project_scan.h"

#include <shlobj.h>

#include <vector>

namespace
{

// The size limit of the map of the queried paths.
constexpr size_t kMaxQueriedPaths = 8192;

// The quiet period after a disk event, in milliseconds.
constexpr DWORD kDebounceMs = 2000;

// The minimum time between two watcher retries, in milliseconds.
constexpr ULONGLONG kRetryIntervalMs = 5000;

// The DLL must not unload while the watcher thread runs. This call pins the DLL
// in the process for the life of the process.
void PinModule()
{
    HMODULE unused = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&PinModule), &unused);
}

} // namespace

ProjectCache& ProjectCache::Instance()
{
    static ProjectCache instance;
    return instance;
}

void ProjectCache::EnsureStarted()
{
    InitOnceExecuteOnce(
        &m_initOnce,
        [](PINIT_ONCE, PVOID param, PVOID*) -> BOOL {
            auto* self = static_cast<ProjectCache*>(param);
            self->m_root = GetProjectsRootPath();
            auto projects = BuildProjectSet(self->m_root);
            AcquireSRWLockExclusive(&self->m_lock);
            self->m_projects = std::move(projects);
            ReleaseSRWLockExclusive(&self->m_lock);
            self->TryStartWatcher();
            return TRUE;
        },
        this, nullptr);
}

void ProjectCache::TryStartWatcher()
{
    if (m_root.empty())
    {
        return;
    }
    const DWORD attributes = GetFileAttributesW(m_root.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return;
    }
    PinModule();
    HANDLE thread = CreateThread(nullptr, 0, WatcherThreadProc, this, 0, nullptr);
    if (thread != nullptr)
    {
        CloseHandle(thread);
        AcquireSRWLockExclusive(&m_lock);
        m_watcherRunning = true;
        ReleaseSRWLockExclusive(&m_lock);
    }
}

DWORD WINAPI ProjectCache::WatcherThreadProc(void* param)
{
    static_cast<ProjectCache*>(param)->WatcherLoop();
    return 0;
}

void ProjectCache::WatcherLoop()
{
    // The filter has no "last write" flag. Thus the constant writes to an open
    // transcript file do not wake this thread. Only a create, a delete, or a
    // rename of a file or a folder wakes it.
    HANDLE watch = FindFirstChangeNotificationW(
        m_root.c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
    if (watch == INVALID_HANDLE_VALUE)
    {
        AcquireSRWLockExclusive(&m_lock);
        m_watcherRunning = false;
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    for (;;)
    {
        if (WaitForSingleObject(watch, INFINITE) != WAIT_OBJECT_0)
        {
            break;
        }
        // Wait until the disk is quiet. When the handle cannot arm again, stop the loop.
        bool armFailed = false;
        for (;;)
        {
            if (!FindNextChangeNotification(watch))
            {
                armFailed = true;
                break;
            }
            if (WaitForSingleObject(watch, kDebounceMs) == WAIT_TIMEOUT)
            {
                break;
            }
        }
        RebuildAndNotify();
        if (armFailed)
        {
            break;
        }
    }
    FindCloseChangeNotification(watch);
    AcquireSRWLockExclusive(&m_lock);
    m_watcherRunning = false;
    ReleaseSRWLockExclusive(&m_lock);
}

void ProjectCache::RebuildAndNotify()
{
    auto fresh = BuildProjectSet(m_root);

    std::vector<std::wstring> pathsToRefresh;

    AcquireSRWLockExclusive(&m_lock);
    for (const auto& name : fresh)
    {
        if (m_projects.count(name) == 0)
        {
            auto it = m_queried.find(name);
            if (it != m_queried.end())
            {
                pathsToRefresh.push_back(it->second);
            }
        }
    }
    for (const auto& name : m_projects)
    {
        if (fresh.count(name) == 0)
        {
            auto it = m_queried.find(name);
            if (it != m_queried.end())
            {
                pathsToRefresh.push_back(it->second);
            }
        }
    }
    m_projects = std::move(fresh);
    ReleaseSRWLockExclusive(&m_lock);

    for (const auto& path : pathsToRefresh)
    {
        SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW | SHCNF_FLUSHNOWAIT, path.c_str(), nullptr);
    }
}

bool ProjectCache::IsProjectFolder(const wchar_t* path)
{
    if (path == nullptr || path[0] == L'\0')
    {
        return false;
    }
    EnsureStarted();

    const std::wstring encoded = EncodeProjectPath(path);

    AcquireSRWLockExclusive(&m_lock);

    // When the watcher is not active, examine the root again after each retry interval.
    if (!m_watcherRunning)
    {
        const ULONGLONG now = GetTickCount64();
        if (now - m_lastRetryTick >= kRetryIntervalMs)
        {
            m_lastRetryTick = now;
            ReleaseSRWLockExclusive(&m_lock);
            TryStartWatcher();
            RebuildAndNotify();
            AcquireSRWLockExclusive(&m_lock);
        }
    }

    if (m_queried.size() >= kMaxQueriedPaths)
    {
        m_queried.clear();
    }
    m_queried[encoded] = path;
    const bool isProject = m_projects.count(encoded) != 0;
    ReleaseSRWLockExclusive(&m_lock);
    return isProject;
}
