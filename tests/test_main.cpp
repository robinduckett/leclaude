// Leclaude - the tests for the scan functions.
// The tests use no framework. The program returns 0 when all the checks pass.

#include "project_scan.h"

#include <windows.h>

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

namespace
{

void WriteTextFile(const std::wstring& path, const char* text)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(file != INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(file, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
        CloseHandle(file);
    }
}

void RemoveTree(const std::wstring& path)
{
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((path + L"\\*").c_str(), &data);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
            {
                continue;
            }
            const std::wstring child = path + L"\\" + data.cFileName;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                RemoveTree(child);
            }
            else
            {
                DeleteFileW(child.c_str());
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    RemoveDirectoryW(path.c_str());
}

void TestEncode()
{
    // These pairs come from a real system. See docs/claude-data-format.md.
    CHECK(EncodeProjectPath(L"L:\\Projects\\Leclaude") == L"L--PROJECTS-LECLAUDE");
    CHECK(EncodeProjectPath(L"E:/Godot Projects/probe") == L"E--GODOT-PROJECTS-PROBE");
    CHECK(EncodeProjectPath(L"C:/Users/robin/.claude") == L"C--USERS-ROBIN--CLAUDE");

    // A forward slash and a backslash give the same result.
    CHECK(EncodeProjectPath(L"L:/Projects/Leclaude") == EncodeProjectPath(L"L:\\Projects\\Leclaude"));

    // Two case variants of one path give the same result.
    CHECK(EncodeProjectPath(L"l:\\projects\\c2") == EncodeProjectPath(L"L:\\Projects\\C2"));

    // A letter with an accent becomes a hyphen. The rule is ASCII-only.
    CHECK(EncodeProjectPath(L"C:\\caf\u00e9") == L"C--CAF-");

    // A drive root.
    CHECK(EncodeProjectPath(L"L:\\") == L"L--");
}

void TestDetection()
{
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    const std::wstring root =
        std::wstring(tempPath) + L"leclaude-test-" + std::to_wstring(GetCurrentProcessId());
    RemoveTree(root);
    CHECK(CreateDirectoryW(root.c_str(), nullptr));

    // Folder A has a transcript file. It counts.
    const std::wstring folderA = root + L"\\A--WITH-JSONL";
    CreateDirectoryW(folderA.c_str(), nullptr);
    WriteTextFile(folderA + L"\\0362acb0.jsonl", "{\"type\":\"mode\"}\n");

    // Folder B has a sessions index with one entry. It counts.
    const std::wstring folderB = root + L"\\B--WITH-INDEX";
    CreateDirectoryW(folderB.c_str(), nullptr);
    WriteTextFile(folderB + L"\\sessions-index.json",
                  "{\"version\":1,\"entries\":[{\"sessionId\":\"abc\"}]}");

    // Folder C has a sessions index without entries. It does not count.
    const std::wstring folderC = root + L"\\C--EMPTY-INDEX";
    CreateDirectoryW(folderC.c_str(), nullptr);
    WriteTextFile(folderC + L"\\sessions-index.json", "{\"version\":1,\"entries\":[]}");

    // Folder D has only a memory subfolder. It does not count.
    const std::wstring folderD = root + L"\\D--ONLY-MEMORY";
    CreateDirectoryW(folderD.c_str(), nullptr);
    CreateDirectoryW((folderD + L"\\memory").c_str(), nullptr);

    // Folder E is empty. It does not count.
    const std::wstring folderE = root + L"\\E--EMPTY";
    CreateDirectoryW(folderE.c_str(), nullptr);

    CHECK(DirectoryHasSessionHistory(folderA));
    CHECK(DirectoryHasSessionHistory(folderB));
    CHECK(!DirectoryHasSessionHistory(folderC));
    CHECK(!DirectoryHasSessionHistory(folderD));
    CHECK(!DirectoryHasSessionHistory(folderE));

    const auto set = BuildProjectSet(root);
    CHECK(set.size() == 2);
    CHECK(set.count(L"A--WITH-JSONL") == 1);
    CHECK(set.count(L"B--WITH-INDEX") == 1);
    CHECK(set.count(L"C--EMPTY-INDEX") == 0);

    // A root that does not exist gives an empty set. It must not give an error.
    CHECK(BuildProjectSet(root + L"\\does-not-exist").empty());
    CHECK(BuildProjectSet(L"").empty());

    RemoveTree(root);
}

} // namespace

int main()
{
    TestEncode();
    TestDetection();
    if (g_failures == 0)
    {
        std::printf("All checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d checks failed.\n", g_failures);
    return 1;
}
