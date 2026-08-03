// Leclaude - the icon-overlay handler.

#pragma once

#include <windows.h>
#include <shlobj.h>

class LeclaudeOverlay final : public IShellIconOverlayIdentifier
{
public:
    LeclaudeOverlay();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IShellIconOverlayIdentifier
    IFACEMETHODIMP IsMemberOf(PCWSTR path, DWORD attributes) override;
    IFACEMETHODIMP GetOverlayInfo(PWSTR iconFile, int iconFileSize, int* iconIndex,
                                  DWORD* flags) override;
    IFACEMETHODIMP GetPriority(int* priority) override;

private:
    ~LeclaudeOverlay();
    volatile LONG m_refCount = 1;
};
