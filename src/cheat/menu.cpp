#include "globals.h"

static ImFont* g_fontZh = nullptr;

void InitImGui(IDirect3DDevice9* dev)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 6.0f;
    st.FrameRounding = 3.0f;
    st.Colors[ImGuiCol_TitleBg] = ImVec4(0.30f, 0.06f, 0.06f, 1.0f);
    st.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.42f, 0.08f, 0.08f, 1.0f);
    st.Colors[ImGuiCol_Tab] = ImVec4(0.28f, 0.08f, 0.08f, 1.0f);
    st.Colors[ImGuiCol_TabHovered] = ImVec4(0.45f, 0.12f, 0.12f, 1.0f);
    st.Colors[ImGuiCol_TabActive] = ImVec4(0.55f, 0.14f, 0.14f, 1.0f);
    st.Colors[ImGuiCol_Button] = ImVec4(0.32f, 0.10f, 0.10f, 1.0f);
    st.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.14f, 0.14f, 1.0f);
    st.Colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.18f, 0.18f, 1.0f);
    st.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    st.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.06f, 0.06f, 0.8f);
    st.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.10f, 0.10f, 0.9f);
    st.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.32f, 0.12f, 0.12f, 1.0f);
    st.Colors[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
    st.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    st.Colors[ImGuiCol_Header] = ImVec4(0.40f, 0.12f, 0.12f, 1.0f);
    st.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.15f, 0.15f, 1.0f);
    st.Colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.18f, 0.18f, 1.0f);
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.08f, 0.96f);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    const char* fonts[] = {
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simsun.ttc",
    };
    for (int i = 0; i < 3; i++)
    {
        // AddFontFromFileTTF asserts when the file is absent. Windows font
        // sets differ by locale, so probe each optional Chinese font first.
        if (GetFileAttributesA(fonts[i]) == INVALID_FILE_ATTRIBUTES)
            continue;
        g_fontZh = io.Fonts->AddFontFromFileTTF(fonts[i], 16.0f, nullptr,
            io.Fonts->GetGlyphRangesChineseFull());
        if (g_fontZh) break;
    }

    bool rendererOk = ImGui_ImplDX9_Init(dev);
    bool platformOk = g_gameHwnd && ImGui_ImplWin32_Init(g_gameHwnd);
    g_imguiReady = rendererOk && platformOk;
}

static void TabAimbot()
{
    ImGui::Checkbox(L("Enabled", "\xe5\x90\xaf\xe7\x94\xa8"), &g_cfg.aimbot.enabled);
    ImGui::Checkbox(L("Show FOV circle", "\xe6\x98\xbe\xe7\xa4\xba FOV \xe5\x9c\x86\xe5\x9c\x88"), &g_cfg.aimbot.showFov);
    ImGui::SliderFloat(L("FOV", "\xe8\xa7\x86\xe9\x87\x8e\xe8\x8c\x83\xe5\x9b\xb4"), &g_cfg.aimbot.fov, 1.0f, 60.0f, "%.0f");
    ImGui::SliderFloat(L("Smooth", "\xe5\xb9\xb3\xe6\xbb\x91\xe5\xba\xa6"), &g_cfg.aimbot.smooth, 1.0f, 15.0f, "%.1f");
    ImGui::Checkbox(L("Aim at infected players", "\xe7\x9e\x84\xe5\x87\x86\xe6\x84\x9f\xe6\x9f\x93\xe8\x80\x85"), &g_cfg.aimbot.players);
    ImGui::Checkbox(L("Aim at any player (PVP)", "\xe7\x9e\x84\xe5\x87\x86\xe4\xbb\xbb\xe4\xbd\x95\xe7\x8e\xa9\xe5\xae\xb6 (PVP)"), &g_cfg.aimbot.anyPlayer);
    ImGui::Checkbox(L("Aim at zombies", "\xe7\x9e\x84\xe5\x87\x86\xe5\x83\xb5\xe5\xb0\xb8"), &g_cfg.aimbot.zombies);
    char keybuf[64];
    if (g_capturing == 1)
        _snprintf(keybuf, sizeof(keybuf), L("Press a key... (ESC to cancel)", "\xe8\xaf\xb7\xe6\x8c\x89\xe9\x94\xae... (ESC \xe5\x8f\x96\xe6\xb6\x88)"));
    else
        _snprintf(keybuf, sizeof(keybuf), "%s: %s", L("Aim key", "\xe7\x9e\x84\xe5\x87\x86\xe9\x94\xae"), VkName(g_cfg.aimbot.key));
    if (ImGui::Button(keybuf, ImVec2(220, 24)))
        g_capturing = 1;
}

static void TabEsp()
{
    ImGui::Checkbox(L("ESP Enabled", "\xe9\x80\x8f\xe8\xa7\x86\xe5\x90\xaf\xe7\x94\xa8"), &g_cfg.esp.enabled);
    ImGui::SliderFloat(L("Draw range (m)", "\xe7\xbb\x98\xe5\x88\xb6\xe8\xb7\x9d\xe7\xa6\xbb (m)"), &g_cfg.esp.range, 5.0f, 300.0f, "%.0f");
    ImGui::SliderFloat(L("X offset", "X \xe5\x81\x8f\xe7\xa7\xbb"), &g_cfg.esp.xOffset, -600.0f, 600.0f, "%.0f");
    ImGui::SliderFloat(L("Y offset", "Y \xe5\x81\x8f\xe7\xa7\xbb"), &g_cfg.esp.yOffset, -600.0f, 600.0f, "%.0f");
    char status[64];
    _snprintf(status, sizeof(status), L("Targets in range: %d", "\xe8\x8c\x83\xe5\x9b\xb4\xe5\x86\x85\xe7\x9b\xae\xe6\xa0\x87: %d"), g_espCount);
    ImGui::Text("%s", status);
    ImGui::Separator();

    const char* cats[5] = {
        L("Teammates", "\xe9\x98\x9f\xe5\x8f\x8b"),
        L("Infected", "\xe6\x84\x9f\xe6\x9f\x93\xe8\x80\x85"),
        L("Zombies", "\xe5\x83\xb5\xe5\xb0\xb8"),
        L("Items", "\xe7\x89\xa9\xe5\x93\x81"),
        L("Objectives", "\xe7\x9b\xae\xe6\xa0\x87"),
    };
    bool* enab[5] = { &g_cfg.esp.teammates, &g_cfg.esp.infected, &g_cfg.esp.zombies, &g_cfg.esp.items, &g_cfg.esp.objectives };
    float* clrs[5] = { g_cfg.esp.clr_team, g_cfg.esp.clr_inf, g_cfg.esp.clr_zombie, g_cfg.esp.clr_item, g_cfg.esp.clr_obj };

    for (int i = 0; i < 5; i++)
    {
        ImGui::PushID(i);
        ImGui::Checkbox(cats[i], enab[i]);
        ImGui::SameLine(140);
        ImGui::Checkbox(L("Box", "\xe6\x96\xb9\xe6\xa1\x86"), &g_cfg.esp.catBox[i]);
        ImGui::SameLine(215);
        ImGui::Checkbox(L("Name", "\xe5\x90\x8d\xe7\xa7\xb0"), &g_cfg.esp.catName[i]);
        ImGui::SameLine(290);
        ImGui::Checkbox(L("Dist", "\xe8\xb7\x9d\xe7\xa6\xbb"), &g_cfg.esp.catDist[i]);
        ImGui::SameLine(365);
        ImGui::Checkbox(L("HP", "\xe8\xa1\x80"), &g_cfg.esp.catHp[i]);
        ImGui::SameLine(430);
        ImGui::ColorEdit3("Color", clrs[i], ImGuiColorEditFlags_NoInputs);
        ImGui::PopID();
    }
}

static void TabMisc()
{
    ImGui::BeginDisabled(true);
    ImGui::Checkbox(L("Infinite stamina (awaiting verified offset)", "\xe6\x97\xa0\xe9\x99\x90\xe4\xbd\x93\xe5\x8a\x9b (\xe7\xad\x89\xe5\xbe\x85\xe5\x81\x8f\xe7\xa7\xbb\xe9\xaa\x8c\xe8\xaf\x81)"), &g_cfg.misc.stamina);
    ImGui::EndDisabled();
    float spd = g_cfg.misc.speed;
    if (ImGui::SliderFloat(L("Speed", "\xe9\x80\x9f\xe5\xba\xa6"), &spd, 0.5f, 3.0f, "%.2fx"))
        g_cfg.misc.speed = spd;
    ImGui::Checkbox(L("God mode", "\xe6\x97\xa0\xe6\x95\x8c\xe6\xa8\xa1\xe5\xbc\x8f"), &g_cfg.misc.god);
    ImGui::Checkbox(L("Infinite ammo", "\xe6\x97\xa0\xe9\x99\x90\xe5\xad\x90\xe5\xbc\xb9"), &g_cfg.misc.ammo);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
        L("God mode and ammo are server-authoritative,\nmay not work on dedicated servers.",
          "\xe6\x97\xa0\xe6\x95\x8c\xe4\xb8\x8e\xe6\x97\xa0\xe9\x99\x90\xe5\xad\x90\xe5\xbc\xb9\xe7\x94\xb1\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x86\xb3\xe5\xae\x9a\xef\xbc\x8c\n\xe4\xb8\x93\xe7\x94\xa8\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xb8\x8a\xe5\x8f\xaf\xe8\x83\xbd\xe6\x97\xa0\xe6\x95\x88\xe3\x80\x82"));
    ImGui::Checkbox(L("Bunny hop", "\xe8\xbf\x9e\xe8\xb7\xb3"), &g_cfg.misc.bhop);
    ImGui::Checkbox(L("Status HUD", "\xe7\x8a\xb6\xe6\x80\x81\xe6\xa0\x8f"), &g_cfg.misc.hud);
    ImGui::SliderFloat(L("Brightness", "\xe4\xba\xae\xe5\xba\xa6"), &g_cfg.misc.brightness, 0.0f, 100.0f, "%.0f");
    ImGui::Separator();
    if (ImGui::Button(L("All on", "\xe5\x85\xa8\xe9\x83\xa8\xe5\xbc\x80\xe5\x90\xaf"), ImVec2(90, 24)))
    {
        g_cfg.aimbot.enabled = true;
        g_cfg.esp.enabled = true;
        g_cfg.esp.teammates = true;
        g_cfg.esp.infected = true;
        g_cfg.esp.zombies = true;
        g_cfg.esp.items = true;
        for (int i = 0; i < 5; i++)
        {
            g_cfg.esp.catBox[i] = true;
            g_cfg.esp.catName[i] = true;
            g_cfg.esp.catDist[i] = true;
            g_cfg.esp.catHp[i] = i < 3;
        }
        g_cfg.misc.god = true;
        g_cfg.misc.bhop = true;
        g_cfg.misc.ammo = true;
        g_cfg.misc.hud = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(L("All off", "\xe5\x85\xa8\xe9\x83\xa8\xe5\x85\xb3\xe9\x97\xad"), ImVec2(90, 24)))
    {
        g_cfg.aimbot.enabled = false;
        g_cfg.esp.enabled = false;
        g_cfg.esp.teammates = false;
        g_cfg.esp.infected = false;
        g_cfg.esp.zombies = false;
        g_cfg.esp.items = false;
        g_cfg.esp.objectives = false;
        g_cfg.misc.stamina = false;
        g_cfg.misc.god = false;
        g_cfg.misc.bhop = false;
        g_cfg.misc.ammo = false;
        g_cfg.misc.hud = false;
        g_cfg.exploit.svbypass = false;
        g_cfg.misc.speed = 1.0f;
        g_cfg.misc.brightness = 0.0f;
    }
}

static void TabExploit()
{
    ImGui::Checkbox(L("sv_cheats bypass", "sv_cheats \xe7\xbb\x95\xe8\xbf\x87"), &g_cfg.exploit.svbypass);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
        L("Writes the sv_cheats flag unlock into engine.dll.\nFrom the known NMRiH bypass tool - use at your own risk.",
          "\xe5\x86\x99\xe5\x85\xa5 engine.dll \xe4\xb8\xad\xe7\x9a\x84 sv_cheats \xe8\xa7\xa3\xe9\x94\x81\xe6\xa0\x87\xe5\xbf\x97\xe3\x80\x82\n\xe6\x9d\xa5\xe8\x87\xaa\xe5\xb7\xb2\xe7\x9f\xa5\xe7\x9a\x84 NMRiH \xe7\xbb\x95\xe8\xbf\x87\xe5\xb7\xa5\xe5\x85\xb7 - \xe9\xa3\x8e\xe9\x99\xa9\xe8\x87\xaa\xe6\x8b\x85\xe3\x80\x82"));
}

static void TabSettings()
{
    ImGui::Text(L("Language", "\xe8\xaf\xad\xe8\xa8\x80"));
    if (ImGui::Button(L("English", "\xe4\xb8\xad\xe6\x96\x87"), ImVec2(110, 24)))
        g_cfg.lang = g_cfg.lang == 0 ? 1 : 0;
    ImGui::Separator();
    char keybuf[64];
    if (g_capturing == 2)
        _snprintf(keybuf, sizeof(keybuf), L("Press a key... (ESC to cancel)", "\xe8\xaf\xb7\xe6\x8c\x89\xe9\x94\xae... (ESC \xe5\x8f\x96\xe6\xb6\x88)"));
    else
        _snprintf(keybuf, sizeof(keybuf), "%s: %s", L("Menu key", "\xe8\x8f\x9c\xe5\x8d\x95\xe9\x94\xae"), VkName(g_cfg.menuKey));
    if (ImGui::Button(keybuf, ImVec2(140, 24)))
        g_capturing = 2;
    ImGui::Separator();
    if (ImGui::Button(L("Save config", "\xe4\xbf\x9d\xe5\xad\x98\xe9\x85\x8d\xe7\xbd\xae"), ImVec2(140, 24)))
        SaveConfig();
    ImGui::SameLine();
    if (ImGui::Button(L("Load config", "\xe8\xaf\xbb\xe5\x8f\x96\xe9\x85\x8d\xe7\xbd\xae"), ImVec2(140, 24)))
        LoadConfig();
    ImGui::Separator();
    if (ImGui::Button(L("Unload cheat (also: END key)", "\xe5\x8d\xb8\xe8\xbd\xbd\xe4\xbf\xae\xe6\x94\xb9\xe5\x99\xa8 (\xe4\xb9\x9f\xe5\x8f\xaf\xe6\x8c\x89 END)"), ImVec2(220, 24)))
        RestoreAndUnload();
    ImGui::Separator();
    ImGui::Text(L("Menu key: %s   |   Unload: END", "\xe8\x8f\x9c\xe5\x8d\x95\xe9\x94\xae: %s   |   \xe5\x8d\xb8\xe8\xbd\xbd: END"), VkName(g_cfg.menuKey));
    ImGui::Text("NMRiHCheat v1.2");
}

void MenuRender()
{
    if (!g_menuOpen) return;

    if (g_cfg.lang == 1 && g_fontZh) ImGui::PushFont(g_fontZh);

    ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_FirstUseEver);
    ImGui::Begin("NMRIH Trainer", &g_menuOpen, ImGuiWindowFlags_NoCollapse);
#ifdef _WIN64
    ImGui::TextWrapped("Compatibility test: game features are disabled until the new offsets and interfaces are verified.");
#endif

    if (ImGui::BeginTabBar("##tabs"))
    {
        if (ImGui::BeginTabItem(L("Aimbot", "\xe8\x87\xaa\xe7\x9e\x84")))
        {
            TabAimbot();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L("ESP", "\xe9\x80\x8f\xe8\xa7\x86")))
        {
            TabEsp();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L("Misc", "\xe6\x9d\x82\xe9\xa1\xb9")))
        {
            TabMisc();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L("Exploit", "\xe5\xbc\x80\xe5\x8f\x91")))
        {
            TabExploit();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L("Settings", "\xe8\xae\xbe\xe7\xbd\xae")))
        {
            TabSettings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    if (g_cfg.lang == 1 && g_fontZh) ImGui::PopFont();
}
