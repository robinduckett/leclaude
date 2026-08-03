// Leclaude - the entry points of the handler DLL.

#include <windows.h>
#include <olectl.h>

HINSTANCE g_module = nullptr;

// The count of live COM objects and locks. The handler increments and decrements it.
volatile LONG g_objectCount = 0;

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

STDAPI DllGetClassObject(REFCLSID, REFIID, void** result)
{
    if (result == nullptr)
    {
        return E_POINTER;
    }
    *result = nullptr;
    // The class factory for the handler comes in a later change.
    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllRegisterServer()
{
    // The registration code comes in a later change.
    return E_NOTIMPL;
}

STDAPI DllUnregisterServer()
{
    return E_NOTIMPL;
}
