#include "globals.h"

Cfg g_cfg;

void* g_engine = nullptr;
void* g_entlist = nullptr;

bool g_menuOpen = false;
bool g_imguiReady = false;
static volatile bool g_unloading = false;
static LONG g_inEndScene = 0;
static HANDLE g_watchStop = nullptr;
static HANDLE g_watchThread = nullptr;

// ---------- logging ----------
static char g_logPath[MAX_PATH] = {};

static const char* LogPath()
{
    if (!g_logPath[0])
    {
        HMODULE self = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)LogPath, &self))
        {
            GetModuleFileNameA(self, g_logPath, sizeof(g_logPath));
            char* slash = strrchr(g_logPath, '\\');
            if (slash)
            {
                slash[1] = 0;
                strncat(g_logPath, "cheat_log.txt", sizeof(g_logPath) - strlen(g_logPath) - 1);
            }
        }
    }
    return g_logPath;
}

void CheatLog(const char* fmt, ...)
{
    FILE* f = fopen(LogPath(), "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

// ---------- interface resolution ----------
typedef void* (*CreateInterfaceFn)(const char*, int*);
static void* GetInterface(HMODULE mod, const char* name)
{
    if (!mod) return nullptr;
    CreateInterfaceFn fn = (CreateInterfaceFn)GetProcAddress(mod, "CreateInterface");
    if (!fn) return nullptr;
    return fn(name, nullptr);
}

// ---------- hooks ----------
int g_capturing = 0;

typedef HRESULT(__stdcall* EndSceneFn)(IDirect3DDevice9*);
typedef HRESULT(__stdcall* ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
WNDPROC g_oWndProc = nullptr;
HWND g_gameHwnd = nullptr;
static IDirect3DDevice9* g_hookedDev = nullptr;
static void RestoreDeviceVt();

// ---------- world-to-screen ----------
// The engine vtable's slots 37 and 36 are the matrix getter candidates. The
// matrix is captured once per frame and then used for every entity, avoiding
// both the old angle/FOV approximation and repeated engine calls.
static const float* g_projectionMatrix = nullptr;
static float g_projectionWidth = 0.0f;
static float g_projectionHeight = 0.0f;
static int g_projectionIndex = -1;

static bool ProjectionMatrixReadable(const float* matrix)
{
    if (!matrix) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(matrix, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    __try
    {
        bool nonZero = false;
        for (int i = 0; i < 16; i++)
        {
            if (!_finite(matrix[i])) return false;
            if (fabsf(matrix[i]) > 0.0001f) nonZero = true;
        }
        return nonZero;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void PrepareProjection()
{
    g_projectionMatrix = nullptr;
    g_projectionWidth = 0.0f;
    g_projectionHeight = 0.0f;
    if (!g_engine) return;

    ImGuiIO& io = ImGui::GetIO();
    g_projectionWidth = io.DisplaySize.x;
    g_projectionHeight = io.DisplaySize.y;
    if (g_projectionWidth < 1.0f || g_projectionHeight < 1.0f) return;

    // Source SDK 2013 IVEngineClient013: WorldToScreenMatrix=36,
    // WorldToViewMatrix=37. Never fall back to the view matrix: it has a
    // different coordinate space and would recreate the old misalignment.
    int indices[1] = { 36 };
    for (int n = 0; n < 1; n++)
    {
        int index = indices[n];
        if (index < 0) continue;
        __try
        {
            typedef const float* (__thiscall* GetMatrixFn)(void*);
            const float* matrix = ((GetMatrixFn)(*(void***)g_engine)[index])(g_engine);
            if (!ProjectionMatrixReadable(matrix)) continue;
            g_projectionIndex = index;
            g_projectionMatrix = matrix;
            static bool logged = false;
            if (!logged)
            {
                logged = true;
                CheatLog("projection matrix: engine vt[%d] = %p\n", index, matrix);
            }
            return;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
}

bool WorldToScreen(const float* pos, float& sx, float& sy)
{
    if (!g_projectionMatrix || g_projectionWidth < 1.0f || g_projectionHeight < 1.0f) return false;
    const float* m = g_projectionMatrix;
    float clipX = m[0] * pos[0] + m[1] * pos[1] + m[2] * pos[2] + m[3];
    float clipY = m[4] * pos[0] + m[5] * pos[1] + m[6] * pos[2] + m[7];
    float clipW = m[12] * pos[0] + m[13] * pos[1] + m[14] * pos[2] + m[15];
    if (!_finite(clipX) || !_finite(clipY) || !_finite(clipW) || clipW <= 0.001f) return false;
    sx = (g_projectionWidth * 0.5f) * (1.0f + clipX / clipW);
    sy = (g_projectionHeight * 0.5f) * (1.0f - clipY / clipW);
    return _finite(sx) && _finite(sy) && sx > -2000.0f && sx < g_projectionWidth + 2000.0f &&
        sy > -2000.0f && sy < g_projectionHeight + 2000.0f;
}

// ---------- entity access mode (probed at runtime) ----------
int g_entSlot = 1;       // entlist vtable slot (0 or 1)
int g_entBaseDelta = 0;  // 0 if slot returns base, -8 if it returns base+8
int g_ccSubOffset = 8;   // 8 = GetClientClass on entity+8, 0 = on entity itself

static const char* RawClassName(void* sub)
{
    __try
    {
        void** vt = *(void***)sub;
        if (!vt) return nullptr;
        // Only call vt[2] if it looks like a zero-arg thiscall (ends with plain
        // ret before any ret N). Non-entity objects in the entity list (teams,
        // vote controllers...) have arg-taking vt[2] functions - calling those
        // corrupts the stack (ret N) and crashes/freezes the game.
        unsigned char* fn = (unsigned char*)vt[2];
        bool safe = false;
        for (int i = 0; i < 96; i++)
        {
            if (fn[i] == 0xC3) { safe = true; break; }
            if (fn[i] == 0xC2) break;
        }
        if (!safe) return nullptr;
        typedef void* (__thiscall* tGetCC)(void*);
        void* cls = ((tGetCC)vt[2])(sub);
        if (!cls) return nullptr;
        // ClientClass contains two function pointers before m_pNetworkName.
        // Pointer-sized arithmetic keeps this valid in both 32- and 64-bit builds.
        return *(const char**)((unsigned char*)cls + sizeof(void*) * 2);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// Try entlist slots {1,0} x class-subobject offsets {pointer size,0}; pick the combo that
// yields "CNMRiH_Player" for the local player. All calls are zero-arg -> safe.
static void ProbeEntityMode()
{
    static bool done = false;
    if (done) return;
    if (!g_entlist) return;
    int localIdx = eng_GetLocalPlayer();
    if (localIdx <= 0) return;

#ifdef _WIN64
    g_entSlot = 3;
    g_entBaseDelta = 0;
    g_ccSubOffset = 16;
    void* entity = ent_GetEntity(localIdx);
    char x64Name[64];
    if (entity && SafeClassName(RawClassName((unsigned char*)entity + g_ccSubOffset), x64Name, 64) &&
        strcmp(x64Name, "CNMRiH_Player") == 0)
    {
        done = true;
        CheatLog("entity mode x64: slot=3 delta=0 ccsub=16 (local %d -> %s)\n", localIdx, x64Name);
    }
    return;
#endif

    typedef void* (__thiscall* tGetEntityRaw)(void*, int);
    void** vt = *(void***)g_entlist;

    void* results[2] = { nullptr, nullptr };
    __try
    {
        results[1] = ((tGetEntityRaw)vt[1])(g_entlist, localIdx);
        results[0] = ((tGetEntityRaw)vt[0])(g_entlist, localIdx);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }

    for (int slot = 1; slot >= 0; slot--)
    {
        if (!results[slot]) continue;
        for (int sub = (int)sizeof(void*); sub >= 0; sub -= (int)sizeof(void*))
        {
            char name[64];
            if (!SafeClassName(RawClassName((void*)((unsigned char*)results[slot] + sub)), name, 64)) continue;
            if (strcmp(name, "CNMRiH_Player") == 0)
            {
                g_entSlot = slot;
                g_entBaseDelta = (sub == 8) ? 0 : -8;
                g_ccSubOffset = sub - g_entBaseDelta; // subobject offset relative to the BASE
                done = true;
                CheatLog("entity mode: slot=%d sub=%d delta=%d ccsub=%d (local %d -> %s)\n",
                    slot, sub, g_entBaseDelta, g_ccSubOffset, localIdx, name);
                return;
            }
        }
    }
}

const char* EntClassName(void* e)
{
    if (!e) return nullptr;
    return RawClassName((void*)((unsigned char*)e + g_ccSubOffset));
}

// copy a class name safely (bounded, printable-only, SEH-guarded)
bool SafeClassName(const char* src, char* out, int outLen)
{
    out[0] = 0;
    if (!src) return false;
    __try
    {
        for (int i = 0; i < outLen - 1; i++)
        {
            char c = src[i];
            if (!c) { out[i] = 0; return true; }
            if (c < 32 || c > 126) return false;
            out[i] = c;
        }
        out[outLen - 1] = 0;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; return false; }
}

float NormYaw(float yaw)
{
    if (!_finite(yaw)) return 0.0f;
    yaw = fmodf(yaw, 360.0f);
    if (yaw > 180.0f) yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
    return yaw;
}

// ---------- FrameStageNotify-era tick: now driven by EndScene (proven firing) ----------
// lazy init: retried every frame until the game modules are up
static void EnsureGameInit()
{
    if (g_engine && g_entlist) return;
    HMODULE engine = GetModuleHandleA("engine.dll");
    HMODULE client = GetModuleHandleA("client.dll");
    if (engine)
    {
        if (!g_engine)
        {
            g_engine = GetInterface(engine, "VEngineClient013");
            if (!g_engine) g_engine = GetInterface(engine, "VEngineClient014");
            CheatLog("engine iface = %p\n", g_engine);
        }
    }
    if (client)
    {
        if (!g_entlist) g_entlist = GetInterface(client, "VClientEntityList003");
    }
}

// ---------- D3D9 hooks ----------
static EndSceneFn g_oEndScene2 = nullptr;
static ResetFn g_oReset2 = nullptr;
static void** g_hookedVt = nullptr;
static LARGE_INTEGER g_lastRender = {};
static LARGE_INTEGER g_freq = {};
static HRESULT hkEndSceneBody(IDirect3DDevice9* dev, EndSceneFn orig);

static HRESULT __stdcall hkReset2(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    if (g_imguiReady) ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT r = g_oReset2 ? g_oReset2(dev, pp) : D3DERR_INVALIDCALL;
    if (r == D3D_OK && g_imguiReady) ImGui_ImplDX9_CreateDeviceObjects();
    return r;
}

static HRESULT __stdcall hkEndScene2(IDirect3DDevice9* dev)
{
    InterlockedIncrement(&g_inEndScene);
    if (g_unloading)
    {
        EndSceneFn original = g_oEndScene2;
        HRESULT result = original ? original(dev) : D3D_OK;
        InterlockedDecrement(&g_inEndScene);
        return result;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!g_freq.QuadPart) QueryPerformanceFrequency(&g_freq);
    if (g_lastRender.QuadPart &&
        (double)(now.QuadPart - g_lastRender.QuadPart) / (double)g_freq.QuadPart < 0.001)
    {
        EndSceneFn original = g_oEndScene2;
        HRESULT result = original ? original(dev) : D3D_OK;
        InterlockedDecrement(&g_inEndScene);
        return result;
    }
    g_lastRender = now;
    EndSceneFn original = g_oEndScene2;
    HRESULT result = original ? hkEndSceneBody(dev, original) : D3D_OK;
    InterlockedDecrement(&g_inEndScene);
    return result;
}

static HRESULT hkEndSceneBody(IDirect3DDevice9* dev, EndSceneFn orig)
{
    static bool logged = false;
    if (!logged) { CheatLog("first EndScene fired (dev=%p)\n", dev); logged = true; }

    EnsureGameInit();

    if (!g_imguiReady) InitImGui(dev);
    if (!g_imguiReady) return orig ? orig(dev) : D3DERR_INVALIDCALL;

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    PrepareProjection();

    static int fc = 0;
    DWORD t0 = GetTickCount();

    // game-facing work on the render thread only - off-thread engine calls
    // can deadlock the game's main thread
#ifdef _WIN64
    // Current x64 offsets and entity layout are verified. This stage performs
    // read-only entity collection; memory-writing features remain disabled.
    __try
    {
        ProbeEntityMode();
        GameplayTick();
        EspCollect();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
#else
    __try
    {
        ProbeEntityMode();
        GameplayTick();
        EspCollect();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
#endif
    DWORD t1 = GetTickCount();

    BrightDraw();
    EspDraw();
    FovDraw();
    HudDraw();
    MenuRender();
    DWORD t2 = GetTickCount();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    DWORD t3 = GetTickCount();

    HRESULT r = orig ? orig(dev) : D3DERR_INVALIDCALL;
    DWORD t4 = GetTickCount();

    fc++;
    if ((fc % 60) == 0)
        CheatLog("frame %d: collect=%dms draw/menu=%dms imgui=%dms orig=%dms\n",
            fc, t1 - t0, t2 - t1, t3 - t2, t4 - t3);

    return r;
}

static DWORD WINAPI UnloadThread(LPVOID param)
{
    if (g_watchStop) SetEvent(g_watchStop);
    if (g_watchThread && g_watchThread != GetCurrentThread())
        WaitForSingleObject(g_watchThread, 3000);
    while (InterlockedCompareExchange(&g_inEndScene, 0, 0) != 0)
        Sleep(1);

    RestoreDeviceVt();
    if (g_gameHwnd && g_oWndProc)
        SetWindowLongPtrA(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)g_oWndProc);

    if (g_imguiReady)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiReady = false;
    }
    if (g_watchThread) { CloseHandle(g_watchThread); g_watchThread = nullptr; }
    if (g_watchStop) { CloseHandle(g_watchStop); g_watchStop = nullptr; }
    FreeLibraryAndExitThread((HMODULE)param, 0);
    return 0;
}

void RestoreAndUnload()
{
    static bool unloading = false;
    if (unloading) return;
    unloading = true;
    g_unloading = true; // stop the device watcher thread

    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)RestoreAndUnload, &self);
    if (self) CreateThread(nullptr, 0, UnloadThread, self, 0, nullptr);
}

static LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN && wp == VK_END)
    {
        RestoreAndUnload();
        return 0;
    }

    if (g_capturing && msg == WM_KEYDOWN)
    {
        if (wp == VK_ESCAPE)
        {
            g_capturing = 0; // cancel
        }
        else
        {
            if (g_capturing == 1) g_cfg.aimbot.key = (int)wp;
            else if (g_capturing == 2) g_cfg.menuKey = (int)wp;
            g_capturing = 0;
        }
        return 0;
    }

    if (g_capturing &&
        (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN))
    {
        int vk = VK_LBUTTON;
        if (msg == WM_RBUTTONDOWN) vk = VK_RBUTTON;
        else if (msg == WM_MBUTTONDOWN) vk = VK_MBUTTON;
        else if (msg == WM_XBUTTONDOWN) vk = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
        if (g_capturing == 1) g_cfg.aimbot.key = vk;
        else if (g_capturing == 2) g_cfg.menuKey = vk;
        g_capturing = 0;
        return 0;
    }

    if (msg == WM_KEYDOWN && (int)wp == g_cfg.menuKey)
        g_menuOpen = !g_menuOpen;

    if (g_menuOpen)
    {
        if (g_imguiReady)
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
        // only eat input ImGui actually needs - an invisible/failed menu
        // can never lock the game out
        if (g_imguiReady)
        {
            ImGuiIO& io = ImGui::GetIO();
            if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && io.WantCaptureMouse) return 0;
            if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR) && io.WantCaptureKeyboard) return 0;
        }
    }

    return g_oWndProc ? CallWindowProcA(g_oWndProc, hwnd, msg, wp, lp) : DefWindowProcA(hwnd, msg, wp, lp);
}

const char* VkName(int vk)
{
    switch (vk)
    {
    case VK_MENU: return "ALT";
    case VK_LMENU: return "L-ALT";
    case VK_RMENU: return "R-ALT";
    case VK_SHIFT: return "SHIFT";
    case VK_LSHIFT: return "L-SHIFT";
    case VK_RSHIFT: return "R-SHIFT";
    case VK_CONTROL: return "CTRL";
    case VK_LCONTROL: return "L-CTRL";
    case VK_RCONTROL: return "R-CTRL";
    case VK_SPACE: return "SPACE";
    case VK_INSERT: return "INSERT";
    case VK_DELETE: return "DELETE";
    case VK_HOME: return "HOME";
    case VK_END: return "END";
    case VK_RBUTTON: return "RMB";
    case VK_MBUTTON: return "MMB";
    case VK_XBUTTON1: return "MB4";
    case VK_XBUTTON2: return "MB5";
    case VK_LBUTTON: return "LMB";
    case VK_CAPITAL: return "CAPS";
    case VK_TAB: return "TAB";
    default:
        if (vk >= 'A' && vk <= 'Z') { static char s[2]; s[0] = (char)vk; s[1] = 0; return s; }
        if (vk >= '0' && vk <= '9') { static char d[2]; d[0] = (char)vk; d[1] = 0; return d; }
        if (vk >= VK_F1 && vk <= VK_F12) { static char f[4]; _snprintf(f, 4, "F%d", vk - VK_F1 + 1); return f; }
        break;
    }
    return "?";
}

static BOOL CALLBACK FindGameWindowByPid(HWND hwnd, LPARAM lparam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd))
    {
        *(HWND*)lparam = hwnd;
        return FALSE;
    }
    return TRUE;
}

// ---------- init ----------
static IDirect3DDevice9* FindGameDeviceShadera();
static bool HookDeviceVt(IDirect3DDevice9* dev);

static void RestoreDeviceVt()
{
    if (!g_hookedVt) return;
    __try
    {
        DWORD old;
        if (g_hookedVt[42] == (void*)hkEndScene2)
        {
            if (!VirtualProtect(&g_hookedVt[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return;
            g_hookedVt[42] = g_oEndScene2;
            VirtualProtect(&g_hookedVt[42], sizeof(void*), old, &old);
        }
        if (g_hookedVt[16] == (void*)hkReset2)
        {
            if (!VirtualProtect(&g_hookedVt[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return;
            g_hookedVt[16] = g_oReset2;
            VirtualProtect(&g_hookedVt[16], sizeof(void*), old, &old);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    g_hookedVt = nullptr;
    g_hookedDev = nullptr;
}

static void TryHookCurrentDevice()
{
    IDirect3DDevice9* dev = FindGameDeviceShadera();
    if (!dev) return;
    void** vt = *(void***)dev;
    if (!vt || vt == g_hookedVt) { g_hookedDev = dev; return; }

    RestoreDeviceVt();
    if (vt[42] == (void*)hkEndScene2 || vt[16] == (void*)hkReset2)
    {
        // This vtable is already owned by this DLL; do not overwrite the
        // original pointers and create a recursive hook chain.
        g_hookedVt = vt;
        g_hookedDev = dev;
        return;
    }
    if (!HookDeviceVt(dev)) return;
    g_hookedDev = dev;
    CheatLog("device hooked (dev=%p, EndScene orig %p)\n", dev, g_oEndScene2);
}

static DWORD WINAPI DeviceWatchThread(LPVOID)
{
    for (;;)
    {
        if (WaitForSingleObject(g_watchStop, 2000) != WAIT_TIMEOUT) break;
        TryHookCurrentDevice();
    }
    return 0;
}

static void* PatternScan(HMODULE mod, const unsigned char* sig, const char* mask, size_t len)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD_PTR p = (DWORD_PTR)mod;
    DWORD_PTR end = p;
    while (VirtualQuery((void*)end, &mbi, sizeof(mbi)) &&
        mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE)
        end = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;

    for (; p + len < end; p++)
    {
        bool ok = true;
        for (size_t i = 0; i < len; i++)
            if (mask[i] == 'x' && *(unsigned char*)(p + i) != sig[i]) { ok = false; break; }
        if (ok) return (void*)p;
    }
    return nullptr;
}

static bool IsExecutableAddress(const void* address)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!address || !VirtualQuery(address, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
        return false;
    DWORD protect = mbi.Protect & 0xFF;
    return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

static bool LooksLikeD3D9Device(IDirect3DDevice9* dev)
{
    __try
    {
        if (!dev) return false;
        void** vt = *(void***)dev;
        HMODULE runtime = GetModuleHandleA("d3d9.dll");
        if (!vt || !runtime) return false;
        const int slots[] = { 0, 3, 6, 16, 42 };
        for (int slot : slots)
        {
            void* fn = vt[slot];
            if ((slot == 16 && fn == (void*)hkReset2) ||
                (slot == 42 && fn == (void*)hkEndScene2)) continue;
            MEMORY_BASIC_INFORMATION mbi;
            if (!IsExecutableAddress(fn) || !VirtualQuery(fn, &mbi, sizeof(mbi)) ||
                mbi.AllocationBase != runtime) return false;
        }
        // Only call COM after checking runtime ownership. Executable slots
        // alone also match unrelated shader interfaces.
        IDirect3DDevice9* verified = nullptr;
        HRESULT hr = dev->QueryInterface(__uuidof(IDirect3DDevice9), (void**)&verified);
        if (FAILED(hr) || !verified) return false;
        D3DDEVICE_CREATION_PARAMETERS params = {};
        hr = verified->GetCreationParameters(&params);
        bool same = verified == dev;
        verified->Release();
        DWORD pid = 0;
        if (SUCCEEDED(hr)) GetWindowThreadProcessId(params.hFocusWindow, &pid);
        return same && SUCCEEDED(hr) && pid == GetCurrentProcessId() &&
            params.hFocusWindow == g_gameHwnd;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static IDirect3DDevice9* FindGameDeviceByDataScan(HMODULE shader)
{
    DWORD_PTR cursor = (DWORD_PTR)shader;
    DWORD_PTR end = cursor;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)end, &mbi, sizeof(mbi)) &&
        mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE)
        end = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;

    // The device is stored in shaderapidx9's writable image data on current
    // x64 builds. Scan only those pages and validate the candidate before use;
    // unlike the previous instruction signature, this survives compiler codegen
    // changes and ASLR.
    while (cursor < end && VirtualQuery((void*)cursor, &mbi, sizeof(mbi)))
    {
        DWORD_PTR pageEnd = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
        if (pageEnd > end) pageEnd = end;
        DWORD protect = mbi.Protect & 0xFF;
        if (mbi.State == MEM_COMMIT &&
            (protect == PAGE_READWRITE || protect == PAGE_WRITECOPY || protect == PAGE_EXECUTE_READWRITE))
        {
            DWORD_PTR p = (cursor + sizeof(void*) - 1) & ~(DWORD_PTR)(sizeof(void*) - 1);
            for (; p + sizeof(void*) <= pageEnd; p += sizeof(void*))
            {
                __try
                {
                    IDirect3DDevice9* dev = *(IDirect3DDevice9**)p;
                    if (LooksLikeD3D9Device(dev))
                    {
                        CheatLog("device data candidate at %p -> %p\n", (void*)p, dev);
                        return dev;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { }
            }
        }
        cursor = pageEnd;
    }
    return nullptr;
}

static IDirect3DDevice9* FindGameDeviceShadera()
{
    static bool logged = false;
    HMODULE sh = GetModuleHandleA("shaderapidx9.dll");
    if (!sh) return nullptr;
    if (!logged) { CheatLog("shaderapidx9 = %p\n", sh); logged = true; }

#ifndef _WIN64
    const unsigned char sigs[3][12] = {
        { 0xA1, 0, 0, 0, 0, 0x50, 0x8B, 0x08, 0xFF, 0x51, 0x0C },
        { 0xA1, 0, 0, 0, 0, 0x50, 0x8B, 0x01, 0xFF, 0x50, 0x0C },
        { 0x8B, 0x0D, 0, 0, 0, 0, 0x8B, 0x01, 0xFF, 0x50, 0x0C },
    };
    const char* masks[3] = { "x????xxxx", "x????xxxx", "xx????xxxx" };

    for (int i = 0; i < 3; i++)
    {
        void* hit = PatternScan(sh, sigs[i], masks[i], 11);
        if (!hit) continue;
        static bool sigLogged = false;
        if (!sigLogged) { CheatLog("device sig %d hit at %p\n", i, hit); sigLogged = true; }
        IDirect3DDevice9** ppDev = *(IDirect3DDevice9***)((unsigned char*)hit + 1);
        if (IsBadReadPtr(ppDev, 4)) continue;
        IDirect3DDevice9* dev = *ppDev;
        if (IsBadReadPtr(dev, 4)) continue;
        void** vt = *(void***)dev;
        if (IsBadReadPtr(vt, 43 * 4)) continue;
        return dev;
    }
#endif
    return FindGameDeviceByDataScan(sh);
}

static bool HookDeviceVt(IDirect3DDevice9* dev)
{
    if (!dev) return false;
    void** vt = *(void***)dev;
    if (!vt) return false;
    g_oEndScene2 = (EndSceneFn)vt[42];
    g_oReset2 = (ResetFn)vt[16];
    DWORD old;
    if (!VirtualProtect(&vt[16], sizeof(void*) * 27, PAGE_EXECUTE_READWRITE, &old)) return false;
    vt[42] = (void*)hkEndScene2;
    vt[16] = (void*)hkReset2;
    VirtualProtect(&vt[16], sizeof(void*) * 27, old, &old);
    g_hookedVt = vt;
    return true;
}

static void InitD3D9Hook()
{
    // Hook only the actual game device. The old dummy-device path patched a
    // shared COM vtable without retaining it, which made unload unsafe.
    g_watchStop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    TryHookCurrentDevice();
    if (g_watchStop)
        g_watchThread = CreateThread(nullptr, 0, DeviceWatchThread, nullptr, 0, nullptr);
}

static void InitGameHooks()
{
    EnsureGameInit();

    // window + input
    g_gameHwnd = FindWindowA("Valve001", nullptr);
    if (!g_gameHwnd) EnumWindows(FindGameWindowByPid, (LPARAM)&g_gameHwnd);
    CheatLog("game hwnd = %p\n", g_gameHwnd);
    if (g_gameHwnd)
        g_oWndProc = (WNDPROC)SetWindowLongPtrA(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
    CheatLog("wndproc hooked, orig = %p\n", g_oWndProc);
}

static DWORD WINAPI MainThread(LPVOID)
{
    CheatLog("--- renderer validation build: x64 game features disabled ---\n");
    static const char* mods[] = { "d3d9.dll", "shaderapidx9.dll", "gameoverlayrenderer.dll",
        "engine.dll", "client.dll", "vgui2.dll", "vguimatsurface.dll", "materialsystem.dll" };
    for (int i = 0; i < 8; i++)
        CheatLog("%s = %p\n", mods[i], GetModuleHandleA(mods[i]));
    LoadConfig();
    g_menuOpen = true; // menu opens on inject; features stay off until enabled
    InitGameHooks();
    InitD3D9Hook();
    CheatLog("=== init done (menuKey=%d, aimKey=%d, unload=END) ===\n", g_cfg.menuKey, g_cfg.aimbot.key);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
