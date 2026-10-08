// ==========================================================================
//  spotlight - quyet THEO TUNG point_spotlight (khong theo lop) luc map doc
//  khoi keyvalue cua no. patches.txt: spotlight=0/1/2
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// VI SAO (docs/01-co-che.md muc spotlight; dia chi o docs/04-dia-chi.md):
//   point_spotlight bat = 3 thuc the mang: chinh no + beam + spotlight_end.
//   swap.txt "point_spotlight -> beam_spotlight" lay lai 2 edict moi cai, NHUNG
//   client.dll ghi cung halo = 60 (C_BeamSpotLight 1006CAD0: m_fHaloScale =
//   DAT_1053F068) => 655 spotlight tren 231 nguon co HaloScale != 60 bi ve SAI
//   (the_hive_m4 312 + m3 80 dat 10 => halo to gap 6; c2m3_coaster 76 dat 16).
//
// LUAT (tu kiem luc chay tu chinh khoi keyvalue, khong danh sach viet tay):
//   HaloScale = 60 (thieu khoa = 60, ctor 0x1018DC20 ghi [+0x46C]=60.0)
//       -> doi RIENG thuc the nay sang beam_spotlight (1 edict, ve giong het;
//          C_BeamSpotLight::ClientThink 1006D1C0 tu tinh tia theo goc cha moi khung)
//   HaloScale != 60, KHONG co parentname
//       -> giu point_spotlight cua Valve; chinh no + spotlight_end cua no KHONG
//          edict (EFL_SERVER_ONLY); chi beam co edict => 1 edict, dung halo.
//          An toan vi: spotlight khong cha la "efficient" (Activate 0x1018F3F0)
//          => beam kieu POINTS, khong tro toi hai thuc the kia; point_spotlight
//          von DONTSEND (slot21 0x1018DB70); client khong lam gi thay duoc voi
//          spotlight_end (ClientThink 1009E7A0 nhan alpha/255 ma server dat alpha
//          0/1); SetTransmitState 0x100569F0 tra ve khi edict NULL;
//          FindEntityByName 0x100B4F20 khong loc edict nen LightOn/LightOff van chay.
//   HaloScale != 60, CO parentname -> giu nguyen Valve (3 edict). 2/1100 cai.
//
// MOC (khong detour inline - chi va lenh call rel32, danh sach call tu kiem luc nap):
//   MapEntity_ParseEntity  0x101198F0  cdecl (CBaseEntity*&, const char* data,
//       IMapEntityFilter*) -> const char*. Goi tu MapEntity_ParseAllEntities
//       0x1011A6DD (nap map + CleanUpMap/wipe) va CPointTemplate::CreateInstance
//       0x101912B2. Quet ca .text: DUNG 2 call, 0 jmp, 0 con tro tuyet doi.
//   CPointSpotlight::SpotlightCreate 0x1018E3D0  thiscall, khong tham so.
//       Goi tu 0x1018F1A2 (CreateEfficientSpotlight), 0x1018ECB3, 0x1018ED08 (think).
//       Quet ca .text: DUNG 3 call, 0 jmp, 0 con tro.
//   PostConstructor (vtable slot 29) cua CPointSpotlight (vt RVA 0x630E64) va
//       CSpotlightEnd (vt RVA 0x643B54) - hai vtable rieng, khong lop nao dung chung.
#include <math.h>
#include <stdlib.h>
static int g_Spotlight = 0;     // 0 tat | 1 chi quan sat (ghi log) | 2 lam that

#define SPOT_PARSE_RVA     0x1198F0u
#define SPOT_CREATE_RVA    0x18E3D0u
#define SPOT_POSTCTOR_RVA  0x55620u
#define SPOT_VT_RVA        0x630E64u
#define SPOT_END_VT_RVA    0x643B54u
#define EFL_SERVER_ONLY_BIT (1u << 9)

typedef const char* (__cdecl    *ParseEntity_t)(void**, const char*, void*);
typedef void        (__fastcall *SpotCreate_t)(void*, void*);

static uint8_t*      g_SpotBase = NULL;
static ParseEntity_t g_OrigParseEntity = NULL;
static SpotCreate_t  g_OrigSpotCreate  = NULL;
static PostCtor_t    g_SpotOrigPostCtor = NULL;
static void**        g_SpotVt    = NULL;
static void**        g_SpotEndVt = NULL;

// Moc dictionary Create RIENG cua module (KHONG sua swap.cpp). Noi chuoi: slot 1
// dang tro gi (Create goc, hoac moc cua swap neu swap da cai truoc) thi goi tiep cai do.
// Khi module da quyet cho MOT point_spotlight (g_SpotLopToi) thi goi THANG Create goc
// 0x10206A40 de swap.txt khong doi lop lan nua.
#define DICT_VT_RVA     0x654F48u
#define DICT_CREATE_RVA 0x206A40u
typedef void* (__fastcall *SpotDictCreate_t)(void*, void*, const char*);
static void**           g_SpotDictVt  = NULL;
static SpotDictCreate_t g_SpotDictKe  = NULL;   // gia tri slot 1 truoc khi moc
static SpotDictCreate_t g_SpotDictGoc = NULL;   // Create goc cua game
static const char*      g_SpotLopToi  = NULL;   // lop tao thay cho point_spotlight ke tiep

// Cac cho call da va - de go khi unload.
#define SPOT_MAX_CALL 4
struct ChoGoi { uint8_t* p; int32_t relGoc; };
static ChoGoi g_ChoParse[SPOT_MAX_CALL];  static int g_SoChoParse  = 0;
static ChoGoi g_ChoCreate[SPOT_MAX_CALL]; static int g_SoChoCreate = 0;

// Co MOT LAN, dat ngay truoc khi tao, xoa ngay sau.
static bool g_SpotServerOnlyToi = false;   // point_spotlight sap tao -> EFL_SERVER_ONLY
static bool g_EndServerOnlyToi  = false;   // spotlight_end sap tao  -> EFL_SERVER_ONLY

// Dem theo map (dat lai o LevelInit, bao o ServerActivate).
static int g_SpotGap = 0, g_SpotDoiBeam = 0, g_SpotKhongEdict = 0, g_SpotGiu = 0;
static int g_SpotEndKhongEdict = 0;
static char g_SpotMapName[64] = "?";     // CHEP ra: con tro pMapName bi ghi de khi sang map sau

// --------------------------------------------------------------------------
// Doc khoi keyvalue cua MOT thuc the (bat dau ngay sau '{', hoac co '{' o dau).
// Chi lay classname / parentname / HaloScale. Dung o '}' ngoai ngoac kep.
// (nonetkill.cpp RewriteLump cung doc chuoi lump nhung theo cach khac: quet CA lump
//  tim "classname" de ghi de. Hai bo doc doc lap, khong dung chung.)
// --------------------------------------------------------------------------
static bool DocKhoiSpotlight(const char* p, bool* laSpot, bool* coCha, bool* coHalo, float* halo) {
    *laSpot = false; *coCha = false; *coHalo = false; *halo = 60.0f;
    if (!p) return false;
    char tok[2][256];
    int n = 0;
    const char* gioiHan = p + 65536;               // chan an toan: mot khoi khong dai the
    while (p < gioiHan && *p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '{') p++;
        if (!*p || *p == '}') break;
        if (*p != '"') { p++; continue; }
        const char* s = ++p;
        while (*p && *p != '"' && p < gioiHan) p++;
        size_t L = (size_t)(p - s);
        if (L >= sizeof(tok[0])) L = sizeof(tok[0]) - 1;
        memcpy(tok[n], s, L); tok[n][L] = 0;
        if (*p == '"') p++;
        if (++n == 2) {
            if (!_stricmp(tok[0], "classname"))      *laSpot = !_stricmp(tok[1], "point_spotlight");
            else if (!_stricmp(tok[0], "parentname")) *coCha  = tok[1][0] != 0;
            else if (!_stricmp(tok[0], "HaloScale"))  { *coHalo = true; *halo = (float)atof(tok[1]); }
            n = 0;
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// Moc
// --------------------------------------------------------------------------
static const char* __cdecl Hook_ParseEntity(void** ppEnt, const char* data, void* filter) {
    bool laSpot, coCha, coHalo; float halo;
    if (g_Spotlight >= 1 && DocKhoiSpotlight(data, &laSpot, &coCha, &coHalo, &halo) && laSpot) {
        g_SpotGap++;
        bool halo60 = !coHalo || fabsf(halo - 60.0f) < 0.001f;
        if (halo60) {
            g_SpotDoiBeam++;
            if (g_Spotlight >= 2) g_SpotLopToi = "beam_spotlight";
        } else if (!coCha) {
            g_SpotKhongEdict++;
            if (g_Spotlight >= 2) {
                // giu dung lop (bo qua swap.txt neu co) + khong edict
                g_SpotLopToi = "point_spotlight";
                g_SpotServerOnlyToi = true;
            }
        } else {
            g_SpotGiu++;
            if (g_Spotlight >= 2) g_SpotLopToi = "point_spotlight";
        }
    }
    const char* r = g_OrigParseEntity(ppEnt, data, filter);
    // Bo loc (CleanUpMap) co the KHONG tao => khong de co treo sang thuc the sau.
    g_SpotServerOnlyToi = false;
    g_SpotLopToi = NULL;
    return r;
}

// SONG SONG voi swap.cpp Hook_DictCreate tren CUNG slot 1 cua dictionary.
// Chuoi luc chay:  engine -> Hook_SpotDictCreate -> g_SpotDictKe (= Hook_DictCreate
// cua swap neu swap da cai, khong thi Create goc) -> Create goc.
// Chi khi g_SpotLopToi dang dat (ngay trong Hook_ParseEntity, cho DUNG point_spotlight
// sap tao) moi re thang sang Create goc; moi lop khac va moi lan tao luc chay di
// tiep qua swap nhu cu.
static void* __fastcall Hook_SpotDictCreate(void* thisptr, void* edx, const char* cls) {
    if (g_SpotLopToi) {
        const char* thanh = g_SpotLopToi;
        g_SpotLopToi = NULL;
        if (cls && !_stricmp(cls, "point_spotlight"))
            return g_SpotDictGoc(thisptr, edx, thanh);
    }
    return g_SpotDictKe(thisptr, edx, cls);
}

static void __fastcall Hook_SpotCreate(void* thisptr, void* edx) {
    // Chi khi CHINH point_spotlight nay da la server-only (do luat o tren) thi
    // spotlight_end cua no cung server-only. Doc thang m_iEFlags (+0x138) bit 9.
    if (g_Spotlight >= 2 && thisptr &&
        (*(unsigned int*)((uint8_t*)thisptr + 0x138) & EFL_SERVER_ONLY_BIT))
        g_EndServerOnlyToi = true;
    g_OrigSpotCreate(thisptr, edx);
    g_EndServerOnlyToi = false;
}

// SONG SONG voi noedict.cpp Hook_PostCtor: cung PostConstructor (slot 29), cung ghi
// EFL_SERVER_ONLY (+0x138 bit 9). Khac: noedict bat cho MOI entity cua vtable da sua;
// ham nay chi bat cho entity dang co co mot-lan (g_SpotServerOnlyToi/g_EndServerOnlyToi)
// - point_spotlight khong co co thi chay y nhu Valve. Hai module sua hai vtable khac
// nhau; cong 3 trong InstallSpotlight tu choi neu noedict da sua slot 29 cua chung.
static void __fastcall Hook_PostCtorSpot(void* thisptr, void* edx, const char* cls) {
    if (thisptr && cls) {
        if (g_SpotServerOnlyToi && !_stricmp(cls, "point_spotlight")) {
            *(unsigned int*)((uint8_t*)thisptr + 0x138) |= EFL_SERVER_ONLY_BIT;
            g_SpotServerOnlyToi = false;
        } else if (g_EndServerOnlyToi && !_stricmp(cls, "spotlight_end")) {
            *(unsigned int*)((uint8_t*)thisptr + 0x138) |= EFL_SERVER_ONLY_BIT;
            g_EndServerOnlyToi = false;
            g_SpotEndKhongEdict++;
        }
    }
    g_SpotOrigPostCtor(thisptr, edx, cls);
}

// --------------------------------------------------------------------------
// Cai dat - moi cong an toan truot la BO QUA ca module, ghi ro ly do.
// --------------------------------------------------------------------------
// Quet toan bo .text cua server.dll tim moi E8/E9 rel32 tro toi `dich`.
static int QuetCall(uint8_t* base, size_t size, uint8_t* dich, ChoGoi* out, int toiDa, int* soJmp) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
    int n = 0; *soJmp = 0;
    for (int s = 0; s < nt->FileHeader.NumberOfSections; s++, sh++) {
        if (memcmp(sh->Name, ".text", 5) != 0) continue;
        uint8_t* a = base + sh->VirtualAddress;
        size_t len = sh->Misc.VirtualSize;
        if ((size_t)(a - base) + len > size) return -1;
        for (size_t i = 0; i + 5 <= len; i++) {
            if (a[i] != 0xE8 && a[i] != 0xE9) continue;
            int32_t rel = *(int32_t*)(a + i + 1);
            if (a + i + 5 + rel != dich) continue;
            if (a[i] == 0xE9) { (*soJmp)++; continue; }
            if (n < toiDa) { out[n].p = a + i; out[n].relGoc = rel; }
            n++;
        }
    }
    return n;
}

static void VaCall(ChoGoi* c, void* thunk) {
    int32_t rel = (int32_t)((uint8_t*)thunk - (c->p + 5));
    WriteProtected(c->p + 1, &rel, sizeof(rel));
}

static bool InstallSpotlight() {
    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* base = (uint8_t*)mi.lpBaseOfDll;
    g_SpotBase = base;
    if (!g_SrvBase) { g_SrvBase = base; g_SrvSize = mi.SizeOfImage; }   // TrongSrv() cua noedict.cpp

    // Cong 1: prologue ba ham dich. kPost trung byte voi kSig cua InstallNoEdict
    // (noedict.cpp) - cung mot PostConstructor RVA 0x55620.
    static const uint8_t kParse[]  = { 0x55,0x8B,0xEC,0xB8,0x10,0x18,0x00,0x00,0xE8 };
    static const uint8_t kCreate[] = { 0x55,0x8B,0xEC,0x83,0xEC,0x6C,0xA1 };
    static const uint8_t kPost[]   = { 0x55,0x8B,0xEC,0x8B,0x45,0x08,0x56,0x8B,0xF1,0x85,0xC0 };
    uint8_t* pParse  = base + SPOT_PARSE_RVA;
    uint8_t* pCreate = base + SPOT_CREATE_RVA;
    uint8_t* pPost   = base + SPOT_POSTCTOR_RVA;
    if (memcmp(pParse, kParse, sizeof(kParse)) || memcmp(pCreate, kCreate, sizeof(kCreate)) ||
        memcmp(pPost, kPost, sizeof(kPost))) {
        EL_LOG("[EdictBudget] SPOTLIGHT: prologue ParseEntity/SpotlightCreate/PostConstructor "
               "KHONG khop - server.dll khac ban? BO QUA");
        return false;
    }

    // Cong 2: dem MOI cho goi trong .text - phai DUNG 2 va 3, khong co jmp.
    // Them/bot mot cho goi (game cap nhat) => co duong tao thuc the ma moc khong thay => tu choi.
    int jp = 0, jc = 0;
    g_SoChoParse  = QuetCall(base, mi.SizeOfImage, pParse,  g_ChoParse,  SPOT_MAX_CALL, &jp);
    g_SoChoCreate = QuetCall(base, mi.SizeOfImage, pCreate, g_ChoCreate, SPOT_MAX_CALL, &jc);
    if (g_SoChoParse != 2 || g_SoChoCreate != 3 || jp || jc) {
        EL_LOG("[EdictBudget] SPOTLIGHT: so cho goi khac du kien (ParseEntity %d call/%d jmp, can 2/0; "
               "SpotlightCreate %d call/%d jmp, can 3/0) - BO QUA",
               g_SoChoParse, jp, g_SoChoCreate, jc);
        g_SoChoParse = g_SoChoCreate = 0;
        return false;
    }

    // Cong 3: vtable hai lop qua factory, dung RVA du kien, slot 29 = PostConstructor
    // (chua bi noedict hay ai khac sua).
    g_SpotVt    = ResolveClassVtable(base, "point_spotlight");
    g_SpotEndVt = ResolveClassVtable(base, "spotlight_end");
    if (g_SpotVt != (void**)(base + SPOT_VT_RVA) || g_SpotEndVt != (void**)(base + SPOT_END_VT_RVA)) {
        EL_LOG("[EdictBudget] SPOTLIGHT: vtable point_spotlight=%p spotlight_end=%p khac du kien "
               "(%p / %p) - BO QUA", (void*)g_SpotVt, (void*)g_SpotEndVt,
               (void*)(base + SPOT_VT_RVA), (void*)(base + SPOT_END_VT_RVA));
        return false;
    }
    if (g_SpotVt[29] != (void*)pPost || g_SpotEndVt[29] != (void*)pPost) {
        EL_LOG("[EdictBudget] SPOTLIGHT: slot29 da bi sua (noedict.txt co point_spotlight/"
               "spotlight_end?) - BO QUA de khong chong cheo");
        return false;
    }

    // Cong 4: lop dich cua luat 1 phai ton tai.
    // (GetEntityFactoryDictionary base+0x20CA70 duoc goi ca o cong 4 va cong 5 -
    //  thua mot lan goi nhung vo hai; cung dia chi voi swap.cpp va ResolveClassVtable.)
    {
        typedef void* (__cdecl *GetDict_t)(void);
        typedef void* (__thiscall *FindFactory_t)(void*, const char*);
        void* dict = ((GetDict_t)(base + 0x20CA70))();
        void** dvt = dict ? *(void***)dict : NULL;
        if (!dvt || !((FindFactory_t)dvt[3])(dict, "beam_spotlight")) {
            EL_LOG("[EdictBudget] SPOTLIGHT: khong co factory beam_spotlight - BO QUA");
            return false;
        }
    }
    // Cong 5: dictionary dung vtable CEntityFactoryDictionary (RVA 0x654F48), Create goc
    // (RVA 0x206A40) dung prologue; slot 1 hien tai la Create goc HOAC moc cua swap.
    {
        static const uint8_t kDictCreate[] = { 0x55,0x8B,0xEC,0x8B,0x01,0x8B,0x50,0x0C,0x56,0x8B,0x75,0x08 };
        typedef void* (__cdecl *GetDict_t)(void);
        void* dict = ((GetDict_t)(base + 0x20CA70))();
        void** dvt = dict ? *(void***)dict : NULL;
        uint8_t* goc = base + DICT_CREATE_RVA;
        if (dvt != (void**)(base + DICT_VT_RVA) || memcmp(goc, kDictCreate, sizeof(kDictCreate))) {
            EL_LOG("[EdictBudget] SPOTLIGHT: dictionary vtable=%p (can %p) hoac prologue Create goc "
                   "khong khop - BO QUA", (void*)dvt, (void*)(base + DICT_VT_RVA));
            return false;
        }
        g_SpotDictVt  = dvt;
        g_SpotDictGoc = (SpotDictCreate_t)goc;
    }

    // Lap moc.
    g_OrigParseEntity  = (ParseEntity_t)pParse;
    g_OrigSpotCreate   = (SpotCreate_t)pCreate;
    g_SpotOrigPostCtor = (PostCtor_t)pPost;
    for (int i = 0; i < g_SoChoParse; i++)  VaCall(&g_ChoParse[i],  (void*)&Hook_ParseEntity);
    if (g_Spotlight >= 2) {
        g_SpotDictKe = (SpotDictCreate_t)g_SpotDictVt[1];     // Create goc hoac moc cua swap
        void* dthunk = (void*)&Hook_SpotDictCreate;
        WriteProtected(&g_SpotDictVt[1], &dthunk, sizeof(void*));
        for (int i = 0; i < g_SoChoCreate; i++) VaCall(&g_ChoCreate[i], (void*)&Hook_SpotCreate);
        void* thunk = (void*)&Hook_PostCtorSpot;
        WriteProtected(&g_SpotVt[29],    &thunk, sizeof(void*));
        WriteProtected(&g_SpotEndVt[29], &thunk, sizeof(void*));
    } else {
        g_SoChoCreate = 0;                 // che do 1 khong va SpotlightCreate
    }
    EL_LOG("[EdictBudget] SPOTLIGHT: da moc (che do %d - %s): ParseEntity @%p (2 cho goi), "
           "SpotlightCreate @%p (%d cho goi), PostConstructor vt %p / %p",
           g_Spotlight, g_Spotlight >= 2 ? "LAM THAT" : "CHI QUAN SAT",
           (void*)pParse, (void*)pCreate, g_SoChoCreate, (void*)g_SpotVt, (void*)g_SpotEndVt);
    return true;
}

static void UninstallSpotlight() {
    for (int i = 0; i < g_SoChoParse; i++)
        WriteProtected(g_ChoParse[i].p + 1, &g_ChoParse[i].relGoc, sizeof(int32_t));
    for (int i = 0; i < g_SoChoCreate; i++)
        WriteProtected(g_ChoCreate[i].p + 1, &g_ChoCreate[i].relGoc, sizeof(int32_t));
    g_SoChoParse = g_SoChoCreate = 0;
    void* orig = (void*)g_SpotOrigPostCtor;
    if (orig) {
        if (g_SpotVt && g_SpotVt[29] == (void*)&Hook_PostCtorSpot)
            WriteProtected(&g_SpotVt[29], &orig, sizeof(void*));
        if (g_SpotEndVt && g_SpotEndVt[29] == (void*)&Hook_PostCtorSpot)
            WriteProtected(&g_SpotEndVt[29], &orig, sizeof(void*));
    }
    g_SpotOrigPostCtor = NULL;
    if (g_SpotDictVt && g_SpotDictKe && g_SpotDictVt[1] == (void*)&Hook_SpotDictCreate) {
        void* ke = (void*)g_SpotDictKe;
        WriteProtected(&g_SpotDictVt[1], &ke, sizeof(void*));
    }
    g_SpotDictKe = NULL;
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// Cai luc Load: lump cua map dau tien duoc phan tich SAU Load.
static void Spotlight_OnLoad() { if (g_Spotlight > 0) InstallSpotlight(); }

// spotlight_end sinh trong Activate() (CreateEfficientSpotlight) va khi LightOn - deu SAU
// moc ServerActivate (moc TRUOC, sample_mm.cpp dang ky false). Do 09/10/2026: dong
// ServerActivate luon in 0. Nen so spotlight_end khong edict bao vao luc HET MAP.
static void SpotlightBaoHetMap() {
    if (g_Spotlight >= 2 && g_SpotGap > 0)
        EL_LOG("[EdictBudget] SPOTLIGHT [%s] het map: spotlight_end khong edict %d",
               g_SpotMapName, g_SpotEndKhongEdict);
}
static void Spotlight_OnLevelInit(const char* pMapName) {
    SpotlightBaoHetMap();
    g_SpotGap = g_SpotDoiBeam = g_SpotKhongEdict = g_SpotGiu = g_SpotEndKhongEdict = 0;
    strncpy(g_SpotMapName, pMapName ? pMapName : "?", sizeof(g_SpotMapName) - 1);
    g_SpotMapName[sizeof(g_SpotMapName) - 1] = 0;
}
static void Spotlight_OnServerActivate() {
    if (g_Spotlight <= 0 || g_SpotGap == 0) return;
    EL_LOG("[EdictBudget] SPOTLIGHT [%s] (%s): %d point_spotlight | doi beam_spotlight %d | "
           "giu + khong edict %d | giu nguyen %d (spotlight_end: xem dong het map)",
           g_SpotMapName, g_Spotlight >= 2 ? "lam that" : "chi quan sat",
           g_SpotGap, g_SpotDoiBeam, g_SpotKhongEdict, g_SpotGiu);
}
// KHONG ghi log o day: module chinh dong log (EL_LOG_CLOSE) TRUOC khi goi ham nay;
// EL_LOG luc nay se mo lai file ma khong ai dong. So "het map" cua map cuoi bi bo.
static void Spotlight_OnUnload() { UninstallSpotlight(); }
