// Leclaude - the menu handler.

#pragma once

#include <windows.h>
#include <shlobj.h>

#include <string>

class LeclaudeContextMenu final : public IShellExtInit, public IContextMenu
{
public:
    LeclaudeContextMenu();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IShellExtInit
    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE folderPidl, IDataObject* dataObject,
                              HKEY progId) override;

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(HMENU menu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast,
                                    UINT flags) override;
    IFACEMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* info) override;
    IFACEMETHODIMP GetCommandString(UINT_PTR command, UINT type, UINT* reserved, CHAR* name,
                                    UINT nameSize) override;

private:
    ~LeclaudeContextMenu();
    volatile LONG m_refCount = 1;
    // The target folder. An empty value means: insert no menu commands.
    std::wstring m_folder;
};
