// ==========================================================================
//  freegate - bo/giu cach ly 1 giay sau khi giai phong edict
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// freegate dat freetime = 0 NGAY khi edict duoc giai phong (moc RemoveEdict), nen
// lan cap phat tiep theo trong CUNG frame cung lay lai duoc o do. Goi
// AllowImmediateEdictReuse() theo nhip frame thi luon muon: wipe xoa roi tao lai
// trong cung mot frame. Xem docs/01-co-che.md muc freegate.
static int  g_PatchFreeGate    = 0;      // 0 TAT (mac dinh) | 1 co danh sach | 2 vo dieu kien
                                         // MAC DINH TAT: co che it duoc nghiem thu nhat, va
                                         // che do 2 da xac minh la lam hong viec chuyen vat pham.

// ==========================================================================
//  FREEGATE - ba che do. Xem docs/01-co-che.md muc freegate va docs/03-loi-da-biet.md
//  muc 6 (freegate=2 lam hong chuyen vat pham).
//
//    0 = TAT (MAC DINH - g_PatchFreeGate = 0 o tren). Cong 1 giay cua engine nguyen ven.
//    1 = CO DANH SACH (tung la mac dinh, nay khong con). Moc IVEngineServer::RemoveEdict (vtable slot
//        23). Sau khi ham goc chay, neu classname KHONG nam trong freekeep.txt
//        thi dat freetime[i] = 0.0 => ED_Alloc nhanh THU NHAT lay ngay.
//        Lop NAM trong danh sach thi de nguyen GetTime() => giu cach ly 1 giay.
//    2 = VO DIEU KIEN (che do cu). Vá mot byte 0x1E022A jae -> jmp.
//
//  VI SAO CAN CHE DO 1 (do duoc 30/08/2026):
//    l4d_gear_transfer huy roi TAO LAI vat pham trong CUNG MOT FRAME:
//        RemoveEdict(ent);  ent = CreateAndEquip(...);
//    Che do 2 tra lai DUNG chi so vua giai phong => client khong bao gio thay
//    ranh gioi xoa/tao => "ghost weapon". Changelog cua chinh plugin do da ghi
//    hai trieu chung nay (v2.16 ghost weapon, v2.19 "Invalid edict").
//    Engine goc chi cho chuyen pain_pills / adrenaline; 7 loai con lai la do
//    plugin mo rong bang cach huy-va-tao-lai - nen chung dinh dung cho nay.
//
//  DA XAC MINH TREN BINARY:
//    IVEngineServer vtable 0x1037D9BC
//      slot 22 CreateEdict              -> ED_Alloc 0x101E0170
//      slot 23 RemoveEdict  0x10130B50  -> ED_Free  0x101DFF60   << cua DUY NHAT
//      slot 95 AllowImmediateEdictReuse -> thunk -> 0x101DFF10
//    ED_Free co DUNG MOT loi vao (1 call rel32, 0 tham chieu du lieu) => moc
//    slot 23 phu 100% moi lan giai phong edict.
//    ED_Free:  or [edict],2 ; freetime[i] = sv.GetTime() ; serial++
//    ED_Alloc: nhanh 1 `comiss 2.0f, freetime[i] ; ja LAY`  <- freetime 0.0 di loi nay
//              nhanh 2 `curtime - freetime >= 1.0 ; jae LAY` <- che do 2 vá o day
// ==========================================================================

#define FREEKEEP_MAX 64
static char  g_FreeKeep[FREEKEEP_MAX][48];
static int   g_FreeKeepCount = 0;

typedef void (__stdcall *fnRemoveEdict_t)(void* pEdict);
static fnRemoveEdict_t g_OrigRemoveEdict = NULL;
static void**  g_RemoveEdictSlot = NULL;
static float*  g_FreeTimeTable   = NULL;   // suy tu ma may, KHONG ghi cung
static void**  g_pEdictsPtr      = NULL;   // suy tu ma may, KHONG ghi cung
static int     g_FreeReleased    = 0;      // so edict duoc tra ve dung ngay
static int     g_FreeQuarantined = 0;      // so edict bi giu lai 1 giay
static int     g_FreeKeepLogged  = 0;

// Luat khop: ten ket thuc bang '_' = khop TIEN TO ca ho, con lai = khop chinh xac.
// CUNG LUAT (chep tay, chua gop) voi InKeepExtra (wipeclear), InMapKeep va
// InMapForceKill (mapclear).
static bool InFreeKeep(const char* cls) {
    if (g_FreeKeepCount <= 0 || !cls || !*cls) return false;
    for (int i = 0; i < g_FreeKeepCount; i++) {
        const char* k = g_FreeKeep[i];
        size_t n = strlen(k);
        if (n == 0) continue;
        if (k[n - 1] == '_') { if (strncmp(cls, k, n) == 0) return true; }   // tien to
        else if (strcmp(cls, k) == 0) return true;                            // chinh xac
    }
    return false;
}

// Cung khuon doc file voi LoadWipeKeep - ke ca cach xa phan du cua dong bi cat.
// (Danh sach day du cac ban chep cua bo doc nay: xem LoadNoEdictList, noedict.cpp.)
static void LoadFreeKeep() {
    g_FreeKeepCount = 0;
    FILE* f = OpenPluginFile("freekeep.txt", "r");
    if (!f) {
        EL_LOG("[EdictBudget] FREEGATE: khong co freekeep.txt - MOI lop deu tai su dung ngay");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f) && g_FreeKeepCount < FREEKEEP_MAX) {
        size_t L = strlen(line);
        if (L > 0 && line[L - 1] != '\n') { int c; while ((c = fgetc(f)) != EOF && c != '\n') {} }
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
        char* e = p + strlen(p) - 1;
        while (e >= p && (*e == '\r' || *e == '\n' || *e == ' ' || *e == '\t')) *e-- = 0;
        if (!*p) continue;
        bool ok = true;
        for (const char* q = p; *q; q++)
            if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                  (*q >= '0' && *q <= '9') || *q == '_')) { ok = false; break; }
        if (!ok) { EL_LOG("[EdictBudget] FREEGATE: freekeep.txt bo qua dong khong hop le: '%s'", p); continue; }
        strncpy(g_FreeKeep[g_FreeKeepCount], p, sizeof(g_FreeKeep[0]) - 1);
        g_FreeKeep[g_FreeKeepCount][sizeof(g_FreeKeep[0]) - 1] = 0;
        g_FreeKeepCount++;
    }
    fclose(f);
    for (int i = 0; i < g_FreeKeepCount; i++)
        EL_LOG("[EdictBudget] FREEGATE:   giu cach ly [%d] '%s'", i, g_FreeKeep[i]);
    EL_LOG("[EdictBudget] FREEGATE: freekeep.txt - %d lop KHONG duoc tai su dung ngay", g_FreeKeepCount);
}

// ---- Thong ke de THEO DOI TREN MAY CHU THAT (doc trong edictbudget.log) ----
// Muc dich: biet duoc lop nao thuc su bi giu cach ly, bao nhieu lan, va chi so
// edict co bi tai dung trong cung frame nua khong. Chi ghi log, khong doi hanh vi.
#define FKTALLY_MAX 24
static char g_FkName[FKTALLY_MAX][48];
static int  g_FkHits[FKTALLY_MAX];
static int  g_FkNames = 0;
static int  g_FreeLastTick = -1;      // tick cua lan giai phong gan nhat
static int  g_SameTickFrees = 0;      // so lan giai phong trong CUNG mot tick
static int  g_SameTickPeak = 0;

// Bang dem theo classname. Cung kieu (chep tay) voi MapHistAdd (mapclear),
// HeartbeatSample (doluong) va kiem ke trong LogAllocFail (baycapphat).
static void FreeKeepTally(const char* cls) {
    for (int i = 0; i < g_FkNames; i++)
        if (strcmp(g_FkName[i], cls) == 0) { g_FkHits[i]++; return; }
    if (g_FkNames >= FKTALLY_MAX) return;
    strncpy(g_FkName[g_FkNames], cls, sizeof(g_FkName[0]) - 1);
    g_FkName[g_FkNames][sizeof(g_FkName[0]) - 1] = 0;
    g_FkHits[g_FkNames] = 1;
    g_FkNames++;
}

// Goi tu heartbeat va tu LevelInit. In mot dong tong hop + bang theo lop.
static void FreeGateReport(const char* moc) {
    if (g_PatchFreeGate != 1) return;
    EL_LOG("[EdictBudget] FREEGATE bao cao (%s): tra ngay %d | giu cach ly %d "
           "| dinh giai phong cung mot tick = %d | danh sach %d lop",
           moc, g_FreeReleased, g_FreeQuarantined, g_SameTickPeak, g_FreeKeepCount);
    for (int i = 0; i < g_FkNames; i++)
        EL_LOG("[EdictBudget] FREEGATE   giu cach ly: %-32s %6d lan", g_FkName[i], g_FkHits[i]);
    if (g_FkNames == 0 && g_FreeKeepCount > 0)
        EL_LOG("[EdictBudget] FREEGATE   (chua lop nao trong danh sach bi giai phong)");
}

static void __stdcall Hook_RemoveEdict(void* pEdict)
{
    // Dem so lan giai phong trong CUNG mot tick - con so nay cho biet co bao
    // nhieu co hoi de mot chi so bi tra lai ngay trong frame. Chi de theo doi.
    if (gpGlobals) {
        int t = gpGlobals->tickcount;
        if (t == g_FreeLastTick) {
            if (++g_SameTickFrees > g_SameTickPeak) g_SameTickPeak = g_SameTickFrees;
        } else { g_FreeLastTick = t; g_SameTickFrees = 1; }
    }

    // Doc classname TRUOC khi goi ham goc: ED_Free goi [[0x106BB8C0]+8](edict)
    // de giai phong phia server, sau do m_pNetworkable khong con dung duoc.
    char cls[48]; cls[0] = 0;
    __try {
        IServerNetworkable* net = *(IServerNetworkable**)((uint8_t*)pEdict + 8);
        if (net) {
            const char* c = net->GetClassName();
            if (c && *c) { strncpy(cls, c, sizeof(cls) - 1); cls[sizeof(cls) - 1] = 0; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { cls[0] = 0; }

    g_OrigRemoveEdict(pEdict);          // ED_Free: FL_EDICT_FREE + freetime = GetTime() + serial++

    if (!g_FreeTimeTable || !g_pEdictsPtr) return;
    if (cls[0] && InFreeKeep(cls)) {    // trong danh sach -> GIU cach ly 1 giay
        g_FreeQuarantined++;
        FreeKeepTally(cls);
        // CHE DO TEST TRUC TIEP: ghi TUNG LAN, kem chi so edict va tick, de doi
        // chieu voi thao tac chuyen do tren may chu that. Tran 2000 dong/map chi
        // de phong truong hop ai do dat 'weapon_' (trum ca sung) vao danh sach.
        if (g_FreeKeepLogged < 2000) {
            g_FreeKeepLogged++;
            int idx = -1;
            __try {
                uint8_t* a = (uint8_t*)*g_pEdictsPtr;
                if (a) { ptrdiff_t d = (uint8_t*)pEdict - a;
                         if (d >= 0 && (d % EDICT_SIZE) == 0) idx = (int)(d / EDICT_SIZE); }
            } __except (EXCEPTION_EXECUTE_HANDLER) { idx = -1; }
            EL_LOG("[EdictBudget] FREEGATE giu cach ly #%d: '%s' edict=%d tick=%d "
                   "| cung tick da giai phong %d cai | ED_Alloc SE KHONG tra lai chi so nay",
                   g_FreeQuarantined, cls, idx,
                   gpGlobals ? gpGlobals->tickcount : -1, g_SameTickFrees);
            if (g_FreeKeepLogged == 2000)
                EL_LOG("[EdictBudget] FREEGATE: da du 2000 dong cho map nay - im den map sau");
        }
        return;
    }
    __try {
        uint8_t* arr = (uint8_t*)*g_pEdictsPtr;
        if (!arr) return;
        ptrdiff_t d = (uint8_t*)pEdict - arr;
        if (d < 0 || (d % EDICT_SIZE) != 0) return;
        int idx = (int)(d / EDICT_SIZE);
        if (idx <= 0 || idx >= NET_LIMIT) return;
        g_FreeTimeTable[idx] = 0.0f;    // ED_Alloc nhanh 1: 0.0 < 2.0 -> lay ngay
        g_FreeReleased++;
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
}

// Suy hai dia chi tu chinh than ED_Free - khong ghi cung RVA nao.
//   A1 <imm32>          mov eax,[g_pEdicts]
//   D9 1C B5 <imm32>    fstp dword [esi*4 + freetime]
static bool DeriveFreeTimeAnchors(uint8_t* edFree) {
    for (int i = 0; i < 0x80; i++) {
        if (!g_pEdictsPtr && edFree[i] == 0xA1)
            g_pEdictsPtr = (void**)*(uint32_t*)(edFree + i + 1);
        if (!g_FreeTimeTable && edFree[i] == 0xD9 && edFree[i+1] == 0x1C && edFree[i+2] == 0xB5)
            g_FreeTimeTable = (float*)*(uint32_t*)(edFree + i + 3);
        if (g_pEdictsPtr && g_FreeTimeTable) return true;
    }
    return false;
}

// Che do 1: moc vtable slot 23 cua IVEngineServer. Hai cong an toan.
static void InstallFreeGateList() {
    if (!engine) { EL_LOG("[EdictBudget] FREEGATE: chua co con tro engine - BO QUA."); return; }
    void** vt = *(void***)engine;
    if (!vt) return;

    uint8_t* fn = (uint8_t*)vt[23];
    // Cong an toan 1: prologue cua RemoveEdict.
    //   55 8B EC 8B 0D ?? ?? ?? ?? 56 8B 75 08
    static const uint8_t kSig[]  = {0x55,0x8B,0xEC,0x8B,0x0D,0,0,0,0,0x56,0x8B,0x75,0x08};
    static const char*   kMask   =  "xxxxx....xxxx";
    bool okSig = true;
    __try {
        for (int i = 0; kMask[i]; i++)
            if (kMask[i] == 'x' && fn[i] != kSig[i]) { okSig = false; break; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { okSig = false; }
    if (!okSig) {
        EL_LOG("[EdictBudget] FREEGATE: slot23 @%p prologue KHONG khop RemoveEdict - BO QUA.", fn);
        return;
    }

    // Cong an toan 2: lan theo call rel32 duy nhat -> ED_Free, roi suy hai dia chi.
    uint8_t* edFree = NULL;
    for (int i = 0; i < 0x40; i++) {
        if (fn[i] == 0xE8) { edFree = fn + i + 5 + *(int32_t*)(fn + i + 1); break; }
        if (fn[i] == 0xC2 || fn[i] == 0xC3) break;
    }
    if (!edFree || !DeriveFreeTimeAnchors(edFree)) {
        EL_LOG("[EdictBudget] FREEGATE: khong suy duoc bang freetime tu ED_Free - BO QUA.");
        return;
    }

    LoadFreeKeep();
    g_OrigRemoveEdict = (fnRemoveEdict_t)fn;
    g_RemoveEdictSlot = &vt[23];
    void* pNew = (void*)&Hook_RemoveEdict;
    WriteProtected(g_RemoveEdictSlot, &pNew, sizeof(void*));

    EL_LOG("[EdictBudget] FREEGATE che do 1 (co danh sach): da moc RemoveEdict slot 23 "
           "(goc @%p, ED_Free @%p) | freetime=%p g_pEdicts=%p | %d lop bi giu cach ly",
           fn, edFree, g_FreeTimeTable, g_pEdictsPtr, g_FreeKeepCount);
}

// Che do 2 (cu): bo thoi gian cho 1 giay VO DIEU KIEN - doi MOT byte tai
// 0x101E022A: jae -> jmp. Giu lai de doi chung. Xem docs/01-co-che.md.
static void PatchFreetimeGate() {
    // fld1 ; fxch st(1) ; fcompi st(1) ; fstp st(0) ; jae
    const char* sig  = "\xD9\xE8\xD9\xC9\xDF\xF1\xDD\xD8\x73";
    const char* mask = "xxxxxxxxx";
    uint8_t* m = FindPattern("engine.dll", sig, mask);
    if (!m) {
        EL_LOG("[EdictBudget] freetime-gate: khong tim thay chu ky!");
        return;
    }
    uint8_t jmp = 0xEB;                 // 73 xx (jae) -> EB xx (jmp)
    WriteProtected(m + 8, &jmp, 1);
    EL_LOG("[EdictBudget] freetime-gate: bo thoi gian cho 1 giay tai %p "
             "- moi edict trong deu tai su dung duoc ngay", m + 8);
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
static void FreeGate_OnLoad() {
    if      (g_PatchFreeGate == 1) InstallFreeGateList();   // co danh sach freekeep.txt
    else if (g_PatchFreeGate >= 2) PatchFreetimeGate();     // vo dieu kien (che do cu)
}

// Bao cao freegate cua MAN VUA XONG truoc khi dat lai bo dem - de doc log
// tren may chu that biet tung map giu cach ly bao nhieu.
static void FreeGate_OnLevelInit() {
    if (g_PatchFreeGate == 1 && (g_FreeReleased || g_FreeQuarantined)) {
        FreeGateReport("het map");
        g_FreeReleased = g_FreeQuarantined = 0;
        g_FkNames = 0; g_FreeKeepLogged = 0; g_SameTickPeak = 0;
    }
}

static void FreeGate_OnHeartbeat() { FreeGateReport("nhip"); }
