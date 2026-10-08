// ==========================================================================
//  congtac - doc patches.txt - phai dung SAU moi module (gan cong tac cua tat ca)
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// Moi tinh nang co cong tac rieng trong patches.txt: ghi "ten=0" roi khoi dong lai
// la tat duoc tung cai de khoanh vung loi, khong phai build lai.
// Thieu file hoac thieu khoa => gia tri mac dinh cua tung module.

// CEF - DA GO KHOI KE HOACH. Ly do: docs/04-cef.md cua tag v2.0.2 (bo docs hien tai
// khong con trang nay) - doc truoc khi ai do them lai.

// Khoa cua cac tinh nang da GO khoi edictbudget (nay thuoc du an mo gioi han thuc
// the). Con bat trong patches.txt thi bao, khong im lang bo qua.
static bool KhoaDaTach(const char* k) {
    static const char* kDaTach[] = { "freetime", "indexbounds", "forcedindex", "bigarray",
        "snapshot", "detour", "pinmax", "pinglobals", "markfree", "nonethigh", "reuse", NULL };
    for (int i = 0; kDaTach[i]; i++) if (!_stricmp(k, kDaTach[i])) return true;
    return false;
}

static void LoadPatchSwitches() {
    FILE* f = OpenPluginFile("patches.txt", "r");
    // Khong tim thay = MOI cong tac ve mac dinh, im lang. Bay thuong gap nhat:
    // chep ban moi vao ma van giu ten thu muc cu. Thu muc PHAI la addons\edictbudget\.
    if (!f) {
        EL_LOG("[EdictBudget] !! KHONG DOC DUOC patches.txt - dung TOAN BO mac dinh !!");
        EL_LOG("[EdictBudget] !! thu muc phai la addons\\edictbudget\\ (khong phai ten cu) !!");
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || !*p) continue;

        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;

        char* end = eq - 1;                       // trim the key
        while (end >= p && (*end == ' ' || *end == '\t')) *end-- = 0;
        bool on = (atoi(eq + 1) != 0);

        if      (!_stricmp(p, "freegate"))    g_PatchFreeGate    = atoi(eq + 1);  // 0/1/2
        else if (!_stricmp(p, "wipeclear"))   g_WipeClear        = atoi(eq + 1);  // 0/1/2
        else if (!_stricmp(p, "trap"))        g_PatchTrap        = on;
        else if (!_stricmp(p, "nonetkill"))   g_NoNetKill        = on;
        else if (!_stricmp(p, "noedict"))     g_NoEdict          = on;
        else if (!_stricmp(p, "mapclear"))    g_MapClear         = atoi(eq + 1);  // 0/1/2
        else if (!_stricmp(p, "mapclearmax")) g_MapClearMax      = atoi(eq + 1);  // 0 = khong gioi han
        else if (!_stricmp(p, "mapclearcarry")) g_MapClearCarryOnly = on;         // 1 = chi xoa cai mang sang
        else if (!_stricmp(p, "loadprobe"))   g_LoadProbe        = atoi(eq + 1);  // so frame lay mau sau khi nap
        else if (!_stricmp(p, "swap"))        g_Swap             = atoi(eq + 1);  // 0/1/2, xem InstallSwap()
        else if (!_stricmp(p, "swapmax"))     g_SwapMax          = atoi(eq + 1);  // 0 = khong gioi han
        else if (!_stricmp(p, "spotlight"))   g_Spotlight        = atoi(eq + 1);  // 0/1/2, xem src/spotlight.cpp
        else if (!_stricmp(p, "heartbeat"))   g_Heartbeat        = atoi(eq + 1);  // giay, 0 = tat
        else if (!_stricmp(p, "logconsole"))  g_LogConsole       = on;
        else if (KhoaDaTach(p) && on)
            EL_LOG("[EdictBudget] !! patches.txt: '%s=1' thuoc nhom 4096 da TACH khoi "
                   "edictbudget - BO QUA", p);
    }
    fclose(f);

    EL_LOG("[EdictBudget] patches: freegate=%d wipeclear=%d trap=%d nonetkill=%d",
             g_PatchFreeGate, g_WipeClear, g_PatchTrap, g_NoNetKill);
}
