// Leclaude - the entry points of the handler DLL.

#include "module.h"

#include "class_factory.h"

#include <olectl.h>

#include <new>

HINSTANCE g_module = nullptr;

// The count of live COM objects and locks. DllCanUnloadNow reads it.
volatile LONG g_objectCount = 0;

HRESULT RegisterServer();
HRESULT UnregisterServer();

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = instance;
        // Explorer creates and stops many threads. The handler does not need the notifications.
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow()
{
    return (g_objectCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** result)
{
    if (result == nullptr)
    {
        return E_POINTER;
    }
    *result = nullptr;
    if (rclsid != CLSID_LeclaudeOverlay && rclsid != CLSID_LeclaudeContextMenu)
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    LeclaudeClassFactory* factory = new (std::nothrow) LeclaudeClassFactory(rclsid);
    if (factory == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = factory->QueryInterface(riid, result);
    factory->Release();
    return hr;
}

STDAPI DllRegisterServer()
{
    return RegisterServer();
}

STDAPI DllUnregisterServer()
{
    return UnregisterServer();
}
