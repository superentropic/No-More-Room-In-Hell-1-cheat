#include "globals.h"

static char g_iniPath[MAX_PATH] = {};

static const char* ConfigPath()
{
    if (!g_iniPath[0])
    {
        HMODULE self = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ConfigPath, &self))
        {
            GetModuleFileNameA(self, g_iniPath, sizeof(g_iniPath));
            char* slash = strrchr(g_iniPath, '\\');
            if (slash)
            {
                slash[1] = 0;
                strncat(g_iniPath, "nmrih_trainer.ini", sizeof(g_iniPath) - strlen(g_iniPath) - 1);
            }
        }
    }
    return g_iniPath;
}

static int ReadInt(const char* sec, const char* key, int def)
{
    return (int)GetPrivateProfileIntA(sec, key, def, ConfigPath());
}

static float ReadFloat(const char* sec, const char* key, float def)
{
    char buf[64];
    GetPrivateProfileStringA(sec, key, "", buf, sizeof(buf), ConfigPath());
    if (!buf[0]) return def;
    return (float)atof(buf);
}

static void WriteInt(const char* sec, const char* key, int val)
{
    char buf[32];
    _snprintf(buf, sizeof(buf), "%d", val);
    WritePrivateProfileStringA(sec, key, buf, ConfigPath());
}

static void WriteFloat(const char* sec, const char* key, float val)
{
    char buf[32];
    _snprintf(buf, sizeof(buf), "%f", val);
    WritePrivateProfileStringA(sec, key, buf, ConfigPath());
}

void LoadConfig()
{
    if (ReadInt("settings", "version", 0) != 2)
    {
        g_cfg = Cfg{};
        return;
    }
    g_cfg.lang = ReadInt("settings", "lang", 0);
    g_cfg.menuKey = ReadInt("settings", "menu_key", VK_INSERT);
    g_cfg.aimbot.enabled = ReadInt("aimbot", "enabled", 0) != 0;
    g_cfg.aimbot.showFov = ReadInt("aimbot", "show_fov", 1) != 0;
    g_cfg.aimbot.fov = ReadFloat("aimbot", "fov", 15.0f);
    g_cfg.aimbot.smooth = ReadFloat("aimbot", "smooth", 3.0f);
    g_cfg.aimbot.players = ReadInt("aimbot", "players", 0) != 0;
    g_cfg.aimbot.anyPlayer = ReadInt("aimbot", "any_player", 0) != 0;
    g_cfg.aimbot.zombies = ReadInt("aimbot", "zombies", 0) != 0;
    g_cfg.aimbot.key = ReadInt("aimbot", "key", VK_MENU);
    g_cfg.esp.enabled = ReadInt("esp", "enabled", 0) != 0;
    g_cfg.esp.range = ReadFloat("esp", "range", 80.0f);
    g_cfg.esp.xOffset = ReadFloat("esp", "x_offset", 0.0f);
    g_cfg.esp.yOffset = ReadFloat("esp", "y_offset", 0.0f);
    g_cfg.esp.teammates = ReadInt("esp", "teammates", 0) != 0;
    g_cfg.esp.infected = ReadInt("esp", "infected", 0) != 0;
    g_cfg.esp.zombies = ReadInt("esp", "zombies", 0) != 0;
    g_cfg.esp.items = ReadInt("esp", "items", 0) != 0;
    g_cfg.esp.objectives = ReadInt("esp", "objectives", 0) != 0;
    for (int i = 0; i < 5; i++)
    {
        char key[32];
        _snprintf(key, sizeof(key), "cat_box_%d", i);
        g_cfg.esp.catBox[i] = ReadInt("esp", key, 0) != 0;
        _snprintf(key, sizeof(key), "cat_name_%d", i);
        g_cfg.esp.catName[i] = ReadInt("esp", key, 0) != 0;
        _snprintf(key, sizeof(key), "cat_dist_%d", i);
        g_cfg.esp.catDist[i] = ReadInt("esp", key, 0) != 0;
        _snprintf(key, sizeof(key), "cat_hp_%d", i);
        g_cfg.esp.catHp[i] = ReadInt("esp", key, 0) != 0;
    }
    g_cfg.esp.clr_team[0] = ReadFloat("esp", "clr_team_r", 0.0f);
    g_cfg.esp.clr_team[1] = ReadFloat("esp", "clr_team_g", 1.0f);
    g_cfg.esp.clr_team[2] = ReadFloat("esp", "clr_team_b", 0.0f);
    g_cfg.esp.clr_inf[0] = ReadFloat("esp", "clr_inf_r", 1.0f);
    g_cfg.esp.clr_inf[1] = ReadFloat("esp", "clr_inf_g", 1.0f);
    g_cfg.esp.clr_inf[2] = ReadFloat("esp", "clr_inf_b", 0.0f);
    g_cfg.esp.clr_zombie[0] = ReadFloat("esp", "clr_zombie_r", 1.0f);
    g_cfg.esp.clr_zombie[1] = ReadFloat("esp", "clr_zombie_g", 0.0f);
    g_cfg.esp.clr_zombie[2] = ReadFloat("esp", "clr_zombie_b", 0.0f);
    g_cfg.esp.clr_item[0] = ReadFloat("esp", "clr_item_r", 0.6f);
    g_cfg.esp.clr_item[1] = ReadFloat("esp", "clr_item_g", 0.8f);
    g_cfg.esp.clr_item[2] = ReadFloat("esp", "clr_item_b", 1.0f);
    g_cfg.esp.clr_obj[0] = ReadFloat("esp", "clr_obj_r", 1.0f);
    g_cfg.esp.clr_obj[1] = ReadFloat("esp", "clr_obj_g", 1.0f);
    g_cfg.esp.clr_obj[2] = ReadFloat("esp", "clr_obj_b", 1.0f);
    g_cfg.misc.stamina = ReadInt("misc", "stamina", 0) != 0;
    g_cfg.misc.speed = ReadFloat("misc", "speed", 1.0f);
    g_cfg.misc.god = ReadInt("misc", "god", 0) != 0;
    g_cfg.misc.bhop = ReadInt("misc", "bhop", 0) != 0;
    g_cfg.misc.ammo = ReadInt("misc", "ammo", 0) != 0;
    g_cfg.misc.hud = ReadInt("misc", "hud", 0) != 0;
    g_cfg.misc.brightness = ReadFloat("misc", "brightness", 0.0f);
    g_cfg.exploit.svbypass = ReadInt("exploit", "svbypass", 0) != 0;
}

void SaveConfig()
{
    WriteInt("settings", "version", 2);
    WriteInt("settings", "lang", g_cfg.lang);
    WriteInt("settings", "menu_key", g_cfg.menuKey);
    WriteInt("aimbot", "enabled", g_cfg.aimbot.enabled);
    WriteInt("aimbot", "show_fov", g_cfg.aimbot.showFov);
    WriteFloat("aimbot", "fov", g_cfg.aimbot.fov);
    WriteFloat("aimbot", "smooth", g_cfg.aimbot.smooth);
    WriteInt("aimbot", "players", g_cfg.aimbot.players);
    WriteInt("aimbot", "any_player", g_cfg.aimbot.anyPlayer);
    WriteInt("aimbot", "zombies", g_cfg.aimbot.zombies);
    WriteInt("aimbot", "key", g_cfg.aimbot.key);
    WriteInt("esp", "enabled", g_cfg.esp.enabled);
    WriteFloat("esp", "range", g_cfg.esp.range);
    WriteFloat("esp", "x_offset", g_cfg.esp.xOffset);
    WriteFloat("esp", "y_offset", g_cfg.esp.yOffset);
    WriteInt("esp", "teammates", g_cfg.esp.teammates);
    WriteInt("esp", "infected", g_cfg.esp.infected);
    WriteInt("esp", "zombies", g_cfg.esp.zombies);
    WriteInt("esp", "items", g_cfg.esp.items);
    WriteInt("esp", "objectives", g_cfg.esp.objectives);
    for (int i = 0; i < 5; i++)
    {
        char key[32];
        _snprintf(key, sizeof(key), "cat_box_%d", i);
        WriteInt("esp", key, g_cfg.esp.catBox[i]);
        _snprintf(key, sizeof(key), "cat_name_%d", i);
        WriteInt("esp", key, g_cfg.esp.catName[i]);
        _snprintf(key, sizeof(key), "cat_dist_%d", i);
        WriteInt("esp", key, g_cfg.esp.catDist[i]);
        _snprintf(key, sizeof(key), "cat_hp_%d", i);
        WriteInt("esp", key, g_cfg.esp.catHp[i]);
    }
    WriteFloat("esp", "clr_team_r", g_cfg.esp.clr_team[0]);
    WriteFloat("esp", "clr_team_g", g_cfg.esp.clr_team[1]);
    WriteFloat("esp", "clr_team_b", g_cfg.esp.clr_team[2]);
    WriteFloat("esp", "clr_inf_r", g_cfg.esp.clr_inf[0]);
    WriteFloat("esp", "clr_inf_g", g_cfg.esp.clr_inf[1]);
    WriteFloat("esp", "clr_inf_b", g_cfg.esp.clr_inf[2]);
    WriteFloat("esp", "clr_zombie_r", g_cfg.esp.clr_zombie[0]);
    WriteFloat("esp", "clr_zombie_g", g_cfg.esp.clr_zombie[1]);
    WriteFloat("esp", "clr_zombie_b", g_cfg.esp.clr_zombie[2]);
    WriteFloat("esp", "clr_item_r", g_cfg.esp.clr_item[0]);
    WriteFloat("esp", "clr_item_g", g_cfg.esp.clr_item[1]);
    WriteFloat("esp", "clr_item_b", g_cfg.esp.clr_item[2]);
    WriteFloat("esp", "clr_obj_r", g_cfg.esp.clr_obj[0]);
    WriteFloat("esp", "clr_obj_g", g_cfg.esp.clr_obj[1]);
    WriteFloat("esp", "clr_obj_b", g_cfg.esp.clr_obj[2]);
    WriteInt("misc", "stamina", g_cfg.misc.stamina);
    WriteFloat("misc", "speed", g_cfg.misc.speed);
    WriteInt("misc", "god", g_cfg.misc.god);
    WriteInt("misc", "bhop", g_cfg.misc.bhop);
    WriteInt("misc", "ammo", g_cfg.misc.ammo);
    WriteInt("misc", "hud", g_cfg.misc.hud);
    WriteFloat("misc", "brightness", g_cfg.misc.brightness);
    WriteInt("exploit", "svbypass", g_cfg.exploit.svbypass);
}
