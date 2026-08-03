// Leclaude - the COM smoke test.
// The test loads the handler DLL in the same way that Explorer loads it.
// It does not touch the registry. Thus it does not need administrator rights.
//
// Usage: leclaude_smoke [expected-project-folder]
// With the optional argument, the test also makes sure that IsMemberOf
// returns S_OK for that folder. The continuous-integration build does not
// give the argument, because the build machine has no Claude Code data.

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <string>

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do                                                                     \
    {                                                                      \
        if (!(cond))                                                       \
        {                                                                  \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

// {AEB0D999-FC8F-4FFA-B160-D4506164F0E7}
static constexpr CLSID kClsid =
    { 0xAEB0D999, 0xFC8F, 0x4FFA, { 0xB1, 0x60, 0xD4, 0x50, 0x61, 0x64, 0xF0, 0xE7 } };

using DllGetClassObjectProc = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);

int wmain(int argc, wchar_t** argv)
{
    // The DLL is in the same folder as this program.
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
    std::wstring path(dllPath);
    path = path.substr(0, path.find_last_of(L'\\') + 1) + L"LeclaudeShell.dll";

    HMODULE dll = LoadLibraryW(path.c_str());
    CHECK(dll != nullptr);
    if (dll == nullptr)
    {
        return 1;
    }

    auto getClassObject =
        reinterpret_cast<DllGetClassObjectProc>(GetProcAddress(dll, "DllGetClassObject"));
    CHECK(getClassObject != nullptr);
    CHECK(GetProcAddress(dll, "DllCanUnloadNow") != nullptr);
    CHECK(GetProcAddress(dll, "DllRegisterServer") != nullptr);
    CHECK(GetProcAddress(dll, "DllUnregisterServer") != nullptr);
    if (getClassObject == nullptr)
    {
        return 1;
    }

    IClassFactory* factory = nullptr;
    CHECK(SUCCEEDED(getClassObject(kClsid, IID_IClassFactory, reinterpret_cast<void**>(&factory))));
    if (factory == nullptr)
    {
        return 1;
    }

    // A different CLSID must not give a factory.
    IClassFactory* wrongFactory = nullptr;
    CHECK(getClassObject(IID_IUnknown, IID_IClassFactory,
                         reinterpret_cast<void**>(&wrongFactory)) == CLASS_E_CLASSNOTAVAILABLE);

    IShellIconOverlayIdentifier* handler = nullptr;
    CHECK(SUCCEEDED(factory->CreateInstance(nullptr, IID_IShellIconOverlayIdentifier,
                                            reinterpret_cast<void**>(&handler))));
    factory->Release();
    if (handler == nullptr)
    {
        return 1;
    }

    // GetOverlayInfo must return the DLL path and the icon index 0.
    wchar_t iconFile[MAX_PATH] = {};
    int iconIndex = -1;
    DWORD flags = 0;
    CHECK(handler->GetOverlayInfo(iconFile, MAX_PATH, &iconIndex, &flags) == S_OK);
    CHECK(iconIndex == 0);
    CHECK(flags == (ISIOI_ICONFILE | ISIOI_ICONINDEX));
    CHECK(_wcsicmp(iconFile, path.c_str()) == 0);

    // The icon resource must be present in the DLL.
    HICON icon = nullptr;
    UINT extracted = ExtractIconExW(iconFile, iconIndex, nullptr, &icon, 1);
    CHECK(extracted == 1);
    CHECK(icon != nullptr);
    if (icon != nullptr)
    {
        DestroyIcon(icon);
    }

    int priority = -1;
    CHECK(handler->GetPriority(&priority) == S_OK);
    CHECK(priority == 0);

    // A file must not get the badge. The attribute flags say "not a folder".
    CHECK(handler->IsMemberOf(L"C:\\Windows\\notepad.exe", FILE_ATTRIBUTE_ARCHIVE) == S_FALSE);

    // The temporary folder has no session history.
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    CHECK(handler->IsMemberOf(tempPath, FILE_ATTRIBUTE_DIRECTORY) == S_FALSE);

    if (argc > 1)
    {
        // The given folder must have session history.
        const HRESULT hr = handler->IsMemberOf(argv[1], FILE_ATTRIBUTE_DIRECTORY);
        CHECK(hr == S_OK);
        std::wprintf(L"IsMemberOf(%s) = 0x%08lx\n", argv[1], hr);
    }

    handler->Release();

    if (g_failures == 0)
    {
        std::printf("All checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d checks failed.\n", g_failures);
    return 1;
}
