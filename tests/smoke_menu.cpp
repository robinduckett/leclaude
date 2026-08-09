// Leclaude - the menu smoke test.
// The test loads the handler DLL in the same way that Explorer loads it.
// It does not touch the registry. Thus it does not need administrator rights.
//
// Usage: leclaude_smoke_menu [expected-project-folder]
// With the optional argument, the test also makes sure that the menu gets
// the two menu commands for that folder. The continuous-integration build
// does not give the argument, because the build machine has no Claude Code
// data.

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

// {F00FE5BC-E333-4E6A-A271-817BA795CFEA}
static constexpr CLSID kMenuClsid =
    { 0xF00FE5BC, 0xE333, 0x4E6A, { 0xA2, 0x71, 0x81, 0x7B, 0xA7, 0x95, 0xCF, 0xEA } };

// The ID of the first menu text, as in src/resource.h.
static constexpr UINT kIdsMenuTerminal = 101;

// The languages of the string table, as in src/resources.rc.
static constexpr WORD kLanguages[] = {
    MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
    MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN),
    MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH),
    MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN),
    MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
};

using DllGetClassObjectProc = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);

// Assembles a DROPFILES block with the given paths, as Explorer does.
static HGLOBAL BuildDropList(const std::vector<std::wstring>& paths)
{
    size_t characters = 1; // the final terminator
    for (const std::wstring& path : paths)
    {
        characters += path.size() + 1;
    }
    const SIZE_T bytes = sizeof(DROPFILES) + characters * sizeof(wchar_t);
    HGLOBAL global = GlobalAlloc(GHND, bytes);
    if (global == nullptr)
    {
        return nullptr;
    }
    auto* drop = static_cast<DROPFILES*>(GlobalLock(global));
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    auto* cursor = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(drop) + sizeof(DROPFILES));
    for (const std::wstring& path : paths)
    {
        std::memcpy(cursor, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
        cursor += path.size() + 1;
    }
    *cursor = L'\0';
    GlobalUnlock(global);
    return global;
}

// A minimal data object with only CF_HDROP. The handler reads the
// selection from it, as it does from the Explorer data object.
class TestDataObject final : public IDataObject
{
public:
    explicit TestDataObject(std::vector<std::wstring> paths) : m_paths(std::move(paths)) {}

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override
    {
        if (result == nullptr)
        {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDataObject)
        {
            *result = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *result = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_refCount);
    }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    // IDataObject
    IFACEMETHODIMP GetData(FORMATETC* format, STGMEDIUM* medium) override
    {
        if (format == nullptr || medium == nullptr)
        {
            return E_POINTER;
        }
        if (format->cfFormat != CF_HDROP || (format->tymed & TYMED_HGLOBAL) == 0)
        {
            return DV_E_FORMATETC;
        }
        // The handler releases the medium. Thus each call gives a new copy.
        HGLOBAL global = BuildDropList(m_paths);
        if (global == nullptr)
        {
            return E_OUTOFMEMORY;
        }
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = global;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }
    IFACEMETHODIMP GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    IFACEMETHODIMP QueryGetData(FORMATETC*) override { return E_NOTIMPL; }
    IFACEMETHODIMP GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override { return E_NOTIMPL; }
    IFACEMETHODIMP SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    IFACEMETHODIMP EnumFormatEtc(DWORD, IEnumFORMATETC**) override { return E_NOTIMPL; }
    IFACEMETHODIMP DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return E_NOTIMPL; }
    IFACEMETHODIMP DUnadvise(DWORD) override { return E_NOTIMPL; }
    IFACEMETHODIMP EnumDAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }

private:
    ~TestDataObject() = default;
    volatile LONG m_refCount = 1;
    std::vector<std::wstring> m_paths;
};

// Makes a new handler instance with both interfaces. The caller releases both.
static bool CreateHandler(IClassFactory* factory, IShellExtInit** init, IContextMenu** menu)
{
    *init = nullptr;
    *menu = nullptr;
    if (FAILED(factory->CreateInstance(nullptr, IID_IShellExtInit,
                                       reinterpret_cast<void**>(init))))
    {
        return false;
    }
    if (FAILED((*init)->QueryInterface(IID_IContextMenu, reinterpret_cast<void**>(menu))))
    {
        (*init)->Release();
        *init = nullptr;
        return false;
    }
    return true;
}

// Counts the menu commands after QueryContextMenu on a new menu.
static int QueryCommandCount(IContextMenu* menu, UINT flags, HRESULT* resultCode)
{
    HMENU popup = CreatePopupMenu();
    const HRESULT hr = menu->QueryContextMenu(popup, 0, 1, 0x7FFF, flags);
    *resultCode = hr;
    const int count = GetMenuItemCount(popup);
    DestroyMenu(popup);
    return count;
}

static BOOL CALLBACK CollectLanguage(HMODULE, LPCWSTR, LPCWSTR, WORD language, LONG_PTR param)
{
    reinterpret_cast<std::vector<WORD>*>(param)->push_back(language);
    return TRUE;
}

int wmain(int argc, wchar_t** argv)
{
    // ILCreateFromPathW needs COM.
    CHECK(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));

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
    if (getClassObject == nullptr)
    {
        return 1;
    }

    IClassFactory* factory = nullptr;
    CHECK(SUCCEEDED(getClassObject(kMenuClsid, IID_IClassFactory,
                                   reinterpret_cast<void**>(&factory))));
    if (factory == nullptr)
    {
        return 1;
    }

    // The string table must have all six language blocks. The strings 96 to
    // 111 are in the resource bundle 101 / 16 + 1.
    std::vector<WORD> found;
    const UINT bundle = kIdsMenuTerminal / 16 + 1;
    EnumResourceLanguagesW(dll, RT_STRING, MAKEINTRESOURCEW(bundle), CollectLanguage,
                           reinterpret_cast<LONG_PTR>(&found));
    for (const WORD language : kLanguages)
    {
        bool present = false;
        for (const WORD item : found)
        {
            if (item == language)
            {
                present = true;
            }
        }
        CHECK(present);
        if (!present)
        {
            std::fprintf(stderr, "The language 0x%04x has no string table.\n", language);
        }
    }

    // The temporary folder has no session history. The path has no final
    // backslash, because Explorer gives paths without one.
    wchar_t tempPathBuffer[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPathBuffer);
    std::wstring tempPath(tempPathBuffer);
    if (!tempPath.empty() && tempPath.back() == L'\\')
    {
        tempPath.pop_back();
    }

    // The background case: the pidl names the folder, the data object is null.
    IShellExtInit* init = nullptr;
    IContextMenu* menu = nullptr;
    CHECK(CreateHandler(factory, &init, &menu));
    if (menu != nullptr)
    {
        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(tempPath.c_str());
        CHECK(pidl != nullptr);
        CHECK(init->Initialize(pidl, nullptr, nullptr) == S_OK);
        ILFree(pidl);

        HRESULT hr = S_OK;
        CHECK(QueryCommandCount(menu, CMF_NORMAL, &hr) == 0);
        CHECK(SUCCEEDED(hr));
        CHECK(HRESULT_CODE(hr) == 0);

        // Without a project folder, an invocation must fail.
        CMINVOKECOMMANDINFO invoke = {};
        invoke.cbSize = sizeof(invoke);
        invoke.lpVerb = "leclaude_terminal";
        CHECK(FAILED(menu->InvokeCommand(&invoke)));

        menu->Release();
        init->Release();
    }

    // The selection case with one folder that is not a project folder.
    CHECK(CreateHandler(factory, &init, &menu));
    if (menu != nullptr)
    {
        TestDataObject* data = new TestDataObject({ tempPath });
        CHECK(init->Initialize(nullptr, data, nullptr) == S_OK);
        data->Release();

        HRESULT hr = S_OK;
        CHECK(QueryCommandCount(menu, CMF_NORMAL, &hr) == 0);
        CHECK(HRESULT_CODE(hr) == 0);

        menu->Release();
        init->Release();
    }

    // The selection case with two folders. The multi-selection rule hides
    // the menu commands, also on project folders.
    CHECK(CreateHandler(factory, &init, &menu));
    if (menu != nullptr)
    {
        TestDataObject* data = new TestDataObject({ tempPath, L"C:\\Windows" });
        CHECK(init->Initialize(nullptr, data, nullptr) == S_OK);
        data->Release();

        HRESULT hr = S_OK;
        CHECK(QueryCommandCount(menu, CMF_NORMAL, &hr) == 0);
        CHECK(HRESULT_CODE(hr) == 0);

        menu->Release();
        init->Release();
    }

    if (argc > 1)
    {
        // The given folder must get the two menu commands.
        CHECK(CreateHandler(factory, &init, &menu));
        if (menu != nullptr)
        {
            TestDataObject* data = new TestDataObject({ argv[1] });
            CHECK(init->Initialize(nullptr, data, nullptr) == S_OK);
            data->Release();

            HMENU popup = CreatePopupMenu();
            const HRESULT hr = menu->QueryContextMenu(popup, 0, 1, 0x7FFF, CMF_NORMAL);
            CHECK(SUCCEEDED(hr));
            CHECK(HRESULT_CODE(hr) == 2);
            CHECK(GetMenuItemCount(popup) == 2);
            CHECK(GetMenuItemID(popup, 0) == 1);
            CHECK(GetMenuItemID(popup, 1) == 2);

            // Each menu command must show the robot icon.
            MENUITEMINFOW item = {};
            item.cbSize = sizeof(item);
            item.fMask = MIIM_BITMAP;
            CHECK(GetMenuItemInfoW(popup, 0, TRUE, &item));
            CHECK(item.hbmpItem != nullptr);
            CHECK(GetMenuItemInfoW(popup, 1, TRUE, &item));
            CHECK(item.hbmpItem != nullptr);
            DestroyMenu(popup);

            // With CMF_DEFAULTONLY, the handler must not change the menu.
            HRESULT defaultOnly = S_OK;
            CHECK(QueryCommandCount(menu, CMF_DEFAULTONLY, &defaultOnly) == 0);
            CHECK(HRESULT_CODE(defaultOnly) == 0);

            // The verbs must come back through GetCommandString.
            wchar_t verb[64] = {};
            CHECK(menu->GetCommandString(0, GCS_VERBW, nullptr,
                                         reinterpret_cast<CHAR*>(verb), 64) == S_OK);
            CHECK(wcscmp(verb, L"leclaude_terminal") == 0);
            CHECK(menu->GetCommandString(1, GCS_VERBW, nullptr,
                                         reinterpret_cast<CHAR*>(verb), 64) == S_OK);
            CHECK(wcscmp(verb, L"leclaude_powershell") == 0);

            // An unknown verb must fail. The test must not start a program.
            CMINVOKECOMMANDINFO invoke = {};
            invoke.cbSize = sizeof(invoke);
            invoke.lpVerb = "leclaude_unknown";
            CHECK(FAILED(menu->InvokeCommand(&invoke)));

            menu->Release();
            init->Release();
        }
    }

    factory->Release();
    CoUninitialize();

    if (g_failures == 0)
    {
        std::printf("All checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d checks failed.\n", g_failures);
    return 1;
}
