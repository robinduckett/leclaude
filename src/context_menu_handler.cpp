#include "context_menu_handler.h"

#include "module.h"
#include "project_cache.h"
#include "resource.h"

#include <shellapi.h>
#include <strsafe.h>

namespace
{

// The command offsets. QueryContextMenu adds them to idCmdFirst.
enum : UINT
{
    kCommandTerminal = 0,
    kCommandPowerShell = 1,
    kCommandCount = 2
};

// The verbs are language-independent identifiers. A program can start the
// menu commands with these names through ShellExecuteEx.
constexpr wchar_t kVerbTerminalW[] = L"leclaude_terminal";
constexpr wchar_t kVerbPowerShellW[] = L"leclaude_powershell";
constexpr char kVerbTerminalA[] = "leclaude_terminal";
constexpr char kVerbPowerShellA[] = "leclaude_powershell";

// Reads a text from the string table. The resource loader selects the
// LANGUAGE block for the UI language of the user.
std::wstring LoadResourceString(UINT id)
{
    wchar_t buffer[128] = {};
    const int length = LoadStringW(g_module, id, buffer, ARRAYSIZE(buffer));
    if (length <= 0)
    {
        return std::wstring();
    }
    return std::wstring(buffer, static_cast<size_t>(length));
}

// Makes a bitmap with an alpha channel from the icon resource. A menu
// shows an hbmpItem bitmap with its alpha channel on Windows Vista and
// later. A null result means: show the menu command without an icon.
HBITMAP CreateMenuBitmap(int size)
{
    HICON icon = static_cast<HICON>(
        LoadImageW(g_module, MAKEINTRESOURCEW(1), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    if (icon == nullptr)
    {
        return nullptr;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size; // negative: the rows start at the top
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = nullptr;
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    if (memory != nullptr)
    {
        bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bitmap != nullptr)
        {
            HGDIOBJ previous = SelectObject(memory, bitmap);
            DrawIconEx(memory, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);
            SelectObject(memory, previous);
            GdiFlush();

            // An icon without an alpha channel gives a black square here.
            // Then the menu command is better without an icon.
            bool hasAlpha = false;
            const BYTE* pixels = static_cast<const BYTE*>(bits);
            for (int index = 3; index < size * size * 4; index += 4)
            {
                if (pixels[index] != 0)
                {
                    hasAlpha = true;
                    break;
                }
            }
            if (!hasAlpha)
            {
                DeleteObject(bitmap);
                bitmap = nullptr;
            }
        }
        DeleteDC(memory);
    }
    ReleaseDC(nullptr, screen);
    DestroyIcon(icon);
    return bitmap;
}

// Gives the shared menu bitmap. An open menu keeps the bitmap in use.
// Thus the cache does not delete a bitmap. The process end releases the
// handles. A new bitmap comes only when the icon size changes.
HBITMAP GetMenuBitmap()
{
    static SRWLOCK lock = SRWLOCK_INIT;
    static int cachedSize = 0;
    static HBITMAP cachedBitmap = nullptr;

    const int size = GetSystemMetrics(SM_CXSMICON);
    AcquireSRWLockExclusive(&lock);
    if (size != cachedSize)
    {
        cachedBitmap = CreateMenuBitmap(size);
        cachedSize = size;
    }
    HBITMAP result = cachedBitmap;
    ReleaseSRWLockExclusive(&lock);
    return result;
}

// Puts quotation marks around a folder path for a command line. A path that
// ends with a backslash (a drive root) gets one more backslash. A single
// backslash before the closing quotation mark changes the meaning of the mark.
std::wstring QuoteFolderArgument(const std::wstring& folder)
{
    std::wstring quoted = L"\"" + folder;
    if (!folder.empty() && folder.back() == L'\\')
    {
        quoted += L'\\';
    }
    quoted += L'"';
    return quoted;
}

// Starts a program with the folder as its start folder.
HRESULT LaunchInFolder(const wchar_t* file, const std::wstring& parameters,
                       const std::wstring& folder, int show)
{
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    // Microsoft recommends SEE_MASK_NOASYNC for a menu handler. The thread
    // of the menu can end directly after the call.
    info.fMask = SEE_MASK_NOASYNC;
    info.lpFile = file;
    info.lpParameters = parameters.c_str();
    info.lpDirectory = folder.c_str();
    info.nShow = show;
    if (!ShellExecuteExW(&info))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    return S_OK;
}

// Finds the Windows Terminal alias of the user. The alias is a reparse
// point with zero bytes. Thus the test is GetFileAttributesW, not a size test.
bool FindWindowsTerminal(std::wstring& aliasPath)
{
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)))
    {
        return false;
    }
    aliasPath.assign(localAppData);
    CoTaskMemFree(localAppData);
    aliasPath += L"\\Microsoft\\WindowsApps\\wt.exe";
    return GetFileAttributesW(aliasPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Starts Claude Code in Windows Terminal. Without Windows Terminal, the
// function starts Claude Code in a plain console. The /k flag keeps the
// window open. Then the user can read an error message when the claude
// command does not start.
HRESULT LaunchTerminal(const std::wstring& folder, int show)
{
    std::wstring alias;
    if (FindWindowsTerminal(alias))
    {
        const std::wstring parameters = L"-d " + QuoteFolderArgument(folder) + L" cmd /k claude";
        if (SUCCEEDED(LaunchInFolder(alias.c_str(), parameters, folder, show)))
        {
            return S_OK;
        }
    }
    return LaunchInFolder(L"cmd.exe", L"/k claude", folder, show);
}

// Starts Claude Code in PowerShell. The function prefers PowerShell 7.
// The -NoExit flag keeps the window open, as /k does for cmd.
HRESULT LaunchPowerShell(const std::wstring& folder, int show)
{
    const wchar_t* file = L"powershell.exe";
    wchar_t found[MAX_PATH];
    if (SearchPathW(nullptr, L"pwsh.exe", nullptr, MAX_PATH, found, nullptr) != 0)
    {
        file = found;
    }
    return LaunchInFolder(file, L"-NoExit -Command claude", folder, show);
}

} // namespace

LeclaudeContextMenu::LeclaudeContextMenu()
{
    InterlockedIncrement(&g_objectCount);
}

LeclaudeContextMenu::~LeclaudeContextMenu()
{
    InterlockedDecrement(&g_objectCount);
}

IFACEMETHODIMP LeclaudeContextMenu::QueryInterface(REFIID riid, void** result)
{
    if (result == nullptr)
    {
        return E_POINTER;
    }
    // The class has two interface bases. The casts select one base.
    if (riid == IID_IUnknown || riid == IID_IShellExtInit)
    {
        *result = static_cast<IShellExtInit*>(this);
        AddRef();
        return S_OK;
    }
    if (riid == IID_IContextMenu)
    {
        *result = static_cast<IContextMenu*>(this);
        AddRef();
        return S_OK;
    }
    *result = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) LeclaudeContextMenu::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) LeclaudeContextMenu::Release()
{
    const ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0)
    {
        delete this;
    }
    return count;
}

IFACEMETHODIMP LeclaudeContextMenu::Initialize(PCIDLIST_ABSOLUTE folderPidl,
                                               IDataObject* dataObject, HKEY progId)
{
    (void)progId;
    m_folder.clear();

    // This function must not touch the disk. A disk access on a dead
    // network path can freeze Explorer while the menu is open.
    std::wstring path;
    if (dataObject != nullptr)
    {
        // The selection case: the data object holds the selected items.
        FORMATETC format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium = {};
        if (FAILED(dataObject->GetData(&format, &medium)))
        {
            return S_OK;
        }
        HDROP drop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
        if (drop != nullptr)
        {
            // The rule: exactly one selected item.
            if (DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) == 1)
            {
                const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
                if (length > 0)
                {
                    path.resize(length);
                    DragQueryFileW(drop, 0, path.data(), length + 1);
                }
            }
            GlobalUnlock(medium.hGlobal);
        }
        ReleaseStgMedium(&medium);
    }
    else if (folderPidl != nullptr)
    {
        // The background case: the pidl names the open folder. A failure
        // means a virtual folder without a path.
        wchar_t buffer[MAX_PATH];
        if (SHGetPathFromIDListW(folderPidl, buffer))
        {
            path = buffer;
        }
    }

    // The menu commands show only on a folder with session history.
    if (!path.empty() && ProjectCache::Instance().IsProjectFolder(path.c_str()))
    {
        m_folder = std::move(path);
    }
    return S_OK;
}

IFACEMETHODIMP LeclaudeContextMenu::QueryContextMenu(HMENU menu, UINT indexMenu, UINT idCmdFirst,
                                                     UINT idCmdLast, UINT flags)
{
    // Explorer sets CMF_DEFAULTONLY when it finds the default command for
    // a double-click. The handler must not change the menu then.
    if ((flags & CMF_DEFAULTONLY) != 0 || m_folder.empty())
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }
    // The command IDs must stay in the given range.
    if (idCmdFirst + kCommandCount - 1 > idCmdLast)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }

    const std::wstring textTerminal = LoadResourceString(IDS_MENU_TERMINAL);
    const std::wstring textPowerShell = LoadResourceString(IDS_MENU_POWERSHELL);
    if (textTerminal.empty() || textPowerShell.empty())
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }

    InsertMenuW(menu, indexMenu, MF_STRING | MF_BYPOSITION,
                static_cast<UINT_PTR>(idCmdFirst) + kCommandTerminal, textTerminal.c_str());
    InsertMenuW(menu, indexMenu + 1, MF_STRING | MF_BYPOSITION,
                static_cast<UINT_PTR>(idCmdFirst) + kCommandPowerShell, textPowerShell.c_str());

    // The robot icon marks the two menu commands.
    HBITMAP robotBitmap = GetMenuBitmap();
    if (robotBitmap != nullptr)
    {
        MENUITEMINFOW item = {};
        item.cbSize = sizeof(item);
        item.fMask = MIIM_BITMAP;
        item.hbmpItem = robotBitmap;
        SetMenuItemInfoW(menu, indexMenu, TRUE, &item);
        SetMenuItemInfoW(menu, indexMenu + 1, TRUE, &item);
    }

    // The code part tells the host the highest used offset plus one.
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, kCommandCount);
}

IFACEMETHODIMP LeclaudeContextMenu::InvokeCommand(CMINVOKECOMMANDINFO* info)
{
    if (info == nullptr)
    {
        return E_INVALIDARG;
    }
    if (m_folder.empty())
    {
        return E_FAIL;
    }

    // The verb comes in three forms: an offset, an ANSI string, or a
    // Unicode string in the extended structure.
    UINT command = kCommandCount;
    if (IS_INTRESOURCE(info->lpVerb))
    {
        command = LOWORD(reinterpret_cast<UINT_PTR>(info->lpVerb));
    }
    else if (_stricmp(info->lpVerb, kVerbTerminalA) == 0)
    {
        command = kCommandTerminal;
    }
    else if (_stricmp(info->lpVerb, kVerbPowerShellA) == 0)
    {
        command = kCommandPowerShell;
    }
    else if (info->cbSize >= sizeof(CMINVOKECOMMANDINFOEX) &&
             (info->fMask & CMIC_MASK_UNICODE) != 0)
    {
        const auto* extended = reinterpret_cast<const CMINVOKECOMMANDINFOEX*>(info);
        if (extended->lpVerbW != nullptr && !IS_INTRESOURCE(extended->lpVerbW))
        {
            if (_wcsicmp(extended->lpVerbW, kVerbTerminalW) == 0)
            {
                command = kCommandTerminal;
            }
            else if (_wcsicmp(extended->lpVerbW, kVerbPowerShellW) == 0)
            {
                command = kCommandPowerShell;
            }
        }
    }

    if (command == kCommandTerminal)
    {
        return LaunchTerminal(m_folder, info->nShow);
    }
    if (command == kCommandPowerShell)
    {
        return LaunchPowerShell(m_folder, info->nShow);
    }
    // For an unknown verb or offset, no process starts.
    return E_FAIL;
}

IFACEMETHODIMP LeclaudeContextMenu::GetCommandString(UINT_PTR command, UINT type, UINT* reserved,
                                                     CHAR* name, UINT nameSize)
{
    (void)reserved;
    if (type == GCS_VALIDATEA || type == GCS_VALIDATEW)
    {
        return (command < kCommandCount) ? S_OK : S_FALSE;
    }
    if (command >= kCommandCount)
    {
        return E_INVALIDARG;
    }

    // For the W types, the name buffer is a wide buffer. The cast is the
    // documented contract of this API.
    PWSTR wideName = reinterpret_cast<PWSTR>(name);
    switch (type)
    {
        case GCS_VERBW:
            return StringCchCopyW(wideName, nameSize,
                                  command == kCommandTerminal ? kVerbTerminalW : kVerbPowerShellW);
        case GCS_HELPTEXTW:
        {
            const std::wstring help = LoadResourceString(
                command == kCommandTerminal ? IDS_HELP_TERMINAL : IDS_HELP_POWERSHELL);
            return StringCchCopyW(wideName, nameSize, help.c_str());
        }
        default:
            return E_NOTIMPL;
    }
}
