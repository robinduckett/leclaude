#include "overlay_handler.h"

#include "module.h"
#include "project_cache.h"

#include <strsafe.h>

LeclaudeOverlay::LeclaudeOverlay()
{
    InterlockedIncrement(&g_objectCount);
}

LeclaudeOverlay::~LeclaudeOverlay()
{
    InterlockedDecrement(&g_objectCount);
}

IFACEMETHODIMP LeclaudeOverlay::QueryInterface(REFIID riid, void** result)
{
    if (result == nullptr)
    {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IShellIconOverlayIdentifier)
    {
        *result = static_cast<IShellIconOverlayIdentifier*>(this);
        AddRef();
        return S_OK;
    }
    *result = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) LeclaudeOverlay::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) LeclaudeOverlay::Release()
{
    const ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0)
    {
        delete this;
    }
    return count;
}

IFACEMETHODIMP LeclaudeOverlay::IsMemberOf(PCWSTR path, DWORD attributes)
{
    // Explorer calls this function for each visible item, on its UI thread.
    // The folder test is one hash-set search in memory.
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        return S_FALSE;
    }
    return ProjectCache::Instance().IsProjectFolder(path) ? S_OK : S_FALSE;
}

IFACEMETHODIMP LeclaudeOverlay::GetOverlayInfo(PWSTR iconFile, int iconFileSize, int* iconIndex,
                                               DWORD* flags)
{
    if (iconFile == nullptr || iconIndex == nullptr || flags == nullptr)
    {
        return E_POINTER;
    }
    // The badge icon is the first icon resource of this DLL.
    if (GetModuleFileNameW(g_module, iconFile, iconFileSize) == 0)
    {
        return E_FAIL;
    }
    *iconIndex = 0;
    *flags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
    return S_OK;
}

IFACEMETHODIMP LeclaudeOverlay::GetPriority(int* priority)
{
    if (priority == nullptr)
    {
        return E_POINTER;
    }
    *priority = 0;
    return S_OK;
}
