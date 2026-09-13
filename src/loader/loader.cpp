// loader.cpp - injects dumper.dll (or any dll) into nmrih.exe via CreateRemoteThread
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

static DWORD FindProcess(const char* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32First(snap, &pe))
    {
        do
        {
            if (_stricmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static HMODULE FindRemoteModule(DWORD pid, const char* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return NULL;
    MODULEENTRY32 me; me.dwSize = sizeof(me);
    HMODULE result = NULL;
    if (Module32First(snap, &me))
    {
        do
        {
            if (_stricmp(me.szModule, name) == 0) { result = me.hModule; break; }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    return result;
}

static bool Inject(DWORD pid, const char* dllPath)
{
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { printf("OpenProcess failed: %lu\n", GetLastError()); return false; }
    size_t len = strlen(dllPath) + 1;
    void* remote = VirtualAllocEx(proc, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { printf("VirtualAllocEx failed: %lu\n", GetLastError()); CloseHandle(proc); return false; }
    if (!WriteProcessMemory(proc, remote, dllPath, len, NULL))
    {
        printf("WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE remoteK32 = FindRemoteModule(pid, "kernel32.dll");
    FARPROC localLoadLib = GetProcAddress(k32, "LoadLibraryA");
    LPTHREAD_START_ROUTINE remoteLoadLib = remoteK32 && localLoadLib
        ? (LPTHREAD_START_ROUTINE)((BYTE*)remoteK32 + ((BYTE*)localLoadLib - (BYTE*)k32))
        : NULL;
    if (!remoteLoadLib)
    {
        printf("Could not resolve remote LoadLibraryA\n");
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    HANDLE thread = CreateRemoteThread(proc, NULL, 0, remoteLoadLib, remote, 0, NULL);
    if (!thread)
    {
        printf("CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    DWORD wait = WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    bool loaded = wait == WAIT_OBJECT_0 && GetExitCodeThread(thread, &exitCode) && exitCode != 0;
    CloseHandle(thread);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    if (!loaded) printf("Remote LoadLibraryA failed or timed out\n");
    return loaded;
}

int main(int argc, char** argv)
{
    const char* exe = "nmrih_win64.exe";
    static char defaultPath[MAX_PATH] = "NMRIHCheat.dll";
    if (argc <= 1)
    {
        GetModuleFileNameA(NULL, defaultPath, sizeof(defaultPath));
        char* slash = strrchr(defaultPath, '\\');
        if (slash)
        {
            slash[1] = 0;
            strncat(defaultPath, "NMRIHCheat.dll", sizeof(defaultPath) - strlen(defaultPath) - 1);
        }
    }
    const char* dll = argc > 1 ? argv[1] : defaultPath;
    DWORD pid = FindProcess(exe);
    int result = 0;
    if (!pid)
    {
        printf("nmrih_win64.exe not running\n");
        result = 1;
    }
    else
    {
        printf("Found nmrih.exe (pid %lu), injecting %s...\n", pid, dll);
        if (Inject(pid, dll)) printf("Injected OK\n");
        else { printf("Inject FAILED\n"); result = 1; }
    }

    printf("Press Enter to close...\n");
    getchar();
    return result;
}
