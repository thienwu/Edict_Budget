// ==========================================================================
//  diachi - BANG DIA CHI server.dll THEO TUNG BAN GAME + cong nhan ban
//  Chi duoc #include tu sample_mm.cpp, SAU nhatky.cpp va TRUOC moi module dung
//  dia chi ghi cung (wipeclear, mapclear, noedict, swap, spotlight).
// ==========================================================================
// VI SAO (26/11/2026, docs/03-loi-da-biet.md muc 13): Valve cap nhat 25/11 doi cho ham trong
// server.dll. Module swap GOI THANG base+0x20CA70 (EntityFactoryDictionary ban cu)
// truoc moi cong => ban moi dia chi do roi giua lenh => SAP luc nap plugin.
//
// Cach lam:
//   - Moi ban game mot dong trong kBan[], nhan bang TimeDateStamp + SizeOfImage cua
//     PE header server.dll (doc trong bo nho, khong mo file).
//   - Ban LA (khong co trong bang) => g_DC = NULL => DiaChi_OnLoad tat moi module
//     dung dia chi ghi cung (noedict, swap, spotlight, wipeclear, mapclear) va ghi log.
//     Module tu do chu ky / neo (freegate, trap, hopbao, bien engine) van chay.
//   - Moi module VAN giu cong rieng (prologue, vtable, so cho goi): bang chi cho dia
//     chi, cong cua module moi quyet co dong vao hay khong.
//
// Them ban moi: khop tung ham ban cu sang ban moi (che relocation + rel32), so TOAN THAN
// cac ham (docs/04-dia-chi.md muc "Suy lai sau khi cap nhat"), roi them mot dong vao
// kBan[]. Dia chi = RVA (ImageBase
// 0x10000000 da tru).

struct DiaChiBan {
    const char* ten;
    uint32_t ts;                 // IMAGE_FILE_HEADER.TimeDateStamp cua server.dll
    uint32_t sizeImage;          // IMAGE_OPTIONAL_HEADER.SizeOfImage
    // ham
    uint32_t getDict;            // EntityFactoryDictionary()        (cdecl, tra con tro tinh)
    uint32_t postCtor;           // CBaseEntity::PostConstructor      (vtable slot 29)
    uint32_t restartRound;       // CTerrorGameRules::RestartRound    (vtable slot 178)
    uint32_t gameRulesSlot38;    // CTerrorGameRules slot 38 (chuan bi chuyen man)
    uint32_t cleanupDeleteList;  // CleanupDeleteList                 (cdecl, khong tham so)
    uint32_t nextEnt;            // CGlobalEntityList::NextEnt        (thiscall, ret 4)
    uint32_t utilRemove;         // UTIL_Remove                       (cdecl)
    uint32_t parseEntity;        // MapEntity_ParseEntity             (cdecl)
    uint32_t spotlightCreate;    // CPointSpotlight::SpotlightCreate  (thiscall)
    uint32_t dictCreate;         // CEntityFactoryDictionary::Create  (vtable slot 1)
    // du lieu
    uint32_t dtBaseEntity;       // ServerClass DT_BaseEntity (gia tri GetServerClass tra ve)
    uint32_t gameRules;          // g_pGameRules (con tro)
    uint32_t entList;            // gEntList
    uint32_t inCleanupDelete;    // g_fInCleanupDelete (bool)
    uint32_t preserveList;       // danh sach 38 lop giu lai khi CleanUpMap
    uint32_t vtSpotlight;        // vtable CPointSpotlight
    uint32_t vtSpotlightEnd;     // vtable CSpotlightEnd
    uint32_t vtDict;             // vtable CEntityFactoryDictionary
};

static const DiaChiBan kBan[] = {
    { "server.dll 533888fb (truoc 25/11/2026)", 0x6A440E53, 0x982000,
      0x20CA70, 0x55620, 0x2E0650, 0x2B8140, 0x0B5D10, 0x0B4270, 0x2071E0, 0x1198F0, 0x18E3D0, 0x206A40,
      0x7D78A8, 0x7F7F6C, 0x7E0760, 0x7E0730, 0x7ACE40, 0x630E64, 0x643B54, 0x654F48 },
    { "server.dll 5e4d1e32 (cap nhat 25/11/2026)", 0x6AFCCCA0, 0x982000,
      0x20CBF0, 0x55610, 0x2E0750, 0x2B8230, 0x0B5DE0, 0x0B4340, 0x207370, 0x1199A0, 0x18E5B0, 0x206BD0,
      0x7D78A8, 0x7F7F6C, 0x7E0760, 0x7E0730, 0x7ACE40, 0x630EBC, 0x643BAC, 0x654FA0 },
};

static const DiaChiBan* g_DC = NULL;     // NULL = ban la / chua nhan

// Doc PE header cua server.dll da nap, tim dong khop trong kBan[].
static const DiaChiBan* NhanBanServer(uint32_t* tsOut, uint32_t* sizeOut) {
    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return NULL;
    uint8_t* base = (uint8_t*)h;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    *tsOut = nt->FileHeader.TimeDateStamp;
    *sizeOut = nt->OptionalHeader.SizeOfImage;
    for (size_t i = 0; i < sizeof(kBan) / sizeof(kBan[0]); i++)
        if (kBan[i].ts == *tsOut && kBan[i].sizeImage == *sizeOut) return &kBan[i];
    return NULL;
}

// EntityFactoryDictionary() - KIEM prologue TRUOC khi goi (ban cu: goi thang => sap khi
// dia chi lech). Than ham: mov eax,1 ; test byte [guard],al ; jne ... (khoi tao tinh
// mot lan roi tra con tro dictionary). Dung chung: noedict, swap, spotlight.
static void* LayFactoryDictionary(uint8_t* base) {
    if (!g_DC) return NULL;
    static const uint8_t kSig[]  = { 0xB8,0x01,0x00,0x00,0x00,0x84,0x05, 0,0,0,0, 0x75 };
    static const char    kMask[] = "xxxxxxx????x";
    uint8_t* fn = base + g_DC->getDict;
    for (size_t i = 0; i < sizeof(kSig); i++)
        if (kMask[i] == 'x' && fn[i] != kSig[i]) {
            EL_LOG("[EdictBudget] DIACHI: EntityFactoryDictionary @%p prologue KHONG khop - khong goi", fn);
            return NULL;
        }
    typedef void* (__cdecl *GetDict_t)(void);
    return ((GetDict_t)fn)();
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// Goi SAU LoadPatchSwitches va TRUOC moi module dung dia chi. Tra false = ban la:
// module chinh tat cong tac cua cac module dung dia chi ghi cung (cac bien do khai bao
// trong module dung SAU file nay nen o day khong tat duoc).
static bool DiaChi_OnLoad() {
    uint32_t ts = 0, sz = 0;
    g_DC = NhanBanServer(&ts, &sz);
    if (g_DC) {
        EL_LOG("[EdictBudget] DIACHI: %s (TimeDateStamp=%08X SizeOfImage=%X) - dung bang dia chi cua ban nay",
               g_DC->ten, ts, sz);
        return true;
    }
    EL_LOG("[EdictBudget] !! DIACHI: server.dll BAN LA (TimeDateStamp=%08X SizeOfImage=%X) - khong co trong bang.", ts, sz);
    EL_LOG("[EdictBudget] !! DIACHI: TAT noedict, swap, spotlight, wipeclear, mapclear (dung dia chi ghi cung). "
           "freegate, trap, hopbao tu do chu ky nen van chay. Can do lai dia chi: docs/04-dia-chi.md");
    return false;
}
