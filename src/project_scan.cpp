#include "project_scan.h"

#include <windows.h>
#include <shlobj.h>

#include <memory>

std::wstring EncodeProjectPath(std::wstring_view path)
{
    std::wstring encoded;
    encoded.reserve(path.size());
    for (wchar_t c : path)
    {
        if (c >= L'a' && c <= L'z')
        {
            encoded.push_back(static_cast<wchar_t>(c - L'a' + L'A'));
        }
        else if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9'))
        {
            encoded.push_back(c);
        }
        else
        {
            encoded.push_back(L'-');
        }
    }
    return encoded;
}

namespace
{

bool HasJsonlFile(const std::wstring& folder)
{
    WIN32_FIND_DATAW data{};
    const std::wstring pattern = folder + L"\\*.jsonl";
    HANDLE find = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data,
                                   FindExSearchNameMatch, nullptr, 0);
    if (find == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    bool found = false;
    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            found = true;
            break;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return found;
}

// A sessions index counts when it names one or more sessions.
// A full JSON parse is not necessary. The text "sessionId" only occurs inside an entry.
bool SessionsIndexHasEntries(const std::wstring& folder)
{
    const std::wstring indexPath = folder + L"\\sessions-index.json";
    HANDLE file = CreateFileW(indexPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    char buffer[8192];
    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(file, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
    CloseHandle(file);
    if (!ok || bytesRead == 0)
    {
        return false;
    }
    buffer[bytesRead] = '\0';
    return strstr(buffer, "\"sessionId\"") != nullptr;
}

} // namespace

bool DirectoryHasSessionHistory(const std::wstring& subfolderPath)
{
    return HasJsonlFile(subfolderPath) || SessionsIndexHasEntries(subfolderPath);
}

std::unordered_set<std::wstring> BuildProjectSet(const std::wstring& projectsRoot)
{
    std::unordered_set<std::wstring> result;
    if (projectsRoot.empty())
    {
        return result;
    }

    WIN32_FIND_DATAW data{};
    const std::wstring pattern = projectsRoot + L"\\*";
    HANDLE find = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data,
                                   FindExSearchNameMatch, nullptr, 0);
    if (find == INVALID_HANDLE_VALUE)
    {
        return result;
    }
    do
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            continue;
        }
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
        {
            continue;
        }
        const std::wstring subfolder = projectsRoot + L"\\" + data.cFileName;
        if (DirectoryHasSessionHistory(subfolder))
        {
            // The subfolder name is already an encoded name. Only the case can be different.
            result.insert(EncodeProjectPath(data.cFileName));
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return result;
}

std::wstring GetProjectsRootPath()
{
    PWSTR profile = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &profile)))
    {
        return {};
    }
    std::wstring root(profile);
    CoTaskMemFree(profile);
    root += L"\\.claude\\projects";
    return root;
}
