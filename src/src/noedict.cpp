// ==========================================================================
//  noedict - EFL_SERVER_ONLY qua PostConstructor (vtable slot 29)
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// noedict: bat EFL_SERVER_ONLY cho cac lop trong noedict.txt => chung KHONG duoc
// cap edict ma nam o dai thuc the khong mang cua bang entity (thiet ke GOC cua
// engine). Xem khoi giai thich day du gan InstallNoEdict().
static bool g_NoEdict   = false;

// Con tro ServerClass ma GetServerClass() tra ve khi lop KHONG co SendTable rieng
// (tuc la dung chung DT_BaseEntity). Xem cong an toan 3 trong InstallNoEdict().
//
// !!  DAY LA DIA CHI TRONG ANH TINH (ImageBase 0x10000000). Luc chay server.dll nap
//    o base khac, con tro doc tu vtable DA DUOC DOI THEO BASE.
//    Phai so voi  base + (0x107D78A8 - 0x10000000)  chu KHONG so thang.
//
//    Ban 16:01 ngay 14/08 so thang -> ca 4 lop dang chay (infodecal/light/light_spot/
//    path_track) deu bi TU CHOI, noedict tat hoan toan, "da sua 0 vtable / 4 lop".
//    Log cho thay ca bon deu tra 0x540378A8, base that la 0x53860000:
//        0x53860000 + 0x7D78A8 = 0x540378A8   <- dung, chi la chua doi hang so.
//    Cung loai loi voi vu chu ky mapclue bi tu choi vi prologue chua mat na.
#define DT_BASEENTITY_RVA 0x7D78A8u

// NOEDICT - khien mot so lop KHONG BAO GIO duoc cap edict, bang cach dat
// EFL_SERVER_ONLY (bit 9 cua m_iEFlags) trong PostConstructor (vtable slot 29).
// Day la co che MANH NHAT va mat mat bang 0. Xem docs/01-co-che.md
typedef void (__fastcall *PostCtor_t)(void*, void*, const char*);
static PostCtor_t g_OrigPostCtor = NULL;

// 02/10/2026: 32 -> 1024. Ban cu dung vong doc o dong thu 32 ma KHONG bao gi het,
// nen bai thu "dua moi lop chua server-only vao noedict" (511 lop) se chi thu
// 32 lop dau va bo phan con lai IM LANG. Gio vuot tran thi co canh bao (xem duoi).
#define NOEDICT_MAX 1024
static char  g_NoEdictList[NOEDICT_MAX][40];
static int   g_NoEdictCount = 0;
static void* g_PatchedVt[NOEDICT_MAX];      // vtable da sua, de go khi unload
static int   g_PatchedVtCount = 0;

// Bo doc danh sach mot-lop. CUNG KHUON (chep tay, chua gop) voi LoadWipeKeep
// (wipeclear), LoadFreeKeep (freegate), LoadMapKill + LoadMapForceKill (mapclear),
// LoadKillList (nonetkill). Sua cach doc o day thi kiem ca nam ban kia.
static void LoadNoEdictList() {
    g_NoEdictCount = 0;
    // !!  MAC DINH RONG - co y. Mac dinh cung tay cua nonetkill
    //    ({infodecal,light,light_spot}) lam sai anh sang ch04_pripyat03.
    //    Khong co noedict.txt = khong dong vao lop nao.
    FILE* f = OpenPluginFile("noedict.txt", "r");
    if (!f) {
        EL_LOG("[EdictBudget] NOEDICT: khong co noedict.txt - khong lam gi");
        return;
    }
    char line[256];
    int bo_vuot_tran = 0, bo_ten_dai = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t L = strlen(line);
        if (L > 0 && line[L-1] != '\n') { int c; while ((c=fgetc(f)) != EOF && c != '\n') {} }
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
        char* e = p + strlen(p) - 1;
        while (e >= p && (*e=='\r'||*e=='\n'||*e==' '||*e=='\t')) *e-- = 0;
        if (!*p) continue;
        bool ok = true;
        for (const char* q = p; *q; q++)
            if (!((*q>='a'&&*q<='z')||(*q>='A'&&*q<='Z')||(*q>='0'&&*q<='9')||*q=='_')) { ok=false; break; }
        if (!ok) continue;
        // Hai cho truoc day CAT IM LANG - gio dem va bao.
        if (g_NoEdictCount >= NOEDICT_MAX) { bo_vuot_tran++; continue; }
        if (strlen(p) >= sizeof(g_NoEdictList[0])) {
            EL_LOG("[EdictBudget] NOEDICT: '%s' dai %d ky tu >= %d - BO QUA (khong cat ten)",
                   p, (int)strlen(p), (int)sizeof(g_NoEdictList[0]));
            bo_ten_dai++; continue;
        }
        strncpy(g_NoEdictList[g_NoEdictCount], p, sizeof(g_NoEdictList[0])-1);
        g_NoEdictList[g_NoEdictCount][sizeof(g_NoEdictList[0])-1] = 0;
        g_NoEdictCount++;
    }
    fclose(f);
    if (bo_vuot_tran)
        EL_LOG("[EdictBudget] NOEDICT: CANH BAO - %d lop VUOT TRAN NOEDICT_MAX=%d, KHONG duoc nap",
               bo_vuot_tran, NOEDICT_MAX);
    EL_LOG("[EdictBudget] NOEDICT: %d lop trong danh sach (bo vi vuot tran %d, vi ten dai %d)",
           g_NoEdictCount, bo_vuot_tran, bo_ten_dai);
    for (int i = 0; i < g_NoEdictCount; i++)
        EL_LOG("[EdictBudget] NOEDICT:   [%d] '%s'", i, g_NoEdictList[i]);
}

static int g_NoEdictHits = 0;

// SONG SONG voi spotlight.cpp Hook_PostCtorSpot: cung PostConstructor (slot 29), cung
// bit. Ham NAY khong xem classname - moi entity cua vtable da sua deu thanh server-only.
// Vi vay noedict cat theo VTABLE: lop khac dung chung vtable bi keo theo (xem
// ResolveClassVtable ben duoi, doan "BAY LON HON").
static void __fastcall Hook_PostCtor(void* thisptr, void* edx, const char* cls) {
    if (thisptr) {
        // bit 9 = EFL_SERVER_ONLY. Bat TRUOC khi ham goc doc no.
        *(unsigned int*)((uint8_t*)thisptr + 0x138) |= (1u << 9);
        g_NoEdictHits++;
    }
    if (g_OrigPostCtor) g_OrigPostCtor(thisptr, edx, cls);
}

// Quet mot ham tim  mov dword ptr [reg], imm32  (= luu con tro vtable).
// Tra NULL neu khong thay, HOAC thay nhieu hon mot (khong chac chan -> bo).
// !! VA 26/09/2026 - CONG DUNG O `ret`/`int3`. TRUOC DO QUET LAN SANG HAM KE TIEP.
//
//    Ban cu quet 80 byte PHANG, khong dung o cuoi ham. Voi lop nao co Create la
//    thunk ngan thi no doc sang ham LIEN SAU va nhat vtable cua LOP KHAC.
//
//    Ca cu the da do duoc - upgrade_spawn:
//        Create 0x1029EDB0 dai DUNG 0x16 byte, ket thuc `ret 4` @0x1029EDC5,
//        roi 8 byte int3 dem, roi 0x1029EDD0 la ctor cua weapon_melee_spawn:
//            1029EDF3  mov dword ptr [esi], 0x1067A18C   <- vtable weapon_melee_spawn
//        Offset 0x43 < 80 nen ban cu BAT duoc no. Va vi chi bat duoc DUNG MOT,
//        luat "nhieu hon mot thi tu choi" KHONG kich hoat => tra ve vtable SAI.
//
//    Da xac minh tren MAY CHU THAT 26/09 15:15 - cong DK1 bat duoc va tu choi:
//        'upgrade_spawn' CO SENDTABLE RIENG (ServerClass=0x6E1B0BF0,
//         can 0x6E1478A8) - TRUOT DIEU KIEN 1, TU CHOI
//        base luc chay = 0x5D970000 => 0x6E1B0BF0 - base = 0x10840BF0
//        0x10840BF0 chinh la ServerClass cua weapon_melee_spawn (vtable
//        0x1067A18C, slot9 0x1029CA90), KHONG phai cua upgrade_spawn (vtable
//        0x10679E34, slot9 0x10050CF0 -> 0x107D78A8 = DT_BaseEntity).
//    => DK1 da cuu, nhung no chi cuu vi lop bi nham CO SendTable rieng. Neu lop
//       nam ke ben KHONG co SendTable rieng thi se va nham ma khong ai biet.
//
//    Sau khi va: quet dung o `ret 4` nen khong thay gi -> tra NULL -> nhanh du
//    phong E8 chay, va ham con 0x1029E860 CO `mov [esi], 0x10679E34` = dung
//    vtable upgrade_spawn. Tuc ban va vua bit duoc lo vua GIAI DUNG them lop.
// !! VA 02/10/2026 - CHAN BIEN TREN. srcds_crash_2026_10_2T4_14_18C0: AV doc 0x72F54760
//    tai ScanForVtableStore+0x10, lop logic_choreographed_scene. Nhanh du phong coi MOI
//    byte 0xE8 la `call rel32`, ke ca 0xE8 nam TRONG rel32 cua call truoc (Create+0x0C,
//    giua `call operator new` o +0x09), roi chi kiem `target >= base` -> con tro rac
//    vuot het server.dll. Mo phong tung byte tren 557 lop (tools/mophong_resolve_vtable.py):
//    9 lop lam sap - beam_spotlight func_breakable logic_choreographed_scene
//    logic_scene_list_manager scene_manager scripted_scene sky_camera
//    weapon_basecsgrenade weapon_hunter_claw. Sau khi va: 0 sap, DUNG 505->533,
//    SAI van 8, 0 lop tu dung thanh sai, 20 lop dang chay giai GIONG HET.
static uint8_t* g_SrvBase = NULL;
static size_t   g_SrvSize = 0;
static bool TrongSrv(const void* p, size_t n) {
    uintptr_t a = (uintptr_t)p, b = (uintptr_t)g_SrvBase;
    return g_SrvBase && a >= b && a + n >= a && a + n <= b + g_SrvSize;
}

static void** ScanForVtableStore(uint8_t* base, uint8_t* fn) {
    void** found = NULL;
    if (!TrongSrv(fn, 80 + 6)) return NULL;
    for (int i = 0; i < 80; i++) {
        // Dung khi gap HAI 0xCC LIEN TIEP = dem int3 giua hai ham.
        // !! KHONG duoc dung o 0xC3/0xC2/0xCC don le: ba byte do xuat hien BEN
        //    TRONG lenh khac (gia tri tuc thoi, ModRM, displacement) nen se dung
        //    som. Da do tren ca 557 lop:
        //        khong cong nao : 485 dung / 21 sai-vtable / 9 lop dang chay OK
        //        C3|C2|CC don le: 451 dung / 17 sai / *** path_track, info_landmark,
        //                         info_player_start va 43 lop khac BI HONG ***
        //        2x 0xCC        : 498 dung /  7 sai / 9 lop dang chay OK
        //    Hai 0xCC lien tiep gan nhu khong bao gio nam trong mot lenh that.
        if (fn[i] == 0xCC && fn[i+1] == 0xCC) break;
        if (fn[i] != 0xC7) continue;
        uint8_t modrm = fn[i+1];
        if (modrm > 0x07 || modrm == 0x04 || modrm == 0x05) continue;   // bo SIB/disp32
        unsigned int imm = *(unsigned int*)(fn + i + 2);
        if (imm < (unsigned int)(uintptr_t)base) continue;
        // vtable phai nam GON trong server.dll - InstallNoEdict se doc vt[9] va vt[29].
        if (!TrongSrv((void*)(uintptr_t)imm, 30 * sizeof(void*))) continue;
        if (found) return NULL;
        found = (void**)(uintptr_t)imm;
    }
    return found;
}

// Tim vtable cua mot lop qua factory cua no. Tra NULL neu khong chac chan.
// Dung chung: noedict (ben duoi) va spotlight.cpp (cong 3). Can g_SrvBase/g_SrvSize
// da gan (TrongSrv) - InstallNoEdict gan, InstallSpotlight gan neu noedict chua chay.
static void** ResolveClassVtable(uint8_t* base, const char* cls) {
    typedef void* (__cdecl *GetDict_t)(void);
    typedef void* (__thiscall *FindFactory_t)(void*, const char*);

    GetDict_t getDict = (GetDict_t)(base + 0x20CA70);
    void* dict = getDict();
    if (!dict) return NULL;

    void** dvt = *(void***)dict;
    if (!dvt) return NULL;
    FindFactory_t findFac = (FindFactory_t)dvt[3];          // slot 3 = FindFactory
    void* fac = findFac(dict, cls);
    if (!fac) return NULL;

    void** fvt = *(void***)fac;
    if (!fvt || !TrongSrv(fvt, sizeof(void*))) return NULL;
    uint8_t* create = (uint8_t*)fvt[0];                     // slot 0 = Create
    if (!create || !TrongSrv(create, 0x30 + 5)) return NULL;

    // Quet ~80 byte dau tim  C7 /r imm32  voi modrm dang [reg] khong displacement
    void** found = ScanForVtableStore(base, create);
    if (found) return found;

    // Khong thay: mot so lop co Create chi la THUNK uy quyen het cho ham con.
    // Vi du path_track (0x1012D680):
    //     mov eax,[ebp+8] ; push eax ; push 0 ; call <helper> ; add eax,0x1c ; ret 4
    // Helper do la ban dac hoa cho rieng lop nay, nen quet trong do la DUNG.
    // Chi lan theo MOT CAP, va chi khi Create ngan (<0x30 byte den ret).
    //
    // !! SUA 22/08/2026 - PHAI THU MOI LOI GOI, KHONG DUOC DUNG O CAI DAU TIEN.
    //    Ban cu tra ve NGAY tai lenh E8 dau tien, ke ca khi ket qua la NULL.
    //    Voi info_zombie_spawn (Create 0x1029C1F0) thi E8 dau tien la
    //    `call operator new` (0x100514D0) chu KHONG phai ctor:
    //        1029C1F4  push 0x510
    //        1029C1F9  call 0x100514D0   <- E8 dau tien = operator new
    //        1029C207  call 0x1029BAC0   <- ctor THAT, luu vtable o 1029BACC
    //    nen ham tra NULL va log ghi "khong tim duoc vtable, BO QUA" - lop khong
    //    bao gio duoc bat, am tham, suot nhieu phien tren may chu chinh.
    //    (func_areaportal khong dinh vi no luu vtable ngay trong Create.)
    //
    //    Do bang mo phong tren ca 557 lop, doi chieu output/binscan/ent_vt.json:
    //        truoc khi sua : 348/557 giai dung (62,5%)
    //        sau khi sua   : 485/557 giai dung (87,1%)   +137 lop
    //
    // !! DINH CHINH 23/08 - CAU "0 LOP BI HONG" O BAN GHI TRUOC LA SAI.
    //    Phep do cu chi dem "cu dung -> moi sai" (= 0). No BO SOT loai nguy hiem hon:
    //    "cu tra NULL an toan -> moi tra SAI". Dem lai dung: co DUNG 8 ca.
    //        point_commentary_viewpoint  env_soundscape_proxy  env_soundscape_triggerable
    //        prop_vehicle_driveable      player                weapon_first_aid_kit
    //        weapon_defibrillator        env_fire_trail
    //    Nguyen nhan: MSVC ghi vtable CHA truoc, CON sau. Khi duong du phong di vao mot
    //    ham chi ghi vtable cha, ta nhan nham vtable cua LOP CO SO.
    //    (Da thu "lay store CUOI thay vi dau": KHONG giai quyet - so ca SAI van la 21,
    //     chi giam NULL tu 51 xuong 44.)
    //
    //    => TUYET DOI KHONG them 8 lop tren vao noedict.txt cho toi khi co cong chan.
    //    7 lop dang chay (infodecal/light/light_spot/path_track/func_areaportal/
    //    info_zombie_spawn/func_nav_blocker) deu giai DUNG - da xac minh hai lan doc lap:
    //    mo phong, va 6 dia chi that trong edictbudget.log deu khop ent_vt.json voi
    //    mot base duy nhat 0x645D0000.
    //
    // !! VA MOT BAY LON HON, KHONG LIEN QUAN BAN SUA NAY:
    //    noedict VA THEO VTABLE, KHONG THEO CLASSNAME. Co 20 nhom classname dung chung
    //    mot vtable. Vi du nguy hiem nhat:
    //        0x105D517C = info_teleport_destination + info_player_start + info_landmark
    //                     + info_hang_lighting + info_player_logo + logic_proximity
    //    Bat info_teleport_destination se KEO THEO diem sinh survivor va moc chuyen man
    //    ma khong bao gi ca. Truoc khi them BAT KY lop nao, phai kiem vtable cua no co
    //    bi classname khac dung chung khong (tra output/binscan/ent_vt.json truong 'vt').
    //    Ho light la ngoai le LANH TINH: light/light_spot/light_directional/light_glspot
    //    dung chung 0x10612744 va ca bon deu la muc tieu hop le.
    for (int i = 0; i < 0x30; i++) {
        if (create[i] == 0xE8) {                              // call rel32
            int32_t rel = *(int32_t*)(create + i + 1);
            uint8_t* target = create + i + 5 + rel;
            if (TrongSrv(target, 80 + 6)) {                   // CHAN CA HAI DAU (02/10)
                void** viaCall = ScanForVtableStore(base, target);
                if (viaCall) return viaCall;                  // thay thi lay, khong thi THU TIEP
                i += 4;                                       // nhay qua rel32 cua call THAT
                continue;
            }
        }
        if (create[i] == 0xC3 || create[i] == 0xC2) break;     // gap ret truoc -> thoi
    }
    return NULL;
}

static bool InstallNoEdict() {
    if (g_NoEdictCount <= 0) return false;

    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* base = (uint8_t*)mi.lpBaseOfDll;

    void* pPostCtor = (void*)(base + 0x55620);

    // Cong an toan 1: prologue PostConstructor phai dung.
    // 0x55620: 55 8B EC 8B 45 08 56 8B F1 85 C0
    static const uint8_t kSig[] = { 0x55,0x8B,0xEC,0x8B,0x45,0x08,0x56,0x8B,0xF1,0x85,0xC0 };
    if (memcmp(pPostCtor, kSig, sizeof(kSig)) != 0) {
        EL_LOG("[EdictBudget] NOEDICT: prologue PostConstructor KHONG khop "
                 "- server.dll khac ban? Bo qua, khong dong gi.");
        return false;
    }
    g_OrigPostCtor = (PostCtor_t)pPostCtor;
    g_SrvBase = base;
    g_SrvSize = mi.SizeOfImage;
    EL_LOG("[EdictBudget] NOEDICT: server.dll base=%p size=0x%X (chan bien cho bo giai vtable)",
           (void*)base, (unsigned)mi.SizeOfImage);

    for (int i = 0; i < g_NoEdictCount; i++) {
        const char* cls = g_NoEdictList[i];
        void** vt = ResolveClassVtable(base, cls);
        if (!vt) {
            EL_LOG("[EdictBudget] NOEDICT: '%s' - khong tim duoc vtable, BO QUA", cls);
            continue;
        }
        // Cong an toan 2: slot 29 phai dang tro dung PostConstructor.
        // Neu da bi sua (vd lop nay dung chung vtable voi lop truoc) -> bo qua.
        if (vt[29] != pPostCtor) {
            EL_LOG("[EdictBudget] NOEDICT: '%s' vtable=%p slot29=%p != PostConstructor "
                     "- da sua roi hoac lop ghi de, BO QUA", cls, (void*)vt, vt[29]);
            continue;
        }
// Cong an toan 3 - DIEU KIEN 1: lop KHONG duoc co SendTable rieng.
// GetServerClass() = vtable slot 9, than ham `mov eax, imm32; ret`.
// PHAI so voi base + DT_BASEENTITY_RVA, KHONG so thang dia chi tinh.
// Xem docs/01-co-che.md muc noedict.
        {
            const uint8_t* gsc = (const uint8_t*)vt[9];
            uint32_t sc = 0;
            bool readable = false;
            if (gsc && gsc[0] == 0xB8 && gsc[5] == 0xC3) {
                sc = *(const uint32_t*)(gsc + 1);
                readable = true;
            }
            if (!readable) {
                // Than ham khac khuon mau => khong ket luan duoc. Khong doan, tu choi.
                EL_LOG("[EdictBudget] NOEDICT: '%s' vtable=%p GetServerClass=%p khong "
                         "dang 'mov eax,imm32; ret' - KHONG XAC DINH DUOC, BO QUA",
                         cls, (void*)vt, (void*)gsc);
                continue;
            }
            uint32_t want = (uint32_t)(uintptr_t)(base + DT_BASEENTITY_RVA);
            if (sc != want) {
                EL_LOG("[EdictBudget] NOEDICT: '%s' CO SENDTABLE RIENG "
                         "(ServerClass=0x%08X, can 0x%08X) - TRUOT DIEU KIEN 1, TU CHOI. "
                         "Go lop nay se lam client khong nhan duoc no.",
                         cls, sc, want);
                continue;
            }
        }
        void* thunk = (void*)&Hook_PostCtor;
        WriteProtected(&vt[29], &thunk, sizeof(void*));
        if (g_PatchedVtCount < NOEDICT_MAX) g_PatchedVt[g_PatchedVtCount++] = vt;
        EL_LOG("[EdictBudget] NOEDICT: '%s' vtable=%p slot29 -> thunk (OK)", cls, (void*)vt);
    }

    EL_LOG("[EdictBudget] NOEDICT: da sua %d vtable / %d lop yeu cau",
             g_PatchedVtCount, g_NoEdictCount);
    return g_PatchedVtCount > 0;
}

static void UninstallNoEdict() {
    if (!g_OrigPostCtor) return;
    void* orig = (void*)g_OrigPostCtor;
    for (int i = 0; i < g_PatchedVtCount; i++) {
        void** vt = (void**)g_PatchedVt[i];
        if (vt && vt[29] == (void*)&Hook_PostCtor)
            WriteProtected(&vt[29], &orig, sizeof(void*));
    }
    g_PatchedVtCount = 0;
    g_OrigPostCtor = NULL;
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// NOEDICT phai cai luc Load, khong phai ServerActivate: vtable cua server.dll
// co san ngay khi module nap, va thunk phai co mat TRUOC khi entity dau tien
// cua map duoc tao.
static void NoEdict_OnLoad() { if (g_NoEdict) { LoadNoEdictList(); InstallNoEdict(); } }
static void NoEdict_OnServerActivate() {
    if (g_NoEdict)
        EL_LOG("[EdictBudget] NOEDICT: %d entity da duoc dat EFL_SERVER_ONLY "
                 "(khong ton edict) tren %d vtable", g_NoEdictHits, g_PatchedVtCount);
}
static void NoEdict_OnUnload() { UninstallNoEdict(); }
