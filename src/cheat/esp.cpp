#include "globals.h"

static ImU32 ToU32(const float* c, float a = 1.0f)
{
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(a * 255));
}

// ---------- cached ESP data: collected + drawn on the render thread ----------
struct EspEntry
{
    float x1, y1, x2, y2;   // head (top) and feet (bottom)
    float hpPct;
    ImU32 col;
    char label[64];
    int dist;
    bool isItem;
    bool drawBox, drawName, drawDist, drawHp;
};

static EspEntry g_esp[2048];
int g_espCount = 0;
static volatile LONG g_espReady = 0;

struct HudData
{
    volatile int hp;
    volatile float sta;
    volatile float spd;
    volatile float fov;
    volatile LONG valid;
};
HudData g_hud = { 0, 0, 0, 75.0f, 0 };

// ---------- collection ----------
// class-name cache: entities rarely change class - avoid 1400 vtable calls/frame
struct ClassCache { char cls[64]; void* entPtr; DWORD frame; };
static ClassCache g_ccache[2048];
static DWORD g_frameNo = 0;

void EspCollect()
{
    static int fc = 0;
    if ((++fc % 2) != 0) return; // collect every 2nd frame (class cache smooths it)

    g_frameNo++;

    int localIdx = eng_GetLocalPlayer();
    if (localIdx <= 0)
    {
        g_espCount = 0;
        InterlockedExchange(&g_espReady, 1);
        return;
    }
    void* lp = ent_GetEntity(localIdx);
    if (!lp)
    {
        g_espCount = 0;
        InterlockedExchange(&g_espReady, 1);
        return;
    }

    __try
    {
        int hp = *(int*)((unsigned char*)lp + NV_HEALTH);
        if (hp < 0) hp = 0;
        if (hp > 100) hp = 100;
        g_hud.hp = hp;
        float sta = *(float*)((unsigned char*)lp + NV_STAMINA);
        if (sta < 0) sta = 0;
        if (sta > 130) sta = 130;
        g_hud.sta = sta;
        g_hud.spd = *(float*)((unsigned char*)lp + NV_LAGGED_MOVE);
        int ifov = *(int*)((unsigned char*)lp + NV_IFOV);
        g_hud.fov = (ifov >= 40 && ifov <= 140) ? (float)ifov : 75.0f;
        InterlockedExchange(&g_hud.valid, 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { InterlockedExchange(&g_hud.valid, 0); }

    if (!g_cfg.esp.enabled)
    {
        g_espCount = 0;
        InterlockedExchange(&g_espReady, 1);
        return;
    }

    int localTeam = *(int*)((unsigned char*)lp + NV_TEAMNUM);
    float* lorg = (float*)((unsigned char*)lp + NV_ORIGIN);

    int highest = ent_GetHighest();
    if (highest > 2048) highest = 2048;

    int n = 0;
    int nClasses = 0, nPlayers = 0, nZombies = 0, nItems = 0;
    int nSkipDead = 0, nSkipOrigin = 0, nSkipRange = 0, nSkipW2S = 0;
    float nearestZ = 999999.0f, nearestI = 999999.0f;
    char sample[5][64];
    int nSample = 0;

    for (int i = 1; i < highest && n < 2048; i++)
    {
        if (i == localIdx) continue;
        void* e = ent_GetEntity(i);
        if (!e) continue;

        char cls[64];
        if (i < 2048)
        {
            ClassCache& cc = g_ccache[i];
            if (cc.entPtr == e && cc.frame + 120 > g_frameNo)
            {
                memcpy(cls, cc.cls, 64);
            }
            else
            {
                cc.entPtr = nullptr;
                if (!SafeClassName(EntClassName(e), cc.cls, 64)) continue;
                cc.entPtr = e;
                cc.frame = g_frameNo;
                memcpy(cls, cc.cls, 64);
            }
        }
        else if (!SafeClassName(EntClassName(e), cls, 64)) continue;
        nClasses++;
        if (nSample < 5) { _snprintf(sample[nSample], 64, "%s", cls); nSample++; }

        bool isPlayer = strcmp(cls, "CNMRiH_Player") == 0;
        // NMRiH 1.15 exposes spawned zombies through the generic client AI
        // class, while older builds exposed names containing "Zombie".
        bool isZombie = strstr(cls, "Zombie") != nullptr || strcmp(cls, "CAI_BaseNPC") == 0;
        bool isObjective = !isPlayer && !isZombie &&
            (strstr(cls, "Objective") != nullptr || strstr(cls, "WaveGameRules") != nullptr);
        bool isItem = !isPlayer && !isZombie && !isObjective && strncmp(cls, "CNMRiH_", 7) == 0;
        if (isPlayer) nPlayers++;
        if (isZombie) nZombies++;
        if (isItem) nItems++;

        const float* col = nullptr;
        const char* label = nullptr;
        int cat = 0;
        if (isPlayer)
        {
            int team = *(int*)((unsigned char*)e + NV_TEAMNUM);
            if (team == localTeam)
            {
                if (!g_cfg.esp.teammates) continue;
                col = g_cfg.esp.clr_team;
                label = L("Teammate", "\xe9\x98\x9f\xe5\x8f\x8b");
                cat = 0;
            }
            else
            {
                if (!g_cfg.esp.infected) continue;
                col = g_cfg.esp.clr_inf;
                label = L("Infected", "\xe6\x84\x9f\xe6\x9f\x93\xe8\x80\x85");
                cat = 1;
            }
        }
        else if (isZombie)
        {
            if (!g_cfg.esp.zombies) continue;
            col = g_cfg.esp.clr_zombie;
            label = L("Zombie", "\xe5\x83\xb5\xe5\xb0\xb8");
            cat = 2;
        }
        else if (isItem)
        {
            if (!g_cfg.esp.items) continue;
            col = g_cfg.esp.clr_item;
            label = cls + 7;
            cat = 3;
        }
        else if (isObjective)
        {
            if (!g_cfg.esp.objectives) continue;
            col = g_cfg.esp.clr_obj;
            label = L("Objective", "\xe7\x9b\xae\xe6\xa0\x87");
            cat = 4;
        }
        else continue;

        __try
        {
            if (*(unsigned char*)((unsigned char*)e + NV_LIFESTATE) != 0 && !isItem && cat != 4) { nSkipDead++; continue; }

            float* org = (float*)((unsigned char*)e + NV_ORIGIN);
            // skip unspawned entities parked at world origin (objectives exempt -
            // game-rule proxies may live at origin)
            if ((org[0] == 0.0f && org[1] == 0.0f && org[2] == 0.0f) && cat != 4) { nSkipOrigin++; continue; }
            float dx0 = org[0] - lorg[0], dy0 = org[1] - lorg[1], dz0 = org[2] - lorg[2];
            float distM = sqrtf(dx0 * dx0 + dy0 * dy0 + dz0 * dz0) / 39.37f;
            if (isZombie && distM < nearestZ) nearestZ = distM;
            if (isItem && distM < nearestI) nearestI = distM;
            if (distM > g_cfg.esp.range) { nSkipRange++; continue; }

            float height = isItem ? 20.0f : 72.0f;
            float feet[3] = { org[0], org[1], org[2] };
            float head[3] = { org[0], org[1], org[2] + height };

            float fx, fy, hx, hy;
            if (!WorldToScreen(feet, fx, fy)) { nSkipW2S++; continue; }
            if (!WorldToScreen(head, hx, hy)) { nSkipW2S++; continue; }

            EspEntry& en = g_esp[n++];
            en.x1 = hx; en.y1 = hy;
            en.x2 = fx; en.y2 = fy;
            en.col = ToU32(col);
            en.isItem = isItem;
            en.drawBox = g_cfg.esp.catBox[cat];
            en.drawName = g_cfg.esp.catName[cat];
            en.drawDist = g_cfg.esp.catDist[cat];
            en.drawHp = g_cfg.esp.catHp[cat];
            _snprintf(en.label, sizeof(en.label), "%s", label);

            int hp = *(int*)((unsigned char*)e + NV_HEALTH);
            if (hp < 0) hp = 0;
            if (hp > 100) hp = 100;
            en.hpPct = hp / 100.0f;

            en.dist = (int)distM;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }

    g_espCount = n;
    InterlockedExchange(&g_espReady, 1);

    static bool logged = false;
    if (!logged)
    {
        logged = true;
        CheatLog("ESP collect live: highest=%d classes=%d players=%d zombies=%d items=%d drawn=%d\n",
            highest, nClasses, nPlayers, nZombies, nItems, n);
        CheatLog("ESP cfg: items=%d catBox3=%d | skips: dead=%d origin=%d range=%d w2s=%d | nearest z=%.0fm i=%.0fm (range=%.0fm)\n",
            g_cfg.esp.items, g_cfg.esp.catBox[3],
            nSkipDead, nSkipOrigin, nSkipRange, nSkipW2S, nearestZ, nearestI, g_cfg.esp.range);
        char names[320] = "";
        for (int i = 0; i < nSample; i++)
        {
            strncat(names, sample[i], sizeof(names) - strlen(names) - 1);
            strncat(names, " | ", sizeof(names) - strlen(names) - 1);
        }
        CheatLog("sample classes: %s\n", names);
    }
}

// ---------- drawing: in-game via ImGui ----------
void EspDraw()
{
    if (!g_cfg.esp.enabled) return;
    if (!g_espReady || g_espCount == 0) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    float sh = io.DisplaySize.y;

    for (int i = 0; i < g_espCount; i++)
    {
        EspEntry& en = g_esp[i];

        float y1f = en.y1, y2f = en.y2;
        y1f += g_cfg.esp.yOffset;
        y2f += g_cfg.esp.yOffset;
        if (y1f > y2f) { float t = y1f; y1f = y2f; y2f = t; }
        float x1f = en.x1 + g_cfg.esp.xOffset;

        float hgt = y2f - y1f;
        if (hgt < 8.0f) hgt = 24.0f;
        float bw = hgt * 0.5f;
        float bx = x1f - bw * 0.5f;

        if (en.drawBox)
        {
            dl->AddLine(ImVec2(bx, y1f), ImVec2(bx + bw, y1f), en.col);
            dl->AddLine(ImVec2(bx + bw, y1f), ImVec2(bx + bw, y1f + hgt), en.col);
            dl->AddLine(ImVec2(bx + bw, y1f + hgt), ImVec2(bx, y1f + hgt), en.col);
            dl->AddLine(ImVec2(bx, y1f + hgt), ImVec2(bx, y1f), en.col);
        }

        if (en.drawHp && !en.isItem)
        {
            float barX = bx - 5.0f;
            dl->AddLine(ImVec2(barX, y1f), ImVec2(barX, y1f + hgt), IM_COL32(0, 0, 0, 180), 2.5f);
            ImU32 hpCol = IM_COL32((int)((1.0f - en.hpPct) * 255), (int)(en.hpPct * 255), 0, 255);
            dl->AddLine(ImVec2(barX, y1f + hgt * (1.0f - en.hpPct)), ImVec2(barX, y1f + hgt), hpCol, 2.5f);
        }

        float ty = y1f - 14.0f;
        if (en.drawName)
        {
            dl->AddText(ImVec2(bx, ty), en.col, en.label);
            ty -= 14.0f;
        }
        if (en.drawDist)
        {
            char txt[32];
            _snprintf(txt, sizeof(txt), "%dm", en.dist);
            dl->AddText(ImVec2(bx, ty), en.col, txt);
        }
    }
}

// brightness filter: fullscreen white overlay
void BrightDraw()
{
    if (g_cfg.misc.brightness <= 0.0f) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    float a = g_cfg.misc.brightness * 0.55f;
    if (a > 60.0f) a = 60.0f;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(io.DisplaySize.x, io.DisplaySize.y),
        IM_COL32(255, 255, 255, (int)a));
}

// aimbot FOV circle - always visible while aimbot is enabled
void FovDraw(){
    if (!g_cfg.aimbot.enabled || !g_cfg.aimbot.showFov) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    float iw = io.DisplaySize.x, ih = io.DisplaySize.y;
    if (iw > 1 && ih > 1)
    {
        ImVec2 center(iw * 0.5f, ih * 0.5f);
        float fovV = g_hud.fov > 1.0f ? g_hud.fov : 75.0f;
        float py = (ih * 0.5f) / tanf(fovV * 0.5f * 0.017453292f);
        float radius = py * tanf(g_cfg.aimbot.fov * 0.5f * 0.017453292f);
        dl->AddCircle(center, radius, IM_COL32(255, 70, 70, 140), 64);
        char buf[32];
        _snprintf(buf, sizeof(buf), "FOV %.0f", g_cfg.aimbot.fov);
        dl->AddText(ImVec2(center.x - 18.0f, center.y + radius + 4.0f),
            IM_COL32(255, 90, 90, 200), buf);
    }
}

// own health / stamina / speed HUD (render thread, cached data only)
void HudDraw()
{
    if (!g_cfg.misc.hud) return;
    if (!g_hud.valid) return;

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##hud", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    char buf[64];
    _snprintf(buf, sizeof(buf), "%s %d/100", L("HP", "\xe7\x94\x9f\xe5\x91\xbd"), g_hud.hp);
    ImGui::Text("%s", buf);
    ImGui::ProgressBar(g_hud.hp / 100.0f, ImVec2(150, 0));
    _snprintf(buf, sizeof(buf), "%s %.0f/130", L("STA", "\xe4\xbd\x93\xe5\x8a\x9b"), g_hud.sta);
    ImGui::Text("%s", buf);
    ImGui::ProgressBar(g_hud.sta / 130.0f, ImVec2(150, 0));
    _snprintf(buf, sizeof(buf), "%s %.2fx", L("Speed", "\xe9\x80\x9f\xe5\xba\xa6"), g_hud.spd);
    ImGui::Text("%s", buf);

    ImGui::End();
}
