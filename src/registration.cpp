// Leclaude - the self-registration code. Regsvr32 calls these functions
// through DllRegisterServer and DllUnregisterServer.

#include "module.h"

#include <olectl.h>

#include <string>

namespace
{

constexpr wchar_t kClsidKeyPath[] =
    L"Software\\Classes\\CLSID\\{AEB0D999-FC8F-4FFA-B160-D4506164F0E7}";
constexpr wchar_t kOverlayIdentifiersPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers";
constexpr wchar_t kApprovedPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

LSTATUS SetKeyValue(HKEY root, const std::wstring& keyPath, const wchar_t* valueName,
                    const std::wstring& data)
{
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(root, keyPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                                     KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS)
    {
        return status;
    }
    const DWORD size = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));
    status = RegSetValueExW(key, valueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(data.c_str()), size);
    RegCloseKey(key);
    return status;
}

} // namespace

HRESULT RegisterServer()
{
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(g_module, modulePath, MAX_PATH) == 0)
    {
        return SELFREG_E_CLASS;
    }

    const std::wstring inprocPath = std::wstring(kClsidKeyPath) + L"\\InProcServer32";
    const std::wstring overlayKey = std::wstring(kOverlayIdentifiersPath) + L"\\" + kOverlayKeyName;

    if (SetKeyValue(HKEY_LOCAL_MACHINE, kClsidKeyPath, nullptr, kHandlerDescription) != ERROR_SUCCESS ||
        SetKeyValue(HKEY_LOCAL_MACHINE, inprocPath, nullptr, modulePath) != ERROR_SUCCESS ||
        SetKeyValue(HKEY_LOCAL_MACHINE, inprocPath, L"ThreadingModel", L"Apartment") != ERROR_SUCCESS ||
        SetKeyValue(HKEY_LOCAL_MACHINE, overlayKey, nullptr, kClsidString) != ERROR_SUCCESS ||
        SetKeyValue(HKEY_LOCAL_MACHINE, kApprovedPath, kClsidString, kHandlerDescription) != ERROR_SUCCESS)
    {
        return SELFREG_E_CLASS;
    }
    return S_OK;
}

HRESULT UnregisterServer()
{
    const std::wstring overlayKey = std::wstring(kOverlayIdentifiersPath) + L"\\" + kOverlayKeyName;

    // A missing key is not an error. The removal must complete on a partial installation.
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, overlayKey.c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, kClsidKeyPath);

    HKEY approved = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kApprovedPath, 0, KEY_SET_VALUE, &approved) ==
        ERROR_SUCCESS)
    {
        RegDeleteValueW(approved, kClsidString);
        RegCloseKey(approved);
    }
    return S_OK;
}
