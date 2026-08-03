// Leclaude - the shared module state.

#pragma once

#include <windows.h>

extern HINSTANCE g_module;
extern volatile LONG g_objectCount;

// The CLSID of the Leclaude overlay handler.
// {AEB0D999-FC8F-4FFA-B160-D4506164F0E7}
inline constexpr CLSID CLSID_LeclaudeOverlay =
    { 0xAEB0D999, 0xFC8F, 0x4FFA, { 0xB1, 0x60, 0xD4, 0x50, 0x61, 0x64, 0xF0, 0xE7 } };

inline constexpr wchar_t kClsidString[] = L"{AEB0D999-FC8F-4FFA-B160-D4506164F0E7}";

// The subkey name starts with one space. Windows loads the first 15 overlay names in
// alphabetical order, and the space moves the name to the front.
inline constexpr wchar_t kOverlayKeyName[] = L" Leclaude";

inline constexpr wchar_t kHandlerDescription[] = L"Leclaude overlay handler";
