// ==========================================================================
//  doluong - loadprobe, heartbeat - chi ghi log
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// loadprobe: ghi so edict trong N frame dau sau khi nap map, de bat dinh tam
// thoi. MOC CO SO ghi tai ServerActivate, luc do nhieu entity CHUA spawn xong.
// Xem docs/06-do-dac.md
static int  g_LoadProbe = 8;
static int  g_LoadProbeLeft = 0;
static int  g_LoadProbeFrame = 0;
static int  g_LoadProbePeak = 0;
// heartbeat: so GIAY giua hai lan ghi so lieu thuc the vao log. 0 = tat.
// Chi ghi log, khong dong vao entity nao. Xem HeartbeatSample().
static int  g_Heartbeat = 0;

// HEARTBEAT - ghi dinh ky so lieu thuc the vao log. Chi ghi, khong dong vao
// entity nao. Xem docs/06-do-dac.md
#define HB_MAX 96
static char  g_HbName[HB_MAX][40];
static int   g_HbCount[HB_MAX];
static int   g_HbN       = 0;
static bool  g_HbHasPrev = false;
static float g_HbNext    = 0.0f;

// Duyet moi entity (g_NextEnt, classname o +0x74) - cung cach duyet voi wipeclear va
// mapclear. Bang dem theo lop + chon top ben duoi cung kieu voi MapHistAdd (mapclear),
// FreeKeepTally (freegate), LogAllocFail (baycapphat). Moi ban viet rieng.
static void HeartbeatSample() {
    if (!g_NextEnt || !g_EntList) return;

    char  name[HB_MAX][40];
    int   cnt[HB_MAX];
    int   n = 0, live = 0;

    void* e = g_NextEnt(g_EntList, NULL, NULL);
    while (e) {
        live++;
        const char* cls = *(const char**)((uint8_t*)e + 0x74);
        if (cls) {
            int i = 0;
            for (; i < n; i++) if (strcmp(name[i], cls) == 0) { cnt[i]++; break; }
            if (i == n && n < HB_MAX) {
                strncpy(name[n], cls, sizeof(name[0])-1);
                name[n][sizeof(name[0])-1] = 0;
                cnt[n] = 1; n++;
            }
        }
        e = g_NextEnt(g_EntList, NULL, e);
    }

    int num  = g_num_edicts ? (int)*g_num_edicts : -1;
    int free = CountFreeEdicts();
    EL_LOG("[EdictBudget] NHIP: song=%d num_edicts=%d trong=%d bien do=%d lop=%d",
             live, num, free, NET_LIMIT - live, n);

    if (g_HbHasPrev) {
        // chenh lech theo lop, chi in cai co thay doi
        char dn[HB_MAX][40];
        int  dv[HB_MAX];
        int  dn_n = 0;
        for (int i = 0; i < n && dn_n < HB_MAX; i++) {
            int old = 0;
            for (int j = 0; j < g_HbN; j++)
                if (strcmp(g_HbName[j], name[i]) == 0) { old = g_HbCount[j]; break; }
            if (cnt[i] != old) {
                strncpy(dn[dn_n], name[i], sizeof(dn[0])-1);
                dn[dn_n][sizeof(dn[0])-1] = 0;
                dv[dn_n] = cnt[i] - old; dn_n++;
            }
        }
        for (int j = 0; j < g_HbN && dn_n < HB_MAX; j++) {
            bool still = false;
            for (int i = 0; i < n; i++) if (strcmp(name[i], g_HbName[j]) == 0) { still = true; break; }
            if (!still) {
                strncpy(dn[dn_n], g_HbName[j], sizeof(dn[0])-1);
                dn[dn_n][sizeof(dn[0])-1] = 0;
                dv[dn_n] = -g_HbCount[j]; dn_n++;
            }
        }
        for (int a = 0; a < dn_n && a < 12; a++) {
            int best = a;
            for (int b = a + 1; b < dn_n; b++) if (dv[b] > dv[best]) best = b;
            if (best != a) {
                int t = dv[a]; dv[a] = dv[best]; dv[best] = t;
                char tn[40]; strncpy(tn, dn[a], sizeof(tn)); tn[sizeof(tn)-1]=0;
                strncpy(dn[a], dn[best], sizeof(dn[0])); dn[a][sizeof(dn[0])-1]=0;
                strncpy(dn[best], tn, sizeof(dn[0])); dn[best][sizeof(dn[0])-1]=0;
            }
            if (dv[a] == 0) break;
            EL_LOG("[EdictBudget] NHIP:   %+5d  %s", dv[a], dn[a]);
        }
    }

    for (int i = 0; i < n; i++) {
        strncpy(g_HbName[i], name[i], sizeof(g_HbName[0])-1);
        g_HbName[i][sizeof(g_HbName[0])-1] = 0;
        g_HbCount[i] = cnt[i];
    }
    g_HbN = n; g_HbHasPrev = true;
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// Duong co so cho phep do WIPECLEAR: dem thuc the NGAY SAU khi nap map, truoc
// bat ky lan wipe nao. Chay ca khi wipeclear=0 - chi doc, khong dong gi.
static void DoLuong_OnServerActivate() {
    EL_LOG("[EdictBudget] MOC CO SO (sau nap map, chua wipe lan nao): "
             "num_edicts=%d slot trong=%d",
             g_num_edicts ? (int)*g_num_edicts : -1, CountFreeEdicts());
    // Bat dau lay mau tung frame. Nhieu entity chua spawn xong o thoi diem nay.
    g_LoadProbeLeft  = g_LoadProbe;
    g_LoadProbeFrame = 0;
    g_LoadProbePeak  = g_num_edicts ? (int)*g_num_edicts : 0;
}

// Lay mau TUNG FRAME ngay sau khi nap map. Doc hai so: num_edicts (moc cao nhat,
// KHONG BAO GIO GIAM) va so slot trong.   song = num_edicts - trong
// num_edicts tang ma trong cung tang  => co cap phat roi tra lai (dinh tam thoi)
// num_edicts tang ma trong dung yen   => entity that su duoc sinh them
static void LoadProbe_OnGameFrame() {
    if (g_LoadProbeLeft > 0) {
        int ne   = g_num_edicts ? (int)*g_num_edicts : -1;
        int free = CountFreeEdicts();
        if (ne > g_LoadProbePeak) g_LoadProbePeak = ne;
        EL_LOG("[EdictBudget] NAP[frame %d]: num_edicts=%d trong=%d song=%d (dinh=%d)",
                 g_LoadProbeFrame, ne, free, ne - free, g_LoadProbePeak);
        g_LoadProbeFrame++;
        if (--g_LoadProbeLeft == 0)
            EL_LOG("[EdictBudget] NAP: het %d frame lay mau, dinh num_edicts=%d",
                     g_LoadProbeFrame, g_LoadProbePeak);
    }
}

// Tra ve true o frame vua lay nhip - module chinh dung de goi bao cao freegate.
static bool Heartbeat_OnGameFrame() {
    if (g_Heartbeat > 0 && gpGlobals) {
        float now = gpGlobals->curtime;
        if (g_HbNext <= 0.0f) g_HbNext = now + (float)g_Heartbeat;
        else if (now >= g_HbNext) { g_HbNext = now + (float)g_Heartbeat; HeartbeatSample();
                                    return true; }
    }
    return false;
}

