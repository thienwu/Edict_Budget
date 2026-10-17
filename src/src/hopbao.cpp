// ==========================================================================
//  hopbao - cho thuc the KHONG edict duoc tinh lai HOP BAO XUNG QUANH
//  patches.txt: hopbao=0/1 (mac dinh 0). Tai lieu: docs/01-co-che.md muc hopbao
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// VI SAO (do that 17/10/2026, docs/06-do-dac.md muc hopbao):
//   CCollisionProperty::MarkSurroundingBoundsDirty (0x10093A80) chi bat
//   EFL_DIRTY_SURROUNDING_COLLISION_BOUNDS (0x4000) khi thuc the CO edict va khong
//   phai world:
//       10093A86  mov eax,[ecx+0x28]   ; edict cua m_pOuter
//       10093A89  test eax,eax
//       10093A8B  je 10093AE2          ; KHONG edict -> ve, khong danh dau gi
//   => thuc the server-only (noedict) khong bao gio duoc tinh lai hop bao xung
//   quanh: WorldSpaceSurroundingBounds (0x100935F0) chi tinh khi bit 14 bat, con
//   khong thi tra hop da luu = mot diem o goc toa do.
//   func_nav_blocker dung dung hop nay de chon vung nav: BlockNav 0x1048BCC0,
//   UnblockNav 0x1048C470 => noedict co func_nav_blocker thi KHONG chan vung nao
//   (defl_3: 0/7 vung bi chan thay vi 7/7; infected ket vao cua -> cua dao chieu).
//   Ngoai hop bao, ma rieng cua CFuncNavBlocker KHONG dung edict/entindex (Ghidra).
//
// VA - mot cho, tu dinh vi, so DU 0x64 byte truoc khi dong vao:
//   dau+6:  8B 41 28 85 C0 74 55   ->   E9 <stub> 90 90
//   stub:   mov  eax,[ecx+0x28]
//           test eax,eax
//           jnz  dau+0x0D                      ; CO edict: chay y nhu ban goc
//           test dword [ecx+0x138],0x200       ; EFL_SERVER_ONLY?
//           jz   dau+0x62                      ; khong: nhu ban goc (pop esi; ret)
//           inc  dword [g_HopBaoDem]           ; dem de kiem luc chay
//           or   dword [ecx+0x138],0x4000      ; CHI danh dau hop bao can tinh lai
//           jmp  dau+0x62
//   Bo qua danh sach partition (0x8000) va PVS: UpdatePartition tu bo thuc the
//   khong edict (SDK collisionproperty.cpp:1312), PVS chi co nghia khi co edict.
//   Ca 12 loi goi MarkSurroundingBoundsDirty (doi vi tri/goc, SetCollisionBounds tu
//   SetModel, SetSolid, SetSolidFlags, SetSurroundingBoundsType, ti le model, vat ly)
//   deu di qua cho va nay.
//
// BAN DO MOC: khac noedict (doi o vtable) va spotlight (chi doi toan hang rel32 cua
// lenh call), o day thay 7 byte GIUA mot ham chung cua server.dll bang lenh nhay toi
// stub - cung kieu trap lam voi ED_Alloc trong engine.dll. Thuc the CO edict di
// dung duong goc, khong doi hanh vi.
static int           g_HopBao       = 0;     // 0 tat | 1 va
static volatile long g_HopBaoDem    = 0;     // stub da danh dau bao nhieu lan (cong don)
static long          g_HopBaoDemMoc = 0;     // moc lan bao truoc (de in theo map)
static uint8_t*      g_HopBaoDau    = NULL;  // dau MarkSurroundingBoundsDirty
static uint8_t*      g_HopBaoStub   = NULL;
static uint8_t       g_HopBaoGoc[7];         // 7 byte goc tai dau+6, de tra lai khi unload

// 0x64 byte cua MarkSurroundingBoundsDirty doc tu server.dll md5 533888fb (L4D2 2.2.4.3 / 10097).
// Hai cho la DIA CHI TUYET DOI (doi theo base khi nap) nen bo qua khi so:
//   +0x0F..+0x12  gpGlobals (0x107F7690)      +0x3A..+0x3D  danh sach partition (0x107DCA70)
// Lenh call o +0x3E la rel32 - khong doi theo base nen VAN so.
static const uint8_t kHopBaoHam[0x64] = {
    0x56,0x8B,0xF1,0x8B,0x4E,0x04,0x8B,0x41,0x28,0x85,0xC0,0x74,0x55,0x8B,0x15,0x90,
    0x76,0x7F,0x10,0x2B,0x42,0x58,0xC1,0xF8,0x04,0x85,0xC0,0x74,0x45,0x81,0x89,0x38,
    0x01,0x00,0x00,0x00,0x40,0x00,0x00,0x8B,0x46,0x04,0x8B,0x88,0x38,0x01,0x00,0x00,
    0xC1,0xE9,0x0F,0xF6,0xC1,0x01,0x75,0x18,0x50,0xB9,0x70,0xCA,0x7D,0x10,0xE8,0x0D,
    0xF6,0xFF,0xFF,0x8B,0x46,0x04,0x81,0x88,0x38,0x01,0x00,0x00,0x00,0x80,0x00,0x00,
    0x8B,0x76,0x04,0x83,0x7E,0x28,0x00,0x74,0x09,0x8B,0x76,0x28,0x81,0x0E,0x80,0x00,
    0x00,0x00,0x5E,0xC3,
};
static bool HopBaoBoQua(int i) { return (i >= 0x0F && i < 0x13) || (i >= 0x3A && i < 0x3E); }

// Neo: `or dword [ecx+0x138],0x4000` - lenh nay xuat hien DUNG MOT lan trong .text
// (chinh la dong bat co trong ham). Dau ham = neo - 0x1D.
static const uint8_t kHopBaoNeo[10] = { 0x81,0x89,0x38,0x01,0x00,0x00,0x00,0x40,0x00,0x00 };
#define HOPBAO_NEO_OFF   0x1D
#define HOPBAO_VA_OFF    0x06     // 7 byte: mov eax,[ecx+0x28] ; test eax,eax ; je
#define HOPBAO_TIEP_OFF  0x0D     // lenh ke sau je (duong co edict)
#define HOPBAO_VE_OFF    0x62     // pop esi ; ret

static void HopBaoGhiRel(uint8_t* p, uint8_t* dich) { *(int32_t*)p = (int32_t)(dich - (p + 4)); }

static bool InstallHopBao() {
    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    uint8_t* base = (uint8_t*)h;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
    uint8_t* tx = NULL; size_t txl = 0;
    for (int s = 0; s < nt->FileHeader.NumberOfSections; s++, sh++)
        if (memcmp(sh->Name, ".text", 5) == 0) { tx = base + sh->VirtualAddress; txl = sh->Misc.VirtualSize; }
    if (!tx) { EL_LOG("[EdictBudget] HOPBAO: khong thay .text cua server.dll - BO QUA"); return false; }

    // Cong 1: neo DUY NHAT.
    uint8_t* neo = NULL; int soNeo = 0;
    for (size_t i = 0; i + sizeof(kHopBaoNeo) <= txl; i++)
        if (tx[i] == 0x81 && memcmp(tx + i, kHopBaoNeo, sizeof(kHopBaoNeo)) == 0) { if (!neo) neo = tx + i; soNeo++; }
    if (soNeo != 1) {
        EL_LOG("[EdictBudget] HOPBAO: neo 'or [ecx+0x138],0x4000' xuat hien %d lan (can DUNG 1) - BO QUA", soNeo);
        return false;
    }
    uint8_t* dau = neo - HOPBAO_NEO_OFF;
    if (dau < tx) return false;

    // Cong 2: du 0x64 byte khop mau (tru 2 dia chi tuyet doi).
    for (int i = 0; i < (int)sizeof(kHopBaoHam); i++) {
        if (HopBaoBoQua(i)) continue;
        if (dau[i] != kHopBaoHam[i]) {
            EL_LOG("[EdictBudget] HOPBAO: MarkSurroundingBoundsDirty @%p lech mau o +0x%02X (%02X != %02X) "
                   "- server.dll khac ban? BO QUA", dau, i, dau[i], kHopBaoHam[i]);
            return false;
        }
    }

    // Stub.
    uint8_t* st = (uint8_t*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!st) { EL_LOG("[EdictBudget] HOPBAO: VirtualAlloc that bai - BO QUA"); return false; }
    int k = 0;
    st[k++] = 0x8B; st[k++] = 0x41; st[k++] = 0x28;                       // mov eax,[ecx+0x28]
    st[k++] = 0x85; st[k++] = 0xC0;                                       // test eax,eax
    st[k++] = 0x0F; st[k++] = 0x85; HopBaoGhiRel(st + k, dau + HOPBAO_TIEP_OFF); k += 4;   // jnz duong goc
    st[k++] = 0xF7; st[k++] = 0x81;                                       // test dword [ecx+0x138],0x200
    *(uint32_t*)(st + k) = 0x138; k += 4; *(uint32_t*)(st + k) = 0x200; k += 4;
    st[k++] = 0x0F; st[k++] = 0x84; HopBaoGhiRel(st + k, dau + HOPBAO_VE_OFF); k += 4;     // jz ve
    st[k++] = 0xFF; st[k++] = 0x05;                                       // inc dword [g_HopBaoDem]
    *(uint32_t*)(st + k) = (uint32_t)(uintptr_t)&g_HopBaoDem; k += 4;
    st[k++] = 0x81; st[k++] = 0x89;                                       // or dword [ecx+0x138],0x4000
    *(uint32_t*)(st + k) = 0x138; k += 4; *(uint32_t*)(st + k) = 0x4000; k += 4;
    st[k++] = 0xE9; HopBaoGhiRel(st + k, dau + HOPBAO_VE_OFF); k += 4;     // jmp ve
    FlushInstructionCache(GetCurrentProcess(), st, k);

    // Va 7 byte: jmp stub ; nop ; nop.
    uint8_t* p = dau + HOPBAO_VA_OFF;
    memcpy(g_HopBaoGoc, p, sizeof(g_HopBaoGoc));
    uint8_t va[7] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90 };
    *(int32_t*)(va + 1) = (int32_t)(st - (p + 5));
    WriteProtected(p, va, sizeof(va));
    FlushInstructionCache(GetCurrentProcess(), p, sizeof(va));

    g_HopBaoDau = dau; g_HopBaoStub = st;
    EL_LOG("[EdictBudget] HOPBAO: da va MarkSurroundingBoundsDirty @%p (RVA 0x%X), stub @%p (%d byte) "
           "- thuc the server-only nay duoc tinh lai hop bao xung quanh",
           dau, (unsigned)(dau - base), st, k);
    // Ghi byte that cua stub va cho va de dich nguoc kiem lai sau (byte mong doi: docs/04-dia-chi.md muc hopbao).
    char hex[3 * 64 + 1]; int o = 0;
    for (int i = 0; i < k && o < (int)sizeof(hex) - 3; i++) o += _snprintf(hex + o, 4, "%02X ", st[i]);
    hex[o] = 0;
    EL_LOG("[EdictBudget] HOPBAO: stub = %s", hex);
    o = 0;
    for (int i = 0; i < 7; i++) o += _snprintf(hex + o, 4, "%02X ", p[i]);
    hex[o] = 0;
    EL_LOG("[EdictBudget] HOPBAO: dau+6 = %s(goc 8B 41 28 85 C0 74 55)", hex);
    return true;
}

static void UninstallHopBao() {
    if (!g_HopBaoDau) return;
    uint8_t* p = g_HopBaoDau + HOPBAO_VA_OFF;
    // Chi tra lai neo cho do van la jmp cua ta (khong de len va cua ai khac).
    if (p[0] == 0xE9 && p + 5 + *(int32_t*)(p + 1) == g_HopBaoStub) {
        WriteProtected(p, g_HopBaoGoc, sizeof(g_HopBaoGoc));
        FlushInstructionCache(GetCurrentProcess(), p, sizeof(g_HopBaoGoc));
    }
    // KHONG giai phong stub: luong nao dang o giua stub van can no de nhay ve.
    g_HopBaoDau = NULL;
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// Goi SAU NoEdict_OnLoad (de doc duoc danh sach noedict.txt) va TRUOC map dau tien.
static void HopBao_OnLoad() {
    bool coNavBlocker = false;
    for (int i = 0; i < g_NoEdictCount; i++)
        if (!_stricmp(g_NoEdictList[i], "func_nav_blocker")) coNavBlocker = true;
    if (g_HopBao > 0) InstallHopBao();
    if (coNavBlocker && g_NoEdict && !g_HopBaoDau)
        EL_LOG("[EdictBudget] !! HOPBAO: noedict.txt co func_nav_blocker ma hopbao KHONG chay "
               "=> nav blocker se KHONG chan vung nav nao (docs/03-loi-da-biet.md muc 11)");
}
static void HopBao_OnServerActivate() {
    if (!g_HopBaoDau) return;
    long n = g_HopBaoDem;
    EL_LOG("[EdictBudget] HOPBAO: %ld lan danh dau hop bao cho thuc the server-only tu lan bao truoc (tong %ld)",
           n - g_HopBaoDemMoc, n);
    g_HopBaoDemMoc = n;
}
static void HopBao_OnUnload() { UninstallHopBao(); }
