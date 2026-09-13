// NMRiH runtime dumper - probes interfaces, entity netprops and vtable indices
// Self-validating with SEH so wrong guesses never crash. Writes dump_log.txt.
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef void* (*CreateInterfaceFn)(const char* name, int* ret);

#define ARRSZ(a) (sizeof(a)/sizeof(a[0]))

static FILE* g_log = NULL;
static void L(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    fflush(g_log);
    va_end(ap);
}

// ---- minimal Source interface declarations (x86, correct vtable layout) ----
class IClientNetworkable;
class IClientEntity;

class IClientUnknown
{
public:
    virtual IClientUnknown* GetIClientUnknown() = 0;
    virtual IClientNetworkable* GetIClientNetworkable() = 0;
    virtual void* GetIClientRenderable() = 0;
    virtual IClientEntity* GetIClientEntity() = 0;
    virtual void* GetIClientThinkable() = 0;
    virtual void* GetIClientCollideable() = 0;
    virtual void* GetIClientEntityList() = 0;
    virtual void* GetIClientModelRenderable() = 0;
};

class RecvProp;
class RecvTable;

class RecvProp
{
public:
    char* m_pVarName;
    int m_RecvType;
    int m_Flags;
    int m_StringBufferSize;
    int m_bInsideArray;
    const void* m_pExtraData;
    RecvProp* m_pArrayProp;
    void* m_ArrayLengthProxy;
    void* m_ProxyFn;
    void* m_DataTableProxyFn;
    RecvTable* m_pDataTable;
    int m_Offset;
    int m_ElementStride;
    int m_nElements;
    const char* m_pParentArrayPropName;
};

class RecvTable
{
public:
    RecvProp* m_pProps;
    int m_nProps;
    void* m_pDecoder;
    char* m_pNetTableName;
    int m_bInitialized;
    int m_bInMainList;
};

class ClientClass
{
public:
    void* m_pCreateFn;
    void* m_pCreateEventFn;
    char* m_pNetworkName;
    RecvTable* m_pRecvTable;
    ClientClass* m_pNext;
    int m_ClassID;
};

class IClientNetworkable
{
public:
    virtual IClientUnknown* GetIClientUnknown2() = 0;
    virtual void Release() = 0;
    virtual ClientClass* GetClientClass() = 0;
};

class IClientEntity : public IClientNetworkable
{
public:
    virtual int GetIndex() = 0;
    virtual void* GetNetworkable() = 0;
    virtual void* GetEntity() = 0;
    virtual int GetClassID() = 0;
};

class IVEngineClient
{
public:
    virtual bool Connect(void* factory, int version) = 0;
    virtual void Disconnect() = 0;
    virtual int GetEngineBuildNumber() = 0;
    virtual const char* GetProductVersionString() = 0;
    virtual int GetGameBuildNumber() = 0;
    virtual int GetPlayerInfo(int ent_num, void* pinfo) = 0;
    virtual int GetPlayerForUserID(int userid) = 0;
    virtual void* GetNetChannelInfo() = 0;
    virtual void DebugDrawPhysCollide() = 0;
    virtual void Con_NPrintf(int pos, const char* fmt, ...) = 0;
    virtual void Con_NXPrintf(void* con_nprint_info, const char* fmt, ...) = 0;
    virtual int IsBoxVisible(int entindex, const float* absmin, const float* absmax) = 0;
    virtual int IsBoxInViewCluster(int entindex, const float* absmin, const float* absmax) = 0;
    virtual int IsPointVisible(int entindex, const float* point) = 0;
    virtual int GetMaxClients() = 0;
    virtual const char* Key_LookupBinding(const char* binding) = 0;
    virtual const char* Key_LookupBindingContext(const char* binding, void* ctx) = 0;
    virtual void* Key_SetBinding() = 0;
    virtual void StartSound() = 0;
    virtual void* ChangeGameUIState() = 0;
    virtual void GetViewAngles(float* va) = 0;
    virtual void SetViewAngles(float* va) = 0;
    virtual int GetMaxPlayers() = 0;
    virtual const char* GetLevelName() = 0;
    virtual const char* GetLevelNameShort() = 0;
    virtual const char* GetMapGroupName() = 0;
    virtual int GetClusterForOrigin(const float* org) = 0;
    virtual int GetClusterTreeSize() = 0;
    virtual void* GetClusterTree() = 0;
    virtual bool IsInGame() = 0;
    virtual bool IsConnected() = 0;
    virtual bool IsDrawingLoadingImage() = 0;
    virtual void ClientCmd(const char* cmd) = 0;
    virtual void ClientCmd_Unrestricted(const char* cmd) = 0;
    virtual bool IsOccluded(const float* absmin, const float* absmax) = 0;
    virtual void GetScreenSize(int* w, int* h) = 0;
    virtual void ServerCmd(const char* cmd) = 0;
    virtual int GetLocalPlayer() = 0;
    virtual void* GetPlayerInfoEx(int ent_num) = 0;
};

// ---- interface resolution ----
static void* GetInterface(HMODULE mod, const char* name)
{
    if (!mod) return NULL;
    CreateInterfaceFn fn = (CreateInterfaceFn)GetProcAddress(mod, "CreateInterface");
    if (!fn) return NULL;
    return fn(name, NULL);
}

// candidate interface versions per module - try all, log hits
static const char* g_engine_ifaces[] = {
    "VEngineClient013", "VEngineClient014", "VEngineClient012", "VEngineClient011",
    "VEngineCvar007", "VEngineCvar006", "VEngineModel016", "VEngineModel015",
    "VEngineRenderView014", "VEngineVGui001", "VEngineEffects001",
};
static const char* g_client_ifaces[] = {
    "VClientEntityList003", "VClientEntityList002", "VClientEntityList001",
    "VClient018", "VClient017", "VClient016", "VClient015",
    "VClientPrediction001", "VClientPrediction002",
};
static const char* g_panel_ifaces[] = { "VGUI_Panel009", "VGUI_Panel010", "VGUI_Panel008" };
static const char* g_surface_ifaces[] = { "VGUI_Surface030", "VGUI_Surface031", "VGUI_Surface029" };
static const char* g_matsys_ifaces[] = { "VMaterialSystem080", "VMaterialSystem081" };
static const char* g_enginevgui_ifaces[] = { "VEngineVGui001", "VEngineVGui002" };
static const char* g_overlay_ifaces[] = { "VDebugOverlay003", "VDebugOverlay004" };
static const char* g_modelinfo_ifaces[] = { "VModelInfoClient006", "VModelInfoClient004" };
static const char* g_studiorender_ifaces[] = { "VStudioRender025", "VStudioRender026", "VStudioRender024" };

static void DumpInterfaces(HMODULE engine, HMODULE client, HMODULE panel, HMODULE surface,
    HMODULE matsys, HMODULE overlay, HMODULE modelinfo, HMODULE studiorender,
    void** out_entlist, void** out_engine, void** out_localize)
{
    int i;
    for (i = 0; i < (int)ARRSZ(g_engine_ifaces); i++)
    {
        void* p = GetInterface(engine, g_engine_ifaces[i]);
        if (p) { L("engine: %s = %p\n", g_engine_ifaces[i], p);
            if (strstr(g_engine_ifaces[i], "VEngineClient")) *out_engine = p; }
    }
    for (i = 0; i < (int)ARRSZ(g_client_ifaces); i++)
    {
        void* p = GetInterface(client, g_client_ifaces[i]);
        if (p) { L("client: %s = %p\n", g_client_ifaces[i], p);
            if (strstr(g_client_ifaces[i], "VClientEntityList")) *out_entlist = p;
            if (strncmp(g_client_ifaces[i], "VClient", 7) == 0) *out_localize = p; }
    }
    for (i = 0; i < (int)ARRSZ(g_panel_ifaces); i++)
        if (GetInterface(panel, g_panel_ifaces[i])) L("vgui2: %s\n", g_panel_ifaces[i]);
    for (i = 0; i < (int)ARRSZ(g_surface_ifaces); i++)
        if (GetInterface(surface, g_surface_ifaces[i])) L("vguimatsurface: %s\n", g_surface_ifaces[i]);
    for (i = 0; i < (int)ARRSZ(g_matsys_ifaces); i++)
        if (GetInterface(matsys, g_matsys_ifaces[i])) L("materialsystem: %s\n", g_matsys_ifaces[i]);
    for (i = 0; i < (int)ARRSZ(g_enginevgui_ifaces); i++)
        if (GetInterface(engine, g_enginevgui_ifaces[i])) L("engine: %s\n", g_enginevgui_ifaces[i]);
    for (i = 0; i < (int)ARRSZ(g_overlay_ifaces); i++)
        if (GetInterface(engine, g_overlay_ifaces[i])) L("engine: %s\n", g_overlay_ifaces[i]);
    for (i = 0; i < (int)ARRSZ(g_modelinfo_ifaces); i++)
        if (GetInterface(engine, g_modelinfo_ifaces[i])) L("engine: %s\n", g_modelinfo_ifaces[i]);
    for (i = 0; i < (int)ARRSZ(g_studiorender_ifaces); i++)
        if (GetInterface(studiorender, g_studiorender_ifaces[i])) L("studiorender: %s\n", g_studiorender_ifaces[i]);
}

// ---- SEH-guarded probes ----
#pragma warning(disable: 4731)

typedef ClientClass* (__thiscall* GetClientClassFn)(void* thisptr);

// ---- pointer validation against module ranges ----
static DWORD_PTR g_clientBase = 0, g_clientEnd = 0;
static DWORD_PTR g_engineBase = 0, g_engineEnd = 0;

static void GetModuleRange(HMODULE h, DWORD_PTR* base, DWORD_PTR* end)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD_PTR b = (DWORD_PTR)h;
    *base = b; *end = b;
    while (VirtualQuery((void*)*end, &mbi, sizeof(mbi)) &&
        mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE)
        *end = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
}

static bool PtrInClient(void* p)
{
    DWORD_PTR v = (DWORD_PTR)p;
    return v >= g_clientBase && v < g_clientEnd;
}

static bool PtrInEngine(void* p)
{
    DWORD_PTR v = (DWORD_PTR)p;
    return v >= g_engineBase && v < g_engineEnd;
}

// follow up to 3 relative jmps (thunks) to reach real code
static unsigned char* ResolveThunk(unsigned char* p)
{
    __try
    {
        for (int k = 0; k < 3; k++)
        {
            if (p[0] == 0xE9) p = p + 5 + *(int*)(p + 1);
            else if (p[0] == 0xEB) p = p + 2 + *(char*)(p + 1);
            else break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    return p;
}

// True if fn looks like a zero-arg MSVC thiscall: first ret-like byte is plain
// ret (C3). If ret N (C2 imm16) appears first, the function takes args and
// calling it without args would corrupt the stack -> never call those.
static bool IsZeroArgSafe(void* fn)
{
    if (!fn) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(fn, &mbi, sizeof(mbi))) return false;
    if (mbi.Type != MEM_IMAGE) return false;
    DWORD p = mbi.Protect & 0xFF;
    if (p != PAGE_EXECUTE && p != PAGE_EXECUTE_READ &&
        p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY) return false;
    unsigned char* base = ResolveThunk((unsigned char*)fn);
    __try
    {
        for (int i = 0; i < 128; i++)
        {
            if (base[i] == 0xC3) return true;
            if (base[i] == 0xC2) return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}

// probe GetClientClass index on entity vtable (Source: 8 = 2013, 7 = 2009/older)
// ONLY zero-arg slots are called; wrong guess = harmless wrong return, never stack damage
static int ProbeGetClientClassIndex(void* ent)
{
    static const int cand[] = { 8, 7 };
    void** vt = *(void***)ent;
    for (int k = 0; k < 2; k++)
    {
        int idx = cand[k];
        __try
        {
            GetClientClassFn fn = (GetClientClassFn)vt[idx];
            ClientClass* cc = fn(ent);
            if (!cc || !PtrInClient(cc)) continue;
            if (!PtrInClient(cc->m_pRecvTable) || !PtrInClient(cc->m_pNetworkName)) continue;
            RecvTable* rt = cc->m_pRecvTable;
            if (!PtrInClient(rt->m_pProps)) continue;
            if (rt->m_nProps < 1 || rt->m_nProps > 400) continue;
            if (!PtrInClient(rt->m_pNetTableName)) continue;
            L("GetClientClass index = %d\n", idx);
            return idx;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    L("GetClientClass index NOT FOUND\n");
    return -1;
}

// dump all netprops of a class; find known props and their offsets
static void DumpProps(RecvTable* table, int depth, int baseOffset = 0, bool verbose = false)
{
    if (depth > 3 || !table || !PtrInClient(table)) return;
    if (!PtrInClient(table->m_pProps) || table->m_nProps <= 0 || table->m_nProps > 400) return;
    for (int i = 0; i < table->m_nProps; i++)
    {
        RecvProp* p = &table->m_pProps[i];
        if (!p || !PtrInClient(p)) continue;
        if (!p->m_pVarName || !PtrInClient(p->m_pVarName)) continue;
        __try
        {
            const char* nm = p->m_pVarName;
            int absoluteOffset = baseOffset + p->m_Offset;
            if (verbose || strcmp(nm, "m_iHealth") == 0 || strcmp(nm, "m_vecOrigin") == 0 ||
                strcmp(nm, "m_fFlags") == 0 || strcmp(nm, "m_lifeState") == 0 ||
                strcmp(nm, "m_iTeamNum") == 0 || strcmp(nm, "m_nModelIndex") == 0 ||
                strcmp(nm, "m_vecVelocity") == 0 || strcmp(nm, "m_flSimulationTime") == 0 ||
                strcmp(nm, "m_hActiveWeapon") == 0 || strcmp(nm, "m_iMaxHealth") == 0 ||
                strcmp(nm, "m_angRotation") == 0 || strcmp(nm, "m_hMyWeapons") == 0)
            {
                L("  [%s.%s] offset=%d (0x%X)\n",
                    table->m_pNetTableName ? table->m_pNetTableName : "?", nm,
                    absoluteOffset, absoluteOffset);
            }
            if (p->m_pDataTable && PtrInClient(p->m_pDataTable))
                DumpProps(p->m_pDataTable, depth + 1, absoluteOffset, verbose);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
}

// probe GetLocalPlayer on engine: ONLY the proven index 12 (returned valid
// local=1 on live server). No blind vtable scanning - calling random slots can
// invoke side-effect functions (Disconnect/Shutdown) which crash the game.
static int ProbeGetLocalPlayerIndex(void* engine, void* entlist, int maxidx)
{
    __try
    {
        typedef int (__thiscall* GetLP)(void*);
        GetLP fn = (GetLP)(*(void***)engine)[12];
        int lp = fn(engine);
        L("GetLocalPlayer slot12 -> %d\n", lp);
        if (lp > 0 && lp < maxidx)
        {
            __try
            {
                typedef void* (__thiscall* GetEnt)(void*, int);
                GetEnt ge = *(GetEnt*)(*(void***)entlist + 0);
                if (ge(entlist, lp)) { L("GetLocalPlayer index = 12 (local=%d)\n", lp); return 12; }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    L("GetLocalPlayer index NOT FOUND\n");
    return -1;
}

// probe GetClientEntity on entity list (UC dump: 1 = GetClientEntity, 0 = GetClientNetworkable)
static int ProbeGetClientEntityIndex(void* entlist)
{
    static const int cand[] = { 1, 0 };
    for (int k = 0; k < 2; k++)
    {
        int idx = cand[k];
        __try
        {
            typedef void* (__thiscall* GetEnt)(void*, int);
            GetEnt ge = (GetEnt)(*(void***)entlist)[idx];
            void* e = ge(entlist, 1);
            if (e)
            {
                __try
                {
                    void** evt = *(void***)e;
                    if (evt && evt[0])
                    {
                        L("GetClientEntity index = %d\n", idx);
                        return idx;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    L("GetClientEntity index NOT FOUND\n");
    return -1;
}

// ---- pure-read ClientClass discovery ----
// Calling vtable slots blindly is proven dangerous: slot 1 = Disconnect,
// slot 7 = Shutdown - those crash the game. Instead scan client.dll memory for
// ClientClass nodes (read-only, crash-proof) and find the list head.
static bool IsAsciiStr(void* p, int maxLen)
{
    if (!p) return false;
    __try
    {
        for (int i = 0; i < maxLen; i++)
        {
            char c = ((char*)p)[i];
            if (!c) return true;
            if (c < 32 || c > 126) return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}

static bool IsExec(void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.Type != MEM_IMAGE) return false;
    DWORD prot = mbi.Protect & 0xFF;
    return prot == PAGE_EXECUTE || prot == PAGE_EXECUTE_READ ||
        prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY;
}

static bool ValidClassNode(void* node)
{
    ClientClass* cc = (ClientClass*)node;
    __try
    {
        if (!cc->m_pCreateFn || !PtrInClient(cc->m_pCreateFn) || !IsExec(cc->m_pCreateFn)) return false;
        if (cc->m_pCreateEventFn && (!PtrInClient(cc->m_pCreateEventFn) || !IsExec(cc->m_pCreateEventFn))) return false;
        if (!PtrInClient(cc->m_pNetworkName) || !IsAsciiStr(cc->m_pNetworkName, 64)) return false;
        RecvTable* rt = cc->m_pRecvTable;
        if (!rt || !PtrInClient(rt)) return false;
        if (rt->m_nProps < 1 || rt->m_nProps > 400) return false;
        if (!PtrInClient(rt->m_pProps) || !PtrInClient(rt->m_pNetTableName)) return false;
        if (cc->m_ClassID < 0 || cc->m_ClassID > 1023) return false;
        if (cc->m_pNext && !PtrInClient(cc->m_pNext)) return false;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}

// find RecvTables via the "DT_" name prefix (Source nettable names always start
// with DT_) - pure reads, then dump known props with offsets for cross-checking
static void DumpRecvTables(void)
{
    L("scanning client.dll for RecvTables...\n");
    // Do not bake in the x86 RecvTable field offsets.  Pointer alignment and
    // member offsets change in the 64-bit client.
    for (DWORD_PTR p = g_clientBase; p + sizeof(RecvTable) < g_clientEnd; p += alignof(void*))
    {
        __try
        {
            int n = *(int*)(p + offsetof(RecvTable, m_nProps));
            if (n < 1 || n > 400) continue;
            char* name = *(char**)(p + offsetof(RecvTable, m_pNetTableName));
            if (!PtrInClient(name)) continue;
            if (name[0] != 'D' || name[1] != 'T' || name[2] != '_') continue;
            DWORD_PTR props = *(DWORD_PTR*)p;
            if (props < g_clientBase || props >= g_clientEnd) continue;
            L("recvtable @ %p: %s (%d props)\n", (void*)p, name, n);
            bool verbose = strstr(name, "NMRiH_Player") != NULL ||
                strstr(name, "NMRiH_WeaponBase") != NULL ||
                strstr(name, "BaseCombatWeapon") != NULL ||
                strcmp(name, "DT_Local") == 0 || strcmp(name, "DT_LocalPlayerExclusive") == 0;
            DumpProps((RecvTable*)p, 0, 0, verbose);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
    L("recvtable scan done\n");
}

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
                strncat(g_logPath, "dump_log.txt", sizeof(g_logPath) - strlen(g_logPath) - 1);
            }
        }
    }
    return g_logPath;
}

static DWORD WINAPI MainThread(LPVOID)
{
    g_log = fopen(LogPath(), "w");
    if (!g_log) return 0;
    L("NMRiH dumper v2.4\n");

    HMODULE engine = GetModuleHandleA("engine.dll");
    HMODULE client = GetModuleHandleA("client.dll");
    HMODULE panel = GetModuleHandleA("vgui2.dll");
    HMODULE surface = GetModuleHandleA("vguimatsurface.dll");
    HMODULE matsys = GetModuleHandleA("materialsystem.dll");
    HMODULE studiorender = GetModuleHandleA("studiorender.dll");
    L("modules: engine=%p client=%p vgui2=%p surface=%p matsys=%p studio=%p\n",
        engine, client, panel, surface, matsys, studiorender);

    if (engine) GetModuleRange(engine, &g_engineBase, &g_engineEnd);
    if (client) GetModuleRange(client, &g_clientBase, &g_clientEnd);
    L("ranges: engine=%p-%p client=%p-%p\n", (void*)g_engineBase, (void*)g_engineEnd,
        (void*)g_clientBase, (void*)g_clientEnd);

    void* entlist = NULL, * eng = NULL, * vclient = NULL;
    DumpInterfaces(engine, client, panel, surface, matsys, NULL, NULL, studiorender,
        &entlist, &eng, &vclient);

#ifdef _WIN64
    // The old probes call x86-era vtable slots. Until their x64 indices are
    // established, collect only exported interfaces and memory-resident
    // RecvTables; both paths are read-only and do not invoke entity methods.
    L("\n-- x64 safe data pass (legacy vtable probes skipped) --\n");
    if (eng)
    {
        void** vt = *(void***)eng;
        L("engine vtable=%p\n", vt);
        for (int i = 0; i < 48; i++)
            L("  engine[%d] = RVA 0x%llX\n", i,
                (unsigned long long)((DWORD_PTR)vt[i] - g_engineBase));
        __try
        {
            typedef int (*GetLP)(void*);
            L("engine[12]() returned %d\n", ((GetLP)vt[12])(eng));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { L("engine[12]() raised an exception\n"); }
    }
    if (entlist)
    {
        void** vt = *(void***)entlist;
        L("entity-list vtable=%p\n", vt);
        for (int i = 0; i < 16; i++)
            L("  entlist[%d] = RVA 0x%llX\n", i,
                (unsigned long long)((DWORD_PTR)vt[i] - g_clientBase));
        __try
        {
            typedef void* (*GetByIndex)(void*, int);
            int local = eng ? ((int (*)(void*))(*(void***)eng)[12])(eng) : -1;
            void* networkable = ((GetByIndex)vt[0])(entlist, local);
            void* entity = ((GetByIndex)vt[3])(entlist, local);
            L("local=%d networkable(slot0)=%p entity(slot3)=%p delta=%lld\n",
                local, networkable, entity,
                (long long)((DWORD_PTR)networkable - (DWORD_PTR)entity));
            if (networkable)
            {
                void** nvt = *(void***)networkable;
                typedef ClientClass* (*GetClass)(void*);
                ClientClass* cc = ((GetClass)nvt[2])(networkable);
                L("networkable vt=%p class=%p name=%s\n", nvt, cc,
                    cc && cc->m_pNetworkName ? cc->m_pNetworkName : "(null)");
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { L("entity-list verification raised an exception\n"); }
    }
    DumpRecvTables();
    L("\nDONE\n");
    fclose(g_log);
    return 0;
#endif

    if (!entlist) { L("FATAL: no entity list interface\n"); fclose(g_log); return 0; }
    if (!eng) { L("FATAL: no engine interface\n"); fclose(g_log); return 0; }

    int geIdx = ProbeGetClientEntityIndex(entlist);
    if (geIdx < 0) { L("FATAL: no GetClientEntity\n"); fclose(g_log); return 0; }

    // GetHighestEntityIndex: UC dump says index 6, zero-arg, safe
    int highest = 2048;
    __try
    {
        typedef int (__thiscall* GetHighFn)(void*);
        GetHighFn gh = (GetHighFn)(*(void***)entlist)[6];
        int h = gh(entlist);
        L("GetHighestEntityIndex -> %d\n", h);
        if (h > 100 && h < 20000) highest = h + 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { L("GetHighestEntityIndex probe crashed\n"); }
    L("highest = %d\n", highest);

    int lpIdx = ProbeGetLocalPlayerIndex(eng, entlist, highest);
    typedef int (__thiscall* GetLPFn)(void*);
    GetLPFn getLP = lpIdx >= 0 ? (GetLPFn)(*(void***)eng)[lpIdx] : NULL;
    int local = getLP ? getLP(eng) : -1;
    L("local player index = %d\n", local);

    // ---- vtable slot RVAs (pure reads) for offline disassembly analysis ----
    void** vtClient = vclient ? *(void***)vclient : NULL;
    void** vtEntlist = entlist ? *(void***)entlist : NULL;
    void** vtEngine = eng ? *(void***)eng : NULL;
    L("\n-- vtable slots --\n");
    if (vtClient)
    {
        L("VClient vt @ %p\n", vtClient);
        for (int i = 0; i < 64; i++)
            L("  client[%d] = RVA 0x%X\n", i, (DWORD)((DWORD_PTR)vtClient[i] - g_clientBase));
    }
    if (vtEntlist)
    {
        L("entlist vt @ %p\n", vtEntlist);
        for (int i = 0; i < 16; i++)
            L("  entlist[%d] = RVA 0x%X\n", i, (DWORD)((DWORD_PTR)vtEntlist[i] - g_clientBase));
    }
    if (vtEngine)
    {
        L("engine vt @ %p\n", vtEngine);
        for (int i = 0; i < 48; i++)
            L("  engine[%d] = RVA 0x%X\n", i, (DWORD)((DWORD_PTR)vtEngine[i] - g_engineBase));
    }

    // local player entity vtable (for GetClientClass discovery)
    if (local > 0)
    {
        __try
        {
            typedef void* (__thiscall* GE)(void*, int);
            GE ge = *(GE*)(*(void***)entlist + 0);
            void* ent = ge(entlist, local);
            if (ent)
            {
                void** vtE = *(void***)ent;
                L("entity vt @ %p\n", vtE);
                for (int i = 0; i < 24; i++)
                    L("  ent[%d] = RVA 0x%X\n", i, (DWORD)((DWORD_PTR)vtE[i] - g_clientBase));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }

    DumpRecvTables();

    L("\nDONE\n");
    fclose(g_log);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(h);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    return TRUE;
}
