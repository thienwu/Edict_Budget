// ==========================================================================
//  wipeclear - don entity dau RestartRound khi wipe
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// wipeclear: don thuc the o dau CTerrorGameRules::RestartRound (vtable slot 178),
// TRUOC vong hoi sinh nguoi choi. Xem docs/01-co-che.md muc wipeclear.
static int g_WipeClear = 0;

// WIPECLEAR - don thuc the o DAU chuoi restart, TRUOC vong hoi sinh player.
// Dung nguyen preserve list 38 lop cua game. Xem docs/01-co-che.md

// Nghe su kien: CHI DE CHAN DOAN, khong con quyen chan. Xem docs/01-co-che.md
static bool  g_LossPending   = false;
static float g_LastLossTime  = -1.0f;
static char  g_LastLossName[32] = "";
static int   g_LossSignals   = 0;

// WIPE = doi survivor THUA = su kien 'mission_lost'.
// CHI 'mission_lost' duoc mo cong. 'round_end' cung ban khi THANG (qua chuong,
// finale) nen chi ghi log de doi chieu thu tu, KHONG mo cong.
class CLossListener : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *ev) {
        if (!ev) return;
        const char* n = ev->GetName();
        if (!n) return;
        float t = gpGlobals ? gpGlobals->curtime : 0.0f;

        if (_stricmp(n, "mission_lost") == 0) {
            g_LossPending  = true;
            g_LastLossTime = t;
            strncpy(g_LastLossName, n, sizeof(g_LastLossName) - 1);
            g_LastLossName[sizeof(g_LastLossName) - 1] = 0;
            g_LossSignals++;
            EL_LOG("[EdictBudget] WIPECLEAR: *** WIPE (mission_lost) *** t=%.2f "
                     "(lan thu %d) - MO CONG", t, g_LossSignals);
        } else {
            // chi doi chieu thu tu, khong mo cong
            EL_LOG("[EdictBudget] WIPECLEAR: (doi chieu) su kien '%s' t=%.2f "
                     "- KHONG mo cong", n, t);
        }
    }
    virtual int GetEventDebugID() { return 42; }
};
static CLossListener g_LossListener;
static bool g_LossHooked = false;

typedef void  (__fastcall *fnRestartRound_t)(void*, void*);
static fnRestartRound_t      g_OrigRestartRound  = NULL;
static void**                g_RestartSlot       = NULL;
static int                   g_WipeClearRuns     = 0;

// Danh sach giu bo sung - wipekeep.txt. DE TRONG moi dung: o wipe, entity bi
// xoa se DUOC DUNG LAI tu entity lump. Xem docs/01-co-che.md
#define KEEP_MAX 128
static char g_KeepExtra[KEEP_MAX][48];
static int  g_KeepCount = 0;

// Bo doc danh sach mot-lop - mot trong sau ban chep (xem LoadNoEdictList, noedict.cpp).
static void LoadWipeKeep() {
    g_KeepCount = 0;
    FILE* f = OpenPluginFile("wipekeep.txt", "r");
    if (!f) {
        EL_LOG("[EdictBudget] WIPECLEAR: khong co wipekeep.txt - chi dung preserve list cua game");
        return;
    }
    // Buffer PHAI du rong, va phai xa phan du neu dong dai hon buffer.
    // line[64] lam dong chu thich dai bi cat doi; phan duoi khong
    // bat dau bang '#' nen lot vao danh sach nhu mot ten lop rac.
    // File 15 dong ma nap ra 31 muc.
    char line[256];
    while (fgets(line, sizeof(line), f) && g_KeepCount < KEEP_MAX) {
        size_t L = strlen(line);
        if (L > 0 && line[L - 1] != '\n') {          // dong bi cat -> xa het dong
            int c; while ((c = fgetc(f)) != EOF && c != '\n') {}
        }
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
        char* e = p + strlen(p) - 1;
        while (e >= p && (*e == '\r' || *e == '\n' || *e == ' ' || *e == '\t')) *e-- = 0;
        if (!*p) continue;
        // Chi nhan ky tu hop le cho classname - chan not moi thu rac con sot
        bool ok = true;
        for (const char* q = p; *q; q++) {
            if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                  (*q >= '0' && *q <= '9') || *q == '_')) { ok = false; break; }
        }
        if (!ok) {
            EL_LOG("[EdictBudget] WIPECLEAR: wipekeep.txt bo qua dong khong hop le: '%s'", p);
            continue;
        }
        strncpy(g_KeepExtra[g_KeepCount], p, sizeof(g_KeepExtra[0]) - 1);
        g_KeepExtra[g_KeepCount][sizeof(g_KeepExtra[0]) - 1] = 0;
        g_KeepCount++;
    }
    fclose(f);

    for (int i = 0; i < g_KeepCount; i++)
        EL_LOG("[EdictBudget] WIPECLEAR:   giu them [%d] '%s'", i, g_KeepExtra[i]);
    EL_LOG("[EdictBudget] WIPECLEAR: wipekeep.txt - giu them %d lop", g_KeepCount);
}

// Cung luat khop "ten_" = tien to voi InFreeKeep (freegate), InMapKeep (mapclear).
static bool InKeepExtra(const char* cls) {
    if (g_KeepCount <= 0 || !cls || !*cls) return false;
    for (int i = 0; i < g_KeepCount; i++) {
        const char* k = g_KeepExtra[i];
        size_t n = strlen(k);
        if (n == 0) continue;
        if (k[n - 1] == '_') {                       // khop tien to ca ho
            if (strncmp(cls, k, n) == 0) return true;
        } else if (strcmp(cls, k) == 0) {            // khop chinh xac
            return true;
        }
    }
    return false;
}

static void __fastcall Hook_RestartRound(void* thisptr, void* edx) {
    if (!g_OrigRestartRound) return;

// Cong chan wipeclear (khoi phuc 07/08 sau khi do that te). docs/01-co-che.md
    float now = gpGlobals ? gpGlobals->curtime : 0.0f;
    float ago = (g_LastLossTime >= 0.0f) ? (now - g_LastLossTime) : -1.0f;

    EL_LOG("[EdictBudget] WIPECLEAR: RestartRound goi luc t=%.2f | "
             "mission_lost gan nhat: %s (cach %.2fs, tong %d) | cong: %s | slot trong=%d",
             now, g_LastLossName[0] ? g_LastLossName : "(chua bao gio ban)",
             ago, g_LossSignals, g_LossPending ? "MO -> SE DON" : "DONG -> BO QUA",
             CountFreeEdicts());

    if (!g_LossPending) { g_OrigRestartRound(thisptr, edx); return; }
    g_LossPending = false;               // dung mot lan cho moi wipe

    // wipeclear=1: chi quan sat. Van log day du hai dau moc quanh RestartRound
    // de biet num_edicts cham 2048 truoc hay sau, nhung KHONG xoa gi.
    if (g_WipeClear < 2) {
        EL_LOG("[EdictBudget] WIPECLEAR[quan sat]: TRUOC RestartRound "
                 "slot trong=%d num_edicts=%d", CountFreeEdicts(),
                 g_num_edicts ? (int)*g_num_edicts : -1);
        g_OrigRestartRound(thisptr, edx);
        EL_LOG("[EdictBudget] WIPECLEAR[quan sat]: SAU RestartRound "
                 "slot trong=%d num_edicts=%d", CountFreeEdicts(),
                 g_num_edicts ? (int)*g_num_edicts : -1);
        return;
    }

    // Tu day xuong la KHUNG DON dung chung y tuong voi Hook_PrepChangelevel
    // (mapclear.cpp): duyet moi entity -> giu preserve list + file giu rieng ->
    // UTIL_Remove -> CleanupDeleteList -> AllowImmediateEdictReuse. Hai ban viet rieng.
    int before = CountFreeEdicts();
    int removed = 0, kept = 0, keptExtra = 0;
    bool did = false;

    // Tai nhap: CleanupDeleteList KHONG co bao ve tai nhap tren ban retail
    // (g_fInCleanupDelete chi duoc doc trong #ifdef DEBUG). Phai tu kiem.
    if (g_InCleanupDelete && *g_InCleanupDelete) {
        EL_LOG("[EdictBudget] WIPECLEAR: dang trong CleanupDeleteList -> bo qua luot nay");
    } else if (g_NextEnt && g_UtilRemove && g_EntList && g_CleanupDeleteList) {
        void* e = g_NextEnt(g_EntList, NULL, NULL);          // FirstEnt = NextEnt(NULL)
        while (e) {
            void* next = g_NextEnt(g_EntList, NULL, e);      // lay truoc khi go
            const char* cls = *(const char**)((uint8_t*)e + 0x74);   // m_iClassname
            if (InPreserveList(cls))      kept++;            // game von giu
            else if (InKeepExtra(cls))  { kept++; keptExtra++; }  // ta giu them
            else { g_UtilRemove(e); removed++; }
            e = next;
        }
        g_CleanupDeleteList(g_EntList, NULL);                // tra edict ngay
        if (engine) engine->AllowImmediateEdictReuse();      // bo thoi gian cho
        did = true;
    }

    int after = CountFreeEdicts();
    g_WipeClearRuns++;
    EL_LOG("[EdictBudget] WIPECLEAR #%d %s: slot trong %d -> %d (chenh %d) | "
             "go %d, giu %d (trong do %d do wipekeep.txt) | num_edicts=%d",
             g_WipeClearRuns, did ? "DA DON" : "BO QUA", before, after,
             (before >= 0 && after >= 0) ? (after - before) : -1,
             removed, kept, keptExtra, g_num_edicts ? (int)*g_num_edicts : -1);

    g_OrigRestartRound(thisptr, edx);

    EL_LOG("[EdictBudget] WIPECLEAR #%d: SAU RestartRound slot trong=%d num_edicts=%d",
             g_WipeClearRuns, CountFreeEdicts(), g_num_edicts ? (int)*g_num_edicts : -1);
}

static bool InstallWipeClear() {
    if (g_OrigRestartRound) return true;              // da cai roi
    if (g_WipeClear <= 0) return false;               // 0 = no-op that su, khong dung vtable

    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* b = (uint8_t*)mi.lpBaseOfDll;

    // Xac minh prologue RestartRound truoc khi dong vao vtable.
    // 0x2E0650: 55 8B EC 0F 57 C0 83 EC 08 53 33 DB 56 57 8B F9
    static const uint8_t kSig[] = {
        0x55,0x8B,0xEC,0x0F,0x57,0xC0,0x83,0xEC,0x08,0x53,0x33,0xDB,0x56,0x57,0x8B,0xF9 };
    uint8_t* fn = b + 0x2E0650;
    if (memcmp(fn, kSig, sizeof(kSig)) != 0) {
        EL_LOG("[EdictBudget] WIPECLEAR: prologue RestartRound KHONG khop "
                 "- server.dll khac ban? Bo qua, khong dong gi.");
        return false;
    }

    void*** grPtr = (void***)(b + 0x7F7F6C);          // g_pGameRules
    void** gr = *grPtr;
    if (!gr) { EL_LOG("[EdictBudget] WIPECLEAR: g_pGameRules con NULL, hoan lai"); return false; }

    void** vtbl = *(void***)gr;
    void** slot = &vtbl[178];                         // CTerrorGameRules::RestartRound
    if (*slot != (void*)fn) {
        EL_LOG("[EdictBudget] WIPECLEAR: vtable slot 178 = %p, khong phai RestartRound %p "
                 "- co plugin khac da moc? Bo qua.", *slot, fn);
        return false;
    }

    // Cong cu go entity dung chung (khai bao o congcu_entity.cpp). InstallMapClear
    // gan CUNG cac dia chi nay neu chua co; o day gan de len - cung gia tri, vo hai.
    // g_InCleanupDelete chi gan o day (mapclear khong can).
    g_CleanupDeleteList = (fnCleanupDeleteList_t)(b + 0x0B5D10);
    g_NextEnt           = (fnNextEnt_t)          (b + 0x0B4270);
    g_UtilRemove        = (fnUtilRemove_t)       (b + 0x2071E0);
    g_EntList           = (void*)                (b + 0x7E0760);
    g_InCleanupDelete   = (bool*)                (b + 0x7E0730);
    g_PreserveList      = (const char**)         (b + 0x7ACE40);

    // Kiem preserve list doc dung khong: phan tu 0 phai la "ai_network",
    // phan tu 33 phai la "predicted_viewmodel".
    const char* p0  = g_PreserveList[0];
    const char* p33 = g_PreserveList[33];
    if (!p0 || !p33 || strcmp(p0, "ai_network") != 0 || strcmp(p33, "predicted_viewmodel") != 0) {
        EL_LOG("[EdictBudget] WIPECLEAR: preserve list sai ([0]=%s [33]=%s) - bo qua.",
                 p0 ? p0 : "(null)", p33 ? p33 : "(null)");
        g_PreserveList = NULL;
        return false;
    }

    // Ghi vtable bang VirtualProtect truc tiep (PAGE_READWRITE) - tuong duong
    // WriteProtected (tienich.cpp) ma cac module khac dung; mapclear cung lam vay.
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    g_OrigRestartRound = (fnRestartRound_t)*slot;
    *slot = (void*)&Hook_RestartRound;
    VirtualProtect(slot, sizeof(void*), old, &old);
    g_RestartSlot = slot;

    LoadWipeKeep();

    // Dang ky nghe tin hieu thua. mission_lost = "survivor team that bai"
    // (chu thich cua Valve trong modevents.res). round_end kem theo de doi chieu.
    if (gameevents && !g_LossHooked) {
        bool a = gameevents->AddListener(&g_LossListener, "mission_lost", true);
        bool b = gameevents->AddListener(&g_LossListener, "round_end",    true);
        g_LossHooked = (a || b);
        EL_LOG("[EdictBudget] WIPECLEAR: nghe su kien mission_lost=%d round_end=%d", a, b);
    }

    EL_LOG("[EdictBudget] WIPECLEAR: da moc vtable slot 178 (RestartRound @ %p), "
             "preserve list OK, wipeclear=%d, cong = co mot-lan tu mission_lost",
             fn, g_WipeClear);
    return true;
}

static void RemoveWipeClear() {
    if (gameevents && g_LossHooked) {
        gameevents->RemoveListener(&g_LossListener);
        g_LossHooked = false;
    }
    if (g_RestartSlot && g_OrigRestartRound) {
        DWORD old;
        if (VirtualProtect(g_RestartSlot, sizeof(void*), PAGE_READWRITE, &old)) {
            *g_RestartSlot = (void*)g_OrigRestartRound;
            VirtualProtect(g_RestartSlot, sizeof(void*), old, &old);
        }
    }
    g_RestartSlot = NULL;
    g_OrigRestartRound = NULL;
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
static void WipeClear_OnServerActivate() {
    // Map moi => xoa co thua con sot lai, tranh don nham o vong choi dau tien.
    g_LossPending = false;
    // g_pGameRules chi ton tai sau khi level da chay => cai o day, khong o Load()
    InstallWipeClear();
}
static void WipeClear_OnUnload() { RemoveWipeClear(); }
