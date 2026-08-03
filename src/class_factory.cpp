#include "class_factory.h"

#include "module.h"
#include "overlay_handler.h"

#include <new>

LeclaudeClassFactory::LeclaudeClassFactory()
{
    InterlockedIncrement(&g_objectCount);
}

LeclaudeClassFactory::~LeclaudeClassFactory()
{
    InterlockedDecrement(&g_objectCount);
}

IFACEMETHODIMP LeclaudeClassFactory::QueryInterface(REFIID riid, void** result)
{
    if (result == nullptr)
    {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *result = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *result = nullptr;
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) LeclaudeClassFactory::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) LeclaudeClassFactory::Release()
{
    const ULONG count = InterlockedDecrement(&m_refCount);
    if (count == 0)
    {
        delete this;
    }
    return count;
}

IFACEMETHODIMP LeclaudeClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** result)
{
    if (result == nullptr)
    {
        return E_POINTER;
    }
    *result = nullptr;
    if (outer != nullptr)
    {
        return CLASS_E_NOAGGREGATION;
    }
    LeclaudeOverlay* handler = new (std::nothrow) LeclaudeOverlay();
    if (handler == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = handler->QueryInterface(riid, result);
    handler->Release();
    return hr;
}

IFACEMETHODIMP LeclaudeClassFactory::LockServer(BOOL lock)
{
    if (lock)
    {
        InterlockedIncrement(&g_objectCount);
    }
    else
    {
        InterlockedDecrement(&g_objectCount);
    }
    return S_OK;
}
