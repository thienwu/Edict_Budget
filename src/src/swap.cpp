// ==========================================================================
//  swap - doi lop entity luc tao (factory dictionary slot 1)
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
static int  g_Swap = 0;
// Tran so lan doi. 0 = khong gioi han. Dung de thu vai chuc cai truoc khi doi het.
static int  g_SwapMax = 0;

// SWAP: doi mot lop entity thanh lop khac RE HON ngay luc tao.
// KHONG phai go mang, KHONG phai xoa - client van nhan va van ve.
// Moc CEntityFactoryDictionary::Create (vtable slot 1). Xem docs/01-co-che.md

#define SWAP_MAX 16
static char g_SwapFrom[SWAP_MAX][40];
static char g_SwapTo[SWAP_MAX][40];
static int  g_SwapSeen[SWAP_MAX];      // gap bao nhieu lan
static int  g_SwapHits[SWAP_MAX];      // doi that bao nhieu lan
static int  g_SwapCount = 0;
static int  g_SwapDone  = 0;

// Ten map cua lan nap hien tai, dat o Hook_LevelInit. In kem bao cao SWAP de
// khoi phai doan map nao qua con so.
static const char* g_SwapMapName = "?";
static void** g_DictVt = NULL;
typedef void* (__fastcall *DictCreate_t)(void*, void*, const char*);
static DictCreate_t g_OrigDictCreate = NULL;

// SONG SONG voi spotlight.cpp Hook_SpotDictCreate tren CUNG slot 1. spotlight cai SAU
// va noi chuoi ben ngoai: no goi ham nay cho moi lan tao, TRU point_spotlight doc tu
// map ma no da quyet (luc do no goi thang Create goc). Vi vay voi spotlight=2, dong
// "point_spotlight -> beam_spotlight" o day chi con tac dung voi point_spotlight tao
// LUC CHAY. Hai bo dem (o day va SPOTLIGHT) dem hai tap khac nhau.
static void* __fastcall Hook_DictCreate(void* thisptr, void* edx, const char* cls) {
    if (cls && g_Swap >= 1) {
        for (int i = 0; i < g_SwapCount; i++) {
            if (_stricmp(cls, g_SwapFrom[i]) != 0) continue;
            g_SwapSeen[i]++;
            // che do 1 = chi dem, khong doi. Dung de biet map co bao nhieu cai
            // truoc khi dam doi that.
            if (g_Swap >= 2 && (g_SwapMax <= 0 || g_SwapDone < g_SwapMax)) {
                g_SwapDone++;
                g_SwapHits[i]++;
                cls = g_SwapTo[i];
            }
            break;
        }
    }
    return g_OrigDictCreate(thisptr, edx, cls);
}

// Doc CAP lop "a -> b" - khac khuon voi cac file danh sach mot-lop (noedict.txt,
// wipekeep.txt, freekeep.txt, mapkeep.txt...), nen khong dung chung bo doc.
static void LoadSwapList() {
    g_SwapCount = 0;
    // Mac dinh RONG. Khong co swap.txt = khong doi lop nao.
    FILE* f = OpenPluginFile("swap.txt", "r");
    if (!f) { EL_LOG("[EdictBudget] SWAP: khong co swap.txt - khong doi lop nao"); return; }
    char line[256];
    while (fgets(line, sizeof(line), f) && g_SwapCount < SWAP_MAX) {
        size_t L = strlen(line);
        if (L > 0 && line[L-1] != '\n') { int c; while ((c=fgetc(f)) != EOF && c != '\n') {} }
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
        // dinh dang:  tu_lop  ->  thanh_lop     (dau -> tuy chon)
        char a[40] = {0}, b[40] = {0};
        int n = sscanf(p, "%39s -> %39s", a, b);
        if (n != 2) { n = sscanf(p, "%39s %39s", a, b); }
        if (n != 2 || !a[0] || !b[0]) continue;
        strncpy(g_SwapFrom[g_SwapCount], a, 39);
        strncpy(g_SwapTo  [g_SwapCount], b, 39);
        g_SwapSeen[g_SwapCount] = 0;
        g_SwapHits[g_SwapCount] = 0;
        g_SwapCount++;
    }
    fclose(f);
    EL_LOG("[EdictBudget] SWAP: %d cap trong danh sach", g_SwapCount);
}

static bool InstallSwap() {
    if (g_Swap <= 0 || g_SwapCount <= 0) return false;

    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    uint8_t* base = (uint8_t*)h;

    // GetEntityFactoryDictionary (RVA 0x20CA70) + FindFactory (slot 3): cung cach voi
    // ResolveClassVtable (noedict.cpp) va InstallSpotlight (spotlight.cpp).
    typedef void* (__cdecl *GetDict_t)(void);
    typedef void* (__thiscall *FindFactory_t)(void*, const char*);
    GetDict_t getDict = (GetDict_t)(base + 0x20CA70);
    void* dict = getDict();
    if (!dict) { EL_LOG("[EdictBudget] SWAP: khong lay duoc factory dictionary, BO QUA"); return false; }

    void** dvt = *(void***)dict;
    if (!dvt || !dvt[1] || !dvt[3]) {
        EL_LOG("[EdictBudget] SWAP: vtable dictionary khong hop le, BO QUA");
        return false;
    }
    FindFactory_t findFac = (FindFactory_t)dvt[3];

    // Cong an toan: CA HAI lop cua moi cap phai ton tai that trong dictionary.
    // Doi sang mot lop khong ton tai = CreateEntityByName tra NULL = mat entity.
    int valid = 0;
    for (int i = 0; i < g_SwapCount; i++) {
        void* fa = findFac(dict, g_SwapFrom[i]);
        void* fb = findFac(dict, g_SwapTo[i]);
        if (!fa || !fb) {
            EL_LOG("[EdictBudget] SWAP: bo cap '%s' -> '%s' (%s khong ton tai trong "
                     "factory dictionary)", g_SwapFrom[i], g_SwapTo[i],
                     !fa ? g_SwapFrom[i] : g_SwapTo[i]);
            g_SwapFrom[i][0] = 0;          // vo hieu cap nay
            continue;
        }
        EL_LOG("[EdictBudget] SWAP:   [%d] '%s' -> '%s' (ca hai lop deu ton tai)",
                 i, g_SwapFrom[i], g_SwapTo[i]);
        valid++;
    }
    if (valid == 0) { EL_LOG("[EdictBudget] SWAP: khong cap nao hop le, BO QUA"); return false; }

    g_DictVt = dvt;
    g_OrigDictCreate = (DictCreate_t)dvt[1];
    void* thunk = (void*)&Hook_DictCreate;
    WriteProtected(&dvt[1], &thunk, sizeof(void*));

    EL_LOG("[EdictBudget] SWAP: da moc dictionary slot 1 (Create) @%p, che do %d (%s), "
             "tran %d, %d cap hop le",
             (void*)g_OrigDictCreate, g_Swap,
             g_Swap >= 2 ? "DOI THAT" : "CHI QUAN SAT",
             g_SwapMax, valid);
    return true;
}

static void UninstallSwap() {
    if (!g_DictVt || !g_OrigDictCreate) return;
    void* orig = (void*)g_OrigDictCreate;
    WriteProtected(&g_DictVt[1], &orig, sizeof(void*));
    g_OrigDictCreate = NULL;
    g_DictVt = NULL;
}

// Bao cao roi DAT LAI VE 0. Ban 16:01 khong dat lai nen so cong don qua cac map,
// phai tu tru moi ra so tung map (0->80->392->404...). Kho doc va de nham.
static void SwapReport() {
    if (g_SwapCount <= 0 || g_Swap <= 0) return;
    for (int i = 0; i < g_SwapCount; i++) {
        if (!g_SwapFrom[i][0]) continue;
        if (g_SwapSeen[i] == 0 && g_SwapHits[i] == 0) continue;   // map nay khong co
        // KHONG in "tiet kiem = doi x 2". Con so do SAI.
        //   point_spotlight chi sinh 2 con khi spawnflags & 1. Cai nao khong co bit
        //   do thi von da la 1 edict, doi sang beam_spotlight khong tiet kiem gi.
        //   Do 14/08 tren the_hive_m5: 12 cai (8 co spawnflags=2, 4 co =3)
        //     truoc  8x1 + 4x3 = 20 edict | sau  12x1 = 12 | tiet kiem THAT = 8
        //   ma cong thuc doi-x2 lai bao 24.
        // Luc Create chua doc keyvalue nen khong biet spawnflags => chi in can tren.
        EL_LOG("[EdictBudget] SWAP [%s]: '%s' -> '%s' | gap %d | doi %d | "
                 "tiet kiem toi da %d edict (that su it hon neu co cai khong bat san)",
                 g_SwapMapName, g_SwapFrom[i], g_SwapTo[i],
                 g_SwapSeen[i], g_SwapHits[i], g_SwapHits[i] * 2);
    }
    if (g_SwapMax > 0 && g_SwapDone >= g_SwapMax)
        EL_LOG("[EdictBudget] SWAP: DA CHAM TRAN %d - phan con lai khong doi", g_SwapMax);
    // KHONG dat lai o day nua - da chuyen sang Hook_LevelInit, xem giai thich o do.
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// SWAP phai cai TRUOC khi map dau tien phan tich lump entity, vi chinh bo phan
// tich lump cung di qua dictionary slot 1. Cai luc Load la som nhat co the.
static void Swap_OnLoad() { if (g_Swap > 0) { LoadSwapList(); InstallSwap(); } }

// Dat lai bo dem SWAP tai DAY, khong phai o SwapReport(). docs/01-co-che.md
static void Swap_OnLevelInit(const char* pMapName) {
    if (g_Swap > 0) {
        for (int i = 0; i < g_SwapCount; i++) { g_SwapSeen[i] = 0; g_SwapHits[i] = 0; }
        g_SwapDone = 0;
        g_SwapMapName = pMapName ? pMapName : "?";
    }
}
static void Swap_OnServerActivate() { SwapReport(); }   // sau khi lump da phan tich xong
static void Swap_OnUnload() { UninstallSwap(); }
