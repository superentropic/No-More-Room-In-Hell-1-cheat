#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include "vmthook.h"

// ---------- interface pointers ----------
extern void* g_engine;        // VEngineClient013/014
extern void* g_entlist;       // VClientEntityList003

// ---------- confirmed vtable indices ----------
// engine Source SDK 2013 layout: GetLocalPlayer=12, GetViewAngles=19,
// SetViewAngles=20, IsInGame=26, ClientCmd=7, WorldToScreenMatrix=36
// entlist mode is selected at runtime; GetHighestEntityIndex=6
// entity: GetClientClass = vt[2] of object at entity+8 ; ClientClass name at +8

// ---------- netvars ----------
#define NV_HEALTH      0xD0
#define NV_LIFESTATE   0xCF
#define NV_TEAMNUM     0xD8
#define NV_ORIGIN      0x430
#define NV_FLAGS       0x448
#define NV_STAMINA     0x1C84
#define NV_SPRINTING   0x1C80
#define NV_SPRINT_ENABLED 0x1DD5
#define NV_LAGGED_MOVE 0x1978
#define NV_MAXSPEED    0x1768
#define NV_HAS_WALKIE  0x1DD2
#define NV_ACTIVE_WEAPON 0x12C0
#define NV_CLIP1       0xD58
#define NV_AMMO        0x1180
#define NV_PRIMARY_AMMO_TYPE 0xD50
#define NV_EYEANGLES   0x1CB8
#define NV_IFOV        0x16C0
#define NV_DEFAULT_FOV 0x16CC

#define FL_ONGROUND 1

// ---------- config ----------
struct CfgAimbot
{
    bool enabled = false;
    bool showFov = true;    // draw the on-screen FOV circle
    float fov = 15.0f;
    float smooth = 3.0f;
    bool players = false;   // enemy players
    bool anyPlayer = false; // aim at players regardless of team (PVP)
    bool zombies = false;   // npc zombies
    int key = VK_MENU;      // alt
};

struct CfgEsp
{
    bool enabled = false;
    float range = 80.0f;   // draw distance in meters
    float xOffset = 0.0f;  // fine-tune calibration
    float yOffset = 0.0f;
    bool teammates = false;
    bool infected = false;
    bool zombies = false;
    bool items = false;
    bool objectives = false;
    // per-category draw options: 0=teammate 1=infected 2=zombie 3=item 4=objective
    bool catBox[5] = { false, false, false, false, false };
    bool catName[5] = { false, false, false, false, false };
    bool catDist[5] = { false, false, false, false, false };
    bool catHp[5] = { false, false, false, false, false };
    float clr_team[3] = { 0.0f, 1.0f, 0.0f };
    float clr_inf[3] = { 1.0f, 1.0f, 0.0f };
    float clr_zombie[3] = { 1.0f, 0.0f, 0.0f };
    float clr_item[3] = { 0.6f, 0.8f, 1.0f };
    float clr_obj[3] = { 1.0f, 1.0f, 1.0f };
};

struct CfgMisc
{
    bool stamina = false;
    float speed = 1.0f;
    bool god = false;
    bool bhop = false;
    bool ammo = false;
    bool hud = false;
    float brightness = 0.0f; // 0-100
};

struct CfgExploit
{
    bool svbypass = false; // write 1 to engine.dll+0x5FDB80 (sv_cheats bypass)
};

struct Cfg
{
    int lang = 0; // 0=en, 1=zh
    int menuKey = VK_INSERT;
    CfgAimbot aimbot;
    CfgEsp esp;
    CfgMisc misc;
    CfgExploit exploit;
};

extern Cfg g_cfg;

void LoadConfig();
void SaveConfig();

// ---------- language ----------
inline const char* L(const char* en, const char* zh)
{
    return g_cfg.lang == 1 ? zh : en;
}

// ---------- engine helpers ----------
typedef int   (__thiscall* tGetLocalPlayer)(void*);
typedef void  (__thiscall* tGetViewAngles)(void*, float*);
typedef void  (__thiscall* tSetViewAngles)(void*, float*);
typedef int   (__thiscall* tIsPointVisible)(void*, int, const float*);
typedef bool  (__thiscall* tIsInGame)(void*);
typedef void  (__thiscall* tClientCmd)(void*, const char*);
typedef void* (__thiscall* tGetEntity)(void*, int);
typedef int   (__thiscall* tGetHighest)(void*);

inline int eng_GetLocalPlayer()
{
    if (!g_engine) return -1;
    return ((tGetLocalPlayer)(*(void***)g_engine)[12])(g_engine);
}
inline void eng_GetViewAngles(float* va)
{
    if (!g_engine) { va[0] = va[1] = va[2] = 0; return; }
    ((tGetViewAngles)(*(void***)g_engine)[19])(g_engine, va);
}
inline void eng_SetViewAngles(float* va)
{
    if (!g_engine) return;
    ((tSetViewAngles)(*(void***)g_engine)[20])(g_engine, va);
}
inline bool eng_IsPointVisible(int entIndex, const float* point)
{
    if (!g_engine || !point) return false;
    __try { return ((tIsPointVisible)(*(void***)g_engine)[13])(g_engine, entIndex, point) != 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline bool eng_IsInGame()
{
    if (!g_engine) return false;
    return ((tIsInGame)(*(void***)g_engine)[26])(g_engine);
}
inline void eng_ClientCmd(const char* cmd)
{
    if (!g_engine) return;
    ((tClientCmd)(*(void***)g_engine)[7])(g_engine, cmd);
}

// entity access mode - probed at runtime (see ProbeEntityMode in cheat.cpp)
extern int g_entSlot;       // entlist vtable slot (0 or 1)
extern int g_entBaseDelta;  // 0 if slot returns base, -8 if it returns base+8
extern int g_ccSubOffset;   // 8 = GetClientClass on entity+8, 0 = on entity itself

inline void* ent_GetEntity(int idx)
{
    if (!g_entlist) return nullptr;
    void* r = ((tGetEntity)(*(void***)g_entlist)[g_entSlot])(g_entlist, idx);
    if (!r) return nullptr;
    return (void*)((unsigned char*)r + g_entBaseDelta);
}
inline int ent_GetHighest()
{
    if (!g_entlist) return 0;
    return ((tGetHighest)(*(void***)g_entlist)[6])(g_entlist);
}

// entity helpers
inline void* EntBase(void* e) { return e; }
const char* EntClassName(void* e);
bool SafeClassName(const char* src, char* out, int outLen);
bool WorldToScreen(const float* pos, float& sx, float& sy);

// ---------- feature entry points ----------
void GameplayTick();   // called from the tick thread
void EspCollect();     // called from the tick thread (game calls, cached)
void EspDraw();        // called from EndScene (ImGui)
void FovDraw();        // aimbot FOV circle (always on when aimbot enabled)
void BrightDraw();     // brightness filter overlay
void HudDraw();        // own health/stamina bars (cached data)
void MenuRender();     // called from EndScene
void InitImGui(IDirect3DDevice9* dev);
void PrepareProjection();

// key capture (keybind rebinding)
extern int g_capturing; // 0=none, 1=aim key, 2=menu key
const char* VkName(int vk);
void CheatLog(const char* fmt, ...);
void RestoreAndUnload();

// ---------- hook state ----------
extern bool g_menuOpen;
extern bool g_imguiReady;
extern HWND g_gameHwnd;
extern int g_espCount;

float NormYaw(float yaw);
