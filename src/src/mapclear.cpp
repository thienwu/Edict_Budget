// ==========================================================================
//  mapclear - don entity luc chuyen man. Xem docs/01-co-che.md muc mapclear,
//  docs/03-loi-da-biet.md muc 7, docs/05-huong-da-bo.md muc "mapclear = 2".
//  (Tai lieu cu, day du hon: docs/02-mapclear.md cua tag v2.0.2.)
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// mapclear: don entity luc CHUYEN MAN, giu lai do nguoi choi mang sang duoc.
// 0 tat | 1 chi quan sat | 2 don that. Xem khoi giai thich gan InstallMapClear().
static int  g_MapClear = 0;
// Tran so entity mapclear duoc xoa moi lan chuyen man. 0 = khong gioi han.
// Dung de TIM NGUONG: hai lan xoa >1300 deu chet cam.
static int  g_MapClearMax = 0;
// 1 = CHI xoa entity co FCAP_ACROSS_TRANSITION. 0 = xoa tat ca (mac dinh).
// !! DE MAC DINH 0. Che do 1 da duoc thu 14/08 va GIET SERVER trong 12 giay.
// Xem docs/03-loi-da-biet.md muc 7 - ba lan chet va co che POST-hook.
static int  g_MapClearCarryOnly = 0;

// MAPCLEAR - don entity TRUOC KHI ENGINE don, luc CHUYEN MAN.
// De mapclear=1 (CHI QUAN SAT). Muc 2 chua chung minh duoc loi ich va da
// tung lam sap server. Xem docs/05-huong-da-bo.md muc "mapclear = 2".

#define MAPKILL_MAX 96
static char  g_MapKill[MAPKILL_MAX][40];
static int   g_MapKillCount = 0;

typedef void (__fastcall *fnPrepChangelevel_t)(void*, void*, void*);
static fnPrepChangelevel_t g_OrigPrepChangelevel = NULL;
static void**              g_PrepSlot            = NULL;
static int                 g_MapClearRuns        = 0;

#define MAPHIST_MAX 64
static char g_MapHistName[MAPHIST_MAX][40];
static int  g_MapHistCount[MAPHIST_MAX];
static int  g_MapHistN = 0;

// Bang dem theo classname - cung kieu (chep tay) voi FreeKeepTally (freegate),
// HeartbeatSample (doluong), kiem ke trong LogAllocFail (baycapphat).
static void MapHistAdd(const char* cls) {
    if (!cls) return;
    for (int i = 0; i < g_MapHistN; i++)
        if (strcmp(g_MapHistName[i], cls) == 0) { g_MapHistCount[i]++; return; }
    if (g_MapHistN >= MAPHIST_MAX) return;
    strncpy(g_MapHistName[g_MapHistN], cls, sizeof(g_MapHistName[0])-1);
    g_MapHistName[g_MapHistN][sizeof(g_MapHistName[0])-1] = 0;
    g_MapHistCount[g_MapHistN] = 1;
    g_MapHistN++;
}

// TEN GAY NHAM: ham va mang mang chu "Kill" nhung doc mapkeep.txt = danh sach GIU
// (bang dung chung, doi y nghia - xem InMapKeep). Danh sach XOA THANG la mapkill.txt,
// doc o LoadMapForceKill.
//
// !! LOI DA BIET, CHUA SUA (tim thay 09/10/2026 khi doc lai, co tu v2.0.2 tro ve
//    truoc): hai dong bo dau/cuoi dong o day va o LoadMapForceKill so '\n' HAI LAN,
//    thieu '\r' (cac ban chep khac cua bo doc nay - noedict, wipeclear, freegate,
//    nonetkill - deu co '\r'). File xuong dong kieu Windows (CRLF) => moi dong con
//    '\r' cuoi => truot phep kiem ky tu => BO IM LANG. configs/mapkeep.txt chi co
//    prop_door_rotating_checkpoint; chi anh huong mapclear (=1 chi ghi log).
static void LoadMapKill() {
    g_MapKillCount = 0;
    // !!  MAC DINH RONG - CO Y, day la diem khac cot loi so voi ban 1.
    //    Khong co mapkill.txt = KHONG XOA GI CA.
    FILE* f = OpenPluginFile("mapkeep.txt", "r");
    if (!f) {
        EL_LOG("[EdictBudget] MAPCLEAR: khong co mapkeep.txt - chi dung danh sach giu ghi cung");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f) && g_MapKillCount < MAPKILL_MAX) {
        size_t L = strlen(line);
        if (L > 0 && line[L-1] != '\n') { int c; while ((c=fgetc(f)) != EOF && c != '\n') {} }
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\n' || !*p) continue;
        char* e = p + strlen(p) - 1;
        while (e >= p && (*e=='\n'||*e=='\n'||*e==' '||*e=='\t')) *e-- = 0;
        if (!*p) continue;
        bool ok = true;
        for (const char* q = p; *q; q++)
            if (!((*q>='a'&&*q<='z')||(*q>='A'&&*q<='Z')||(*q>='0'&&*q<='9')||*q=='_')) { ok=false; break; }
        if (!ok) continue;
        strncpy(g_MapKill[g_MapKillCount], p, sizeof(g_MapKill[0])-1);
        g_MapKill[g_MapKillCount][sizeof(g_MapKill[0])-1] = 0;
        g_MapKillCount++;
    }
    fclose(f);
    EL_LOG("[EdictBudget] MAPCLEAR: mapkeep.txt - giu them %d lop", g_MapKillCount);
    for (int i = 0; i < g_MapKillCount; i++)
        EL_LOG("[EdictBudget] MAPCLEAR:   [%d] '%s'", i, g_MapKill[i]);
}

// Hoi chinh engine: entity nay CO duoc mang sang map moi khong?
// ObjectCaps() = vtable slot 40, bit 0x2 = FCAP_ACROSS_TRANSITION.
#define FCAP_ACROSS_TRANSITION 0x00000002
static bool WillCarryOver(void* ent) {
    if (!ent) return false;
    void** vt = *(void***)ent;
    if (!vt) return false;
    typedef int (__fastcall *fnObjectCaps_t)(void*, void*);
    fnObjectCaps_t caps = (fnObjectCaps_t)vt[40];      // +0xA0
    if (!caps) return false;
    return (caps(ent, NULL) & FCAP_ACROSS_TRANSITION) != 0;
}

// Danh sach KHONG DUOC DON. Ngoai ra don het - phan con lai engine tu lo.
// Ghi cung trong ma nguon phan toi thieu, doc them tu mapkeep.txt.
// Hai vong ben duoi cung luat khop "ten_" = tien to; luat nay con chep o InMapForceKill,
// InFreeKeep (freegate), InKeepExtra (wipeclear).
static bool InMapKeep(const char* cls) {
    if (!cls) return true;                        // khong ro ten => GIU
    static const char* k[] = {
        "weapon_",              // vu khi, vat pham, bom, weapon_*_spawn,
                                // weapon_gascan / propanetank / oxygentank
        "prop_fuel_barrel",     // thung xang / binh ga
        "prop_fuel_barrel_piece",
        "player",
        // ha tang chuyen man - xoa la hong CHINH viec chuyen man
        "info_landmark", "trigger_changelevel", "info_changelevel", "trigger_transition",
        NULL };
    for (int i = 0; k[i]; i++) {
        size_t len = strlen(k[i]);
        if (k[i][len-1] == '_') { if (strncmp(cls, k[i], len) == 0) return true; }
        else                    { if (strcmp(cls, k[i]) == 0)      return true; }
    }
    // them tu mapkeep.txt (dung chung bang voi g_MapKill, doi ten y nghia)
    for (int i = 0; i < g_MapKillCount; i++) {
        size_t len = strlen(g_MapKill[i]);
        if (len == 0) continue;
        if (g_MapKill[i][len-1] == '_') { if (strncmp(cls, g_MapKill[i], len) == 0) return true; }
        else                            { if (strcmp(cls, g_MapKill[i]) == 0)      return true; }
    }
    return false;
}

// Danh sach XOA THANG - thang ca dieu kien "se mang sang". Doc tu mapkill.txt.
// Dung cho RAC ma engine van dinh mang sang: xac zombie, xac nguoi, hat, lua.
// Rac an toan de don: xac zombie, hat, lua.
#define MAPFKILL_MAX 64
static char g_MapFKill[MAPFKILL_MAX][40];
static int  g_MapFKillCount = 0;

// Doc mapkill.txt - cung loi '\r' nhu LoadMapKill (xem ghi chu o do).
static void LoadMapForceKill() {
    g_MapFKillCount = 0;
    FILE* f = OpenPluginFile("mapkill.txt", "r");
    if (!f) {
        EL_LOG("[EdictBudget] MAPCLEAR: khong co mapkill.txt - khong xoa thang lop nao");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f) && g_MapFKillCount < MAPFKILL_MAX) {
        size_t L = strlen(line);
        if (L > 0 && line[L-1] != '\n') { int c; while ((c=fgetc(f)) != EOF && c != '\n') {} }
        char* q = line;
        while (*q == ' ' || *q == '\t') q++;
        if (*q == '#' || *q == '\n' || *q == '\n' || !*q) continue;
        char* e2 = q + strlen(q) - 1;
        while (e2 >= q && (*e2=='\n'||*e2=='\n'||*e2==' '||*e2=='\t')) *e2-- = 0;
        if (!*q) continue;
        bool ok = true;
        for (const char* z = q; *z; z++)
            if (!((*z>='a'&&*z<='z')||(*z>='A'&&*z<='Z')||(*z>='0'&&*z<='9')||*z=='_')) { ok=false; break; }
        if (!ok) continue;
        strncpy(g_MapFKill[g_MapFKillCount], q, sizeof(g_MapFKill[0])-1);
        g_MapFKill[g_MapFKillCount][sizeof(g_MapFKill[0])-1] = 0;
        g_MapFKillCount++;
    }
    fclose(f);
    EL_LOG("[EdictBudget] MAPCLEAR: mapkill.txt - %d lop XOA THANG (thang ca mang sang)",
             g_MapFKillCount);
    for (int i = 0; i < g_MapFKillCount; i++)
        EL_LOG("[EdictBudget] MAPCLEAR:   xoa thang [%d] '%s'", i, g_MapFKill[i]);
}

static bool InMapForceKill(const char* cls) {
    if (!cls) return false;
    for (int i = 0; i < g_MapFKillCount; i++) {
        size_t len = strlen(g_MapFKill[i]);
        if (len == 0) continue;
        if (g_MapFKill[i][len-1] == '_') { if (strncmp(cls, g_MapFKill[i], len) == 0) return true; }
        else                            { if (strcmp(cls, g_MapFKill[i]) == 0)      return true; }
    }
    return false;
}

// CUNG KHUNG DON voi Hook_RestartRound (wipeclear.cpp) - duyet moi entity, giu
// preserve list + danh sach rieng, UTIL_Remove, CleanupDeleteList. Khac: chay luc
// CHUYEN MAN (POST, sau khi game lap xong danh sach mang sang), danh sach giu la
// mapkeep.txt + InMapKeep, va mac dinh chi DEM (mapclear=1), khong xoa.
static void __fastcall Hook_PrepChangelevel(void* thisptr, void* edx, void* a1) {
    // POST: de ban goc chup anh trang bi nguoi choi TRUOC da.
    if (g_OrigPrepChangelevel) g_OrigPrepChangelevel(thisptr, edx, a1);

    g_MapClearRuns++;
    if (g_MapClear < 1) return;

    if (!g_NextEnt || !g_UtilRemove || !g_EntList || !g_CleanupDeleteList) {
        EL_LOG("[EdictBudget] MAPCLEAR #%d: thieu cong cu go entity - BO QUA",
                 g_MapClearRuns);
        return;
    }

    int freeBefore = CountFreeEdicts();
    int total = 0, carry = 0, kept = 0, removed = 0, removedForce = 0, wouldRemove = 0, capped = 0;
    int transient = 0;      // khong mang sang -> bo qua, chet theo map
    g_MapHistN = 0;

    void* e = g_NextEnt(g_EntList, NULL, NULL);
    while (e) {
        void* next = g_NextEnt(g_EntList, NULL, e);
        const char* cls = *(const char**)((uint8_t*)e + 0x74);   // m_iClassname
        total++;

        bool willCarry = WillCarryOver(e);
        if (willCarry) carry++;

// 1. DANH SACH KHONG DUOC DON: vat pham nguoi choi, diem chuyen map, cua an
// toan, nguoi choi, cong toan bo preserve list cua game. docs/01-co-che.md muc mapclear
        if (InPreserveList(cls) || InMapKeep(cls)) { kept++; e = next; continue; }

        // 2. g_MapClearCarryOnly: THI NGHIEM DA THAT BAI, mac dinh 0 nen nhanh nay
        //    khong chay. Xem khoi giai thich dai o cho khai bao g_MapClearCarryOnly.
        //    Bat len = xoa dung nhung cai engine da ghi vao danh sach chuyen man = sap.
        if (g_MapClearCarryOnly && !willCarry) { transient++; e = next; continue; }

        // 3. XOA.
        //
        // !!  TRAN SO LUONG (g_MapClearMax): luoi an toan, 0 = khong gioi han.
        //    Luu y tran nay dem theo SO LUOT XOA, khong theo so cai mang sang - ma
        //    cai mang sang moi la thu giet server. Tran cang cao thi cang vo phai
        //    nhieu cai mang sang. Do la ly do cap 100 song ma >1300 chet.
        if (g_MapClear >= 2) {
            if (g_MapClearMax > 0 && removed >= g_MapClearMax) { capped++; e = next; continue; }
            g_UtilRemove(e); removed++;
            if (willCarry) removedForce++;      // dem rieng: xoa ca thu se mang sang
        } else {
            wouldRemove++; MapHistAdd(cls);
        }
        e = next;
    }

    if (g_MapClear >= 2) {
        if (removed > 0) {
            g_CleanupDeleteList(g_EntList, NULL);
            if (engine) engine->AllowImmediateEdictReuse();
        }
        int freeAfter = CountFreeEdicts();
        EL_LOG("[EdictBudget] MAPCLEAR #%d (che do %d, chi-mang-sang=%d) DA DON: tong %d "
                 "| mang sang %d | go %d (mang sang %d), giu %d, bo qua vi khong mang "
                 "sang %d, cham tran %d | slot trong %d -> %d | num_edicts=%d",
                 g_MapClearRuns, g_MapClear, g_MapClearCarryOnly, total, carry,
                 removed, removedForce, kept, transient, capped, freeBefore, freeAfter,
                 g_num_edicts ? (int)*g_num_edicts : -1);
        return;
    }

    EL_LOG("[EdictBudget] MAPCLEAR #%d QUAN SAT (chi-mang-sang=%d): tong %d "
             "| **SE MANG SANG %d** | se go %d, giu %d, bo qua vi khong mang sang %d "
             "| slot trong=%d num_edicts=%d",
             g_MapClearRuns, g_MapClearCarryOnly, total, carry, wouldRemove, kept,
             transient, freeBefore, g_num_edicts ? (int)*g_num_edicts : -1);
    // Histogram nay duoc nap tu nhanh wouldRemove, tuc la DUNG nhung lop sap bi go.
    // Voi g_MapClearCarryOnly=1 thi day chinh la cac lop mang sang; voi =0 thi no la
    // TAT CA cac lop bi go, khong rieng gi mang sang - dung doc nham nhu lan truoc.
    EL_LOG("[EdictBudget] MAPCLEAR: cac lop SE BI GO%s:",
             g_MapClearCarryOnly ? " (deu la lop MANG SANG)" : " (ca mang sang lan khong)");
    for (int shown = 0; shown < 25; shown++) {
        int best = -1;
        for (int i = 0; i < g_MapHistN; i++)
            if (g_MapHistCount[i] > 0 && (best < 0 || g_MapHistCount[i] > g_MapHistCount[best])) best = i;
        if (best < 0 || g_MapHistCount[best] <= 0) break;
        EL_LOG("[EdictBudget] MAPCLEAR:   %5d  %s", g_MapHistCount[best], g_MapHistName[best]);
        g_MapHistCount[best] = 0;
    }
}

static bool InstallMapClear() {
    if (g_OrigPrepChangelevel) return true;       // da cai
    if (g_MapClear <= 0) return false;

    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* b = (uint8_t*)mi.lpBaseOfDll;

    void*** grPtr = (void***)(b + 0x7F7F6C);      // g_pGameRules
    void** gr = *grPtr;
    if (!gr) { EL_LOG("[EdictBudget] MAPCLEAR: g_pGameRules con NULL, hoan lai"); return false; }
    void** vt = *(void***)gr;

    // Cong cu go: nap neu wipeclear chua nap (mapclear chay doc lap duoc).
    // CUNG dia chi voi InstallWipeClear (wipeclear.cpp) - sua o mot cho thi sua ca hai.
    // g_pGameRules (0x7F7F6C) o tren cung chep tu wipeclear.
    if (!g_CleanupDeleteList) g_CleanupDeleteList = (fnCleanupDeleteList_t)(b + 0x0B5D10);
    if (!g_NextEnt)           g_NextEnt           = (fnNextEnt_t)          (b + 0x0B4270);
    if (!g_UtilRemove)        g_UtilRemove        = (fnUtilRemove_t)       (b + 0x2071E0);
    if (!g_EntList)           g_EntList           = (void*)                (b + 0x7E0760);
    if (!g_PreserveList)      g_PreserveList      = (const char**)         (b + 0x7ACE40);

// Cong an toan 1: prologue cua 0x102B8140 phai dung. Prologue chua dia chi
// tuyet doi DA DOI THEO BASE nen phai dung MAT NA (nhu FindPattern, tienich.cpp).
    static const uint8_t kSig[]  = {
        0x55,0x8B,0xEC,0x56,0x8B,0x35, 0,0,0,0, 0x8B,0x06,0x8B,0x50,0x68,0x8B };
    static const char    kMask[] = "xxxxxx????xxxxxx";
    uint8_t* fn = b + 0x2B8140;
    bool sigOk = true;
    for (size_t i = 0; i < sizeof(kSig); i++)
        if (kMask[i] == 'x' && fn[i] != kSig[i]) { sigOk = false; break; }
    if (!sigOk) {
        EL_LOG("[EdictBudget] MAPCLEAR: prologue slot38 KHONG khop - "
                 "server.dll khac ban? BO QUA, khong dong gi.");
        return false;
    }
    // Cong an toan 2: slot 38 phai dang tro dung ham do.
    void** slot = &vt[38];
    if (*slot != (void*)fn) {
        EL_LOG("[EdictBudget] MAPCLEAR: slot38=%p != %p - BO QUA.", *slot, (void*)fn);
        return false;
    }

    // VirtualProtect truc tiep - tuong duong WriteProtected (tienich.cpp), nhu wipeclear.
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    g_OrigPrepChangelevel = (fnPrepChangelevel_t)*slot;
    *slot = (void*)&Hook_PrepChangelevel;
    VirtualProtect(slot, sizeof(void*), old, &old);
    g_PrepSlot = slot;

    LoadMapKill();
    LoadMapForceKill();
    EL_LOG("[EdictBudget] MAPCLEAR: da moc vtable slot 38 (0x102B8140 @ %p), "
             "mapclear=%d (%s)", (void*)fn, g_MapClear,
             g_MapClear >= 2 ? "DON THAT" : "CHI QUAN SAT");
    return true;
}

static void RemoveMapClear() {
    if (g_PrepSlot && g_OrigPrepChangelevel) {
        DWORD old;
        if (VirtualProtect(g_PrepSlot, sizeof(void*), PAGE_READWRITE, &old)) {
            *g_PrepSlot = (void*)g_OrigPrepChangelevel;
            VirtualProtect(g_PrepSlot, sizeof(void*), old, &old);
        }
    }
    g_PrepSlot = NULL;
    g_OrigPrepChangelevel = NULL;
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
static void MapClear_OnServerActivate() { InstallMapClear(); }
static void MapClear_OnUnload() { RemoveMapClear(); }
