#include "globals.h"

// ---------- stage 8 x64 SetupBones runtime validation ----------
// IDA: client.dll+0xEAAA0. The decompiled implementation subtracts 8 from
// RCX before accessing the C_BaseEntity portion, so the verified C_BaseAnimating
// subobject is the entity base + 8. This probe runs only once on the live local
// CNMRiH_Player and logs matrices; it intentionally does not select a head bone.
#ifdef _WIN64
struct BoneMatrix34
{
    float m[3][4];
};

typedef bool(__fastcall* SetupBonesFn)(void* animSubobject, BoneMatrix34* out,
    int maxBones, int boneMask, float currentTime);

// CStudioHdr is stored at (entity + 8) + 3264 by the verified SetupBones
// implementation. Its first pointer is the studiohdr_t. The studiohdr_t bone
// count/table fields and mstudiobone_t stride below were cross-checked against
// the installed NMRiH player MDLs, including p_bateman.mdl (62 bones, head=15).
static bool ResolveHeadBone(void* player, int& outBone, int& outCount)
{
    outBone = -1; outCount = 0;
    static int diag = 0;
    __try
    {
        unsigned char* base = (unsigned char*)player;
        void* studioHdrWrapper = *(void**)(base + 8 + 3264);
        if (!studioHdrWrapper) { if (diag++ < 3) CheatLog("stage11 head: no CStudioHdr wrapper player=%p\n", player); return false; }
        unsigned char* studioHdr = *(unsigned char**)studioHdrWrapper;
        if (!studioHdr) { if (diag++ < 3) CheatLog("stage11 head: wrapper=%p has null studiohdr\n", studioHdrWrapper); return false; }
        int count = *(int*)(studioHdr + 156);
        int boneTable = *(int*)(studioHdr + 160);
        if (diag < 3) CheatLog("stage11 head: player=%p wrapper=%p hdr=%p count=%d table=%d\n", player, studioHdrWrapper, studioHdr, count, boneTable);
        if (count < 1 || count > 256 || boneTable < 0) return false;
        for (int i = 0; i < count; ++i)
        {
            unsigned char* bone = studioHdr + boneTable + i * 216;
            int nameOffset = *(int*)bone;
            if (nameOffset <= 0 || nameOffset > 0x100000) continue;
            const char* name = (const char*)(bone + nameOffset);
            char copied[128] = {};
            int n = 0;
            for (; n < (int)sizeof(copied) - 1; ++n)
            {
                char c = name[n];
                if (!c) break;
                if ((unsigned char)c < 32 || (unsigned char)c > 126) break;
                copied[n] = c;
            }
            if (!copied[0]) continue;
            if (_stricmp(copied, "ValveBiped.Bip01_Head1") == 0 ||
                strstr(copied, "Head") != nullptr || strstr(copied, "head") != nullptr)
            {
                outBone = i;
                outCount = count;
                if (diag++ < 6) CheatLog("stage11 head: resolved bone=%d name=%s count=%d\n", i, copied, count);
                return true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    if (diag++ < 6) CheatLog("stage11 head: exception or no named head for player=%p\n", player);
    return false;
}

static bool GetExactHeadPosition(void* player, float out[3])
{
    int headBone = -1, boneCount = 0;
    if (!ResolveHeadBone(player, headBone, boneCount)) return false;
    HMODULE client = GetModuleHandleA("client.dll");
    if (!client) return false;
    BoneMatrix34 bones[256] = {};
    bool ok = false;
    __try
    {
        SetupBonesFn setup = (SetupBonesFn)((unsigned char*)client + 0x0EAAA0);
        ok = setup((unsigned char*)player + 8, bones, boneCount, 0x7FF00, 0.0f);
        if (!ok || headBone < 0 || headBone >= boneCount) return false;
        out[0] = bones[headBone].m[0][3];
        out[1] = bones[headBone].m[1][3];
        out[2] = bones[headBone].m[2][3];
        return _finite(out[0]) && _finite(out[1]) && _finite(out[2]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Verified from engine.dll in IDA: EngineTraceClient003 -> TraceRay is vt[4],
// and the implementation reads Ray_t at offsets 0,16,32,48,64. Runtime
// capture on this build shows the returned hit fraction at trace offset 44.
struct TraceRayInput
{
    float start[4];
    float delta[4];
    float startOffset[4];
    float extents[4];
    bool isRay;
    bool isSwept;
    unsigned char pad[14];
};
struct TraceResult { unsigned char raw[128]; };
class TraceFilterSkipPair
{
public:
    TraceFilterSkipPair(void* a, void* b) : skipA(a), skipB(b) {}
    virtual bool ShouldHitEntity(void* entity, int) { return entity != skipA && entity != skipB; }
    virtual int GetTraceType() { return 0; } // engine's default filter trace type
private:
    void* skipA;
    void* skipB;
};
typedef void(__fastcall* TraceRayFn)(void*, const TraceRayInput*, unsigned int, TraceFilterSkipPair*, TraceResult*);

static void* GetVerifiedEngineTrace()
{
    static void* trace = nullptr;
    static bool tried = false;
    if (tried) return trace;
    tried = true;
    HMODULE engine = GetModuleHandleA("engine.dll");
    if (!engine) return nullptr;
    typedef void* (*CreateInterfaceFn)(const char*, int*);
    CreateInterfaceFn factory = (CreateInterfaceFn)GetProcAddress(engine, "CreateInterface");
    if (!factory) return nullptr;
    void* candidate = factory("EngineTraceClient003", nullptr);
    if (!candidate) return nullptr;
    __try
    {
        void* traceRay = (*(void***)candidate)[4];
        if (traceRay != (unsigned char*)engine + 0x18E970)
        {
            CheatLog("stage9 TraceRay validation failed: iface=%p vt4=%p expected=%p\n", candidate, traceRay, (unsigned char*)engine + 0x18E970);
            return nullptr;
        }
        trace = candidate;
        CheatLog("stage9 TraceRay verified: iface=%p vt4=%p\n", trace, traceRay);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { trace = nullptr; }
    return trace;
}

static bool TraceVisible(void* local, void* target, const float from[3], const float to[3])
{
    void* trace = GetVerifiedEngineTrace();
    if (!trace) return false;
    TraceRayInput ray = {};
    ray.start[0] = from[0]; ray.start[1] = from[1]; ray.start[2] = from[2];
    ray.delta[0] = to[0] - from[0]; ray.delta[1] = to[1] - from[1]; ray.delta[2] = to[2] - from[2];
    ray.isRay = true; ray.isSwept = true;
    TraceResult result = {};
    TraceFilterSkipPair filter(local, target);
    __try
    {
        ((TraceRayFn)(*(void***)trace)[4])(trace, &ray, 0xFFFFFFFFu, &filter, &result);
        float fraction = 0.0f;
        memcpy(&fraction, result.raw + 44, sizeof(fraction));
        static int traceDiag = 0;
        if (traceDiag++ < 4)
        {
            float f40=0, f44=0, f48=0, f52=0, f56=0, f60=0, f64=0, f68=0;
            memcpy(&f40, result.raw + 40, 4); memcpy(&f44, result.raw + 44, 4);
            memcpy(&f48, result.raw + 48, 4); memcpy(&f52, result.raw + 52, 4);
            memcpy(&f56, result.raw + 56, 4); memcpy(&f60, result.raw + 60, 4);
            memcpy(&f64, result.raw + 64, 4); memcpy(&f68, result.raw + 68, 4);
            CheatLog("stage12 trace: target=%p f40=%.3f f44=%.3f f48=%.3f f52=%.3f f56=%.3f f60=%.3f f64=%.3f f68=%.3f\n",
                target, f40, f44, f48, f52, f56, f60, f64, f68);
        }
        return _finite(fraction) && fraction >= 0.999f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void ProbeSetupBones(void* player, int playerIndex)
{
    static bool attempted = false;
    if (attempted || !player) return;
    attempted = true;

    HMODULE client = GetModuleHandleA("client.dll");
    if (!client)
    {
        CheatLog("stage8 SetupBones: client.dll unavailable\n");
        return;
    }

    const uintptr_t rva = 0x0EAAA0;
    void* entry = (unsigned char*)client + rva;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(entry, &mbi, sizeof(mbi)) || !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
    {
        CheatLog("stage8 SetupBones: entry %p is not executable\n", entry);
        return;
    }

    BoneMatrix34 bones[256] = {};
    bool ok = false;
    __try
    {
        // The IDA pseudocode verifies a3 is the output capacity and a4 is
        // the bone mask. The fifth argument is the source currentTime float.
        ok = ((SetupBonesFn)entry)((unsigned char*)player + 8, bones, 256, 0x7FF00, 0.0f);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        CheatLog("stage8 SetupBones: exception calling %p for player=%p\n", entry, player);
        return;
    }
    if (!ok)
    {
        CheatLog("stage8 SetupBones: returned false for local=%d base=%p\n", playerIndex, player);
        return;
    }

    int boneCount = 0;
    __try { boneCount = *(int*)((unsigned char*)player + 8 + 3168); }
    __except (EXCEPTION_EXECUTE_HANDLER) { boneCount = 0; }
    if (boneCount < 1 || boneCount > 256) boneCount = 256;

    int highest = -1;
    float highestZ = -3.402823e+38f;
    CheatLog("stage8 SetupBones OK: local=%d base=%p this=%p count=%d entry=%p\n",
        playerIndex, player, (unsigned char*)player + 8, boneCount, entry);
    for (int i = 0; i < boneCount; ++i)
    {
        const float x = bones[i].m[0][3], y = bones[i].m[1][3], z = bones[i].m[2][3];
        if (!_finite(x) || !_finite(y) || !_finite(z)) continue;
        if (z > highestZ) { highestZ = z; highest = i; }
        CheatLog("stage8 bone[%d] %.3f %.3f %.3f\n", i, x, y, z);
    }
    CheatLog("stage8 highest valid bone=%d z=%.3f (diagnostic only; not treated as head)\n", highest, highestZ);
}
#endif

// ---------- aimbot ----------
static void AimbotTick(void* lp, int localIdx)
{
    if (!g_cfg.aimbot.enabled) return;
    if (!(GetAsyncKeyState(g_cfg.aimbot.key) & 0x8000)) return;

    float eye[3];
    float* org = (float*)((unsigned char*)lp + NV_ORIGIN);
    eye[0] = org[0]; eye[1] = org[1]; eye[2] = org[2] + 64.0f;

    float va[3] = { 0, 0, 0 };
    __try { eng_GetViewAngles(va); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!_finite(va[0]) || !_finite(va[1])) return;
    // Source QAngle: [0]=pitch, [1]=yaw
    float viewPitch = va[0];
    float viewYaw = NormYaw(va[1]);

    int localTeam = *(int*)((unsigned char*)lp + NV_TEAMNUM);
    int highest = ent_GetHighest();
    if (highest > 2048) highest = 2048;

    float bestAngle = g_cfg.aimbot.fov;
    float bestDistance = 3.402823e+38f;
    int bestIdx = -1;
    float bestYaw = 0, bestPitch = 0;

    for (int i = 1; i < highest; i++)
    {
        if (i == localIdx) continue;
        void* e = ent_GetEntity(i);
        if (!e) continue;

        char cls[64];
        if (!SafeClassName(EntClassName(e), cls, 64)) continue;

        bool isPlayer = strcmp(cls, "CNMRiH_Player") == 0;
        bool isZombie = strstr(cls, "Zombie") != nullptr || strcmp(cls, "CAI_BaseNPC") == 0;
        if (!isPlayer && !isZombie) continue;
        if (isPlayer)
        {
            if (!g_cfg.aimbot.players && !g_cfg.aimbot.anyPlayer) continue;
            if (!g_cfg.aimbot.anyPlayer && *(int*)((unsigned char*)e + NV_TEAMNUM) == localTeam) continue;
        }
        else if (!g_cfg.aimbot.zombies) continue;

        if (*(unsigned char*)((unsigned char*)e + NV_LIFESTATE) != 0) continue;

        float head[3] = {};
        if (!GetExactHeadPosition(e, head)) continue;
        if (!TraceVisible(lp, e, eye, head)) continue;
        float dx = head[0] - eye[0], dy = head[1] - eye[1], dz = head[2] - eye[2];
        float yaw = atan2f(dy, dx) * 57.2957795f;
        float dist2d = sqrtf(dx * dx + dy * dy);
        // Source QAngle pitch is negative when aiming upward.
        float pitch = -atan2f(dz, dist2d) * 57.2957795f;

        float dYaw = NormYaw(yaw - viewYaw);
        float dPitch = pitch - viewPitch;
        float d = sqrtf(dYaw * dYaw + dPitch * dPitch);
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d <= g_cfg.aimbot.fov && distance < bestDistance)
        {
            bestAngle = d;
            bestDistance = distance;
            bestIdx = i;
            bestYaw = yaw;
            bestPitch = pitch;
        }
    }

    if (bestIdx >= 0)
    {
        float smooth = g_cfg.aimbot.smooth;
        if (smooth < 1.0f) smooth = 1.0f;
        va[0] = viewPitch + (bestPitch - viewPitch) / smooth;
        va[1] = NormYaw(viewYaw + NormYaw(bestYaw - viewYaw) / smooth);
        static int logFrames = 0;
        if ((++logFrames % 60) == 1)
            CheatLog("aim target=%d yaw=%.1f pitch=%.1f fov=%.1f smooth=%.1f\n",
                bestIdx, bestYaw, bestPitch, bestAngle, smooth);
        __try { eng_SetViewAngles(va); }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
}

// ---------- misc ----------
static void MiscTick(void* lp)
{
    __try
    {
        if (g_cfg.misc.stamina)
        {
            // DISABLED: the 0x1644 offset is unverified for this build and the
            // write corrupts memory (crashes). Re-enable after getting the
            // real offset from the SourceDissector dump.
            static bool noted = false;
            if (!noted)
            {
                noted = true;
                CheatLog("stamina: write disabled (offset unverified)\n");
            }
        }

        if (g_cfg.misc.speed != 1.0f)
            *(float*)((unsigned char*)lp + NV_LAGGED_MOVE) = g_cfg.misc.speed;

        if (g_cfg.misc.god)
        {
            *(int*)((unsigned char*)lp + NV_HEALTH) = 100;
            *(unsigned char*)((unsigned char*)lp + NV_LIFESTATE) = 0;
        }

        if (g_cfg.misc.ammo)
        {
            int whandle = *(int*)((unsigned char*)lp + NV_ACTIVE_WEAPON);
            int widx = whandle & 0xFFF;
            if (widx > 0 && widx < 2048)
            {
                void* wpn = ent_GetEntity(widx);
                if (wpn)
                {
                    *(int*)((unsigned char*)wpn + NV_CLIP1) = 30;
                    int ammoType = *(int*)((unsigned char*)wpn + NV_PRIMARY_AMMO_TYPE);
                    if (ammoType >= 0 && ammoType < 32)
                        *(int*)((unsigned char*)lp + NV_AMMO + ammoType * 4) = 200;
                }
            }
        }

        if (g_cfg.misc.bhop)
        {
            static bool wasGround = false;
            static bool logged = false;
            int flags = *(int*)((unsigned char*)lp + NV_FLAGS);
            bool onGround = (flags & FL_ONGROUND) != 0;
            bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (space && onGround && !wasGround)
            {
                eng_ClientCmd("+jump");
                eng_ClientCmd("-jump");
                // direct jump impulse - pure netvar write, no engine command
                *(float*)((unsigned char*)lp + 0xFC) = 268.3f;
                if (!logged)
                {
                    logged = true;
                    CheatLog("bhop fired: flags=0x%X vz=268.3\n", flags);
                }
            }
            wasGround = onGround;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

#ifdef _WIN64
// The old MiscTick bundle remains disabled on x64 because its stamina/speed/
// movement writes are not verified. The ammo netvars are verified, but their
// values must not be forced every frame: doing so races the weapon's own fire
// and damage code.
static void AmmoTickX64(void* lp)
{
    if (!g_cfg.misc.ammo) return;
    __try
    {
        int whandle = *(int*)((unsigned char*)lp + NV_ACTIVE_WEAPON);
        int widx = whandle & 0xFFF;
        if (widx <= 0 || widx >= 2048) return;
        void* wpn = ent_GetEntity(widx);
        if (!wpn) return;

        // Never write the active magazine: forcing clip values races the
        // weapon fire code and can make shots register without damage.
        // Replenish reserve ammo only, allowing normal firing/reload logic.

        int ammoType = *(int*)((unsigned char*)wpn + NV_PRIMARY_AMMO_TYPE);
        if (ammoType >= 0 && ammoType < 32)
        {
            int* reserve = (int*)((unsigned char*)lp + NV_AMMO + ammoType * 4);
            int reserveValue = *reserve;
            // Keep special/invalid negative reserve values intact. A reserve
            // is replenished only when it has actually run out or is low.
            if (reserveValue >= 0 && reserveValue <= 5)
            {
                *reserve = 200;
                CheatLog("ammo: refilled reserve type=%d from %d to 200\\n", ammoType, reserveValue);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}
#endif

// called every rendered frame from the EndScene hook
void GameplayTick()
{
    int localIdx = eng_GetLocalPlayer();
    if (localIdx <= 0) return;
    void* lp = ent_GetEntity(localIdx);
    if (!lp) return;

#ifndef _WIN64
    // sv_cheats bypass: engine.dll + 0x5FDB80 = 1 (reverse-engineered from the
    // NMRiH Sv_cheats Bypass tool - it writes this via WriteProcessMemory)
    if (g_cfg.exploit.svbypass)
    {
        static bool noted = false;
        __try
        {
            HMODULE eng = GetModuleHandleA("engine.dll");
            if (eng)
            {
                *(DWORD*)((unsigned char*)eng + 0x5FDB80) = 1;
                if (!noted)
                {
                    noted = true;
                    CheatLog("svbypass: wrote 1 to engine+0x5FDB80 (%p)\n", (unsigned char*)eng + 0x5FDB80);
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }
#endif

    // stale-pointer guard: only write to a live CNMRiH_Player
    char cls[64];
    if (!SafeClassName(EntClassName(lp), cls, 64) || strcmp(cls, "CNMRiH_Player") != 0) return;

    static bool liveLogged = false;
    if (!liveLogged)
    {
        liveLogged = true;
        __try
        {
            CheatLog("GameplayTick live: local=%d hp=%d team=%d stamina=%.0f cls=%s\n",
                localIdx, *(int*)((unsigned char*)lp + NV_HEALTH),
                *(int*)((unsigned char*)lp + NV_TEAMNUM),
                *(float*)((unsigned char*)lp + NV_STAMINA),
                EntClassName(lp) ? EntClassName(lp) : "?");
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }

#ifdef _WIN64
    ProbeSetupBones(lp, localIdx);
    AmmoTickX64(lp);
#endif

#ifndef _WIN64
    MiscTick(lp);
#endif
    AimbotTick(lp, localIdx);
}
