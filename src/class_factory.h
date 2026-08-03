// Leclaude - the class factory for the overlay handler.

#pragma once

#include <windows.h>
#include <unknwn.h>

class LeclaudeClassFactory final : public IClassFactory
{
public:
    LeclaudeClassFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** result) override;
    IFACEMETHODIMP LockServer(BOOL lock) override;

private:
    ~LeclaudeClassFactory();
    volatile LONG m_refCount = 1;
};
