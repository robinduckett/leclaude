// Leclaude - the class factory for the two handlers.

#pragma once

#include <windows.h>
#include <unknwn.h>

class LeclaudeClassFactory final : public IClassFactory
{
public:
    // The CLSID selects the class that CreateInstance makes.
    explicit LeclaudeClassFactory(REFCLSID clsid);

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
    const CLSID m_clsid;
};
