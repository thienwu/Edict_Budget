// ==========================================================================
//  baycapphat - chan doan luc ED_Alloc that bai (chi ghi log)
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// trap=1 (mac dinh): ghi 0xE9 (JMP) de len 8 byte nhanh loi cua ED_Alloc trong
// engine.dll, chay lai hai nhanh goc trong stub. Day la ban va byte vao engine.dll
// nen co cong tac rieng - trap=0 de tat.
static bool g_PatchTrap = true;

// trap: bay tai CHINH nhanh loi cua ED_Alloc, in ban kiem ke truoc khi chet.
// Chi ghi log, khong doi hanh vi. Xem docs/06-do-dac.md
static uint8_t* g_AllocFailStub = NULL;
static int g_AllocFailReports = 0;

static void __cdecl LogAllocFail(int ebxVal)
{
    if (g_AllocFailReports >= 4) return;
    g_AllocFailReports++;

    // Vong dem duoi day TRUNG voi CountFreeEdicts() (congcu_entity.cpp, da #include
    // truoc file nay) - ban chep, ket qua nhu nhau.
    int freeCount = -1;
    if (gpGlobals && gpGlobals->pEdicts && g_num_edicts) {
        int n = (int)*g_num_edicts;
        if (n > QUET_TOI_DA) n = QUET_TOI_DA;
        uint8_t* arr = (uint8_t*)gpGlobals->pEdicts;
        freeCount = 0;
        for (int i = 0; i < n; i++)
            if (*(uint32_t*)(arr + i * EDICT_SIZE) & FL_FREE) freeCount++;
    }

    EL_LOG("[EdictBudget] *** ED_ALLOC SAP BAO LOI *** ebx(slot trong cuoi cung "
             "vong quet thay)=%d | num_edicts=%d max_edicts=%d | plugin dem duoc %d slot trong",
             ebxVal,
             g_num_edicts ? (int)*g_num_edicts : -1,
             g_max_edicts ? (int)*g_max_edicts : -1,
             freeCount);

// Kiem ke tai thoi diem het edict: cai gi dang chiem 2048 slot. docs/06-do-dac.md
    if (gpGlobals && gpGlobals->pEdicts && g_num_edicts && g_AllocFailReports == 1) {
        int n = (int)*g_num_edicts;
        if (n > QUET_TOI_DA) n = QUET_TOI_DA;

        EL_LOG("[EdictBudget] === KIEM KE LUC HET EDICT: bat dau quet %d slot ===", n);

        static char name[128][40];
        static int  count[128];
        int used = 0, total = 0, unreadable = 0;

        for (int i = 0; i < n; i++) {
            const char* cn = NULL;
            __try {
                edict_t* e = gpGlobals->pEdicts + i;
                if (e->IsFree()) continue;
                total++;
                IServerNetworkable* net = e->GetNetworkable();
                if (net) cn = net->GetClassName();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                unreadable++;
                continue;
            }
            if (!cn || !*cn) { unreadable++; continue; }

            int j = 0;
            for (; j < used; j++) if (strcmp(name[j], cn) == 0) { count[j]++; break; }
            if (j == used && used < 128) {
                strncpy(name[used], cn, sizeof(name[0]) - 1);
                name[used][sizeof(name[0]) - 1] = 0;
                count[used] = 1;
                used++;
            }
        }

        EL_LOG("[EdictBudget] === %d thuc the dang song, %d lop, %d khong doc duoc ===",
                 total, used, unreadable);

        for (int a = 0; a < used && a < 30; a++) {
            int best = a;
            for (int b = a + 1; b < used; b++) if (count[b] > count[best]) best = b;
            if (best != a) {
                int c = count[a]; count[a] = count[best]; count[best] = c;
                char t[40];
                strncpy(t, name[a], sizeof(t));
                strncpy(name[a], name[best], sizeof(name[0]));
                strncpy(name[best], t, sizeof(name[0]));
            }
            EL_LOG("[EdictBudget]   %5d  %s", count[a], name[a]);
        }
    }
}

static void PatchAllocFailTrap() {
    // "test ebx,ebx ; js rel32" mot minh xuat hien 16 lan trong engine.dll.
    // Phai neo them hai lenh phia truoc - "cmp ecx,eax ; jl rel32" - moi duy
    // nhat (da xac minh: dung 1 lan, tai RVA 0x1E023F).
    const char* sig  = "\x3B\xC8\x0F\x8C\x00\x00\x00\x00\x85\xDB\x0F\x88\x00\x00\x00\x00";
    const char* mask = "xxxx....xxxx....";
    uint8_t* anchor = FindPattern("engine.dll", sig, mask);
    uint8_t* m = anchor ? anchor + 8 : NULL;   // tro toi "test ebx,ebx"
    if (!m) {
        EL_LOG("[EdictBudget] alloc-fail trap: khong tim thay chu ky!");
        return;
    }

    uint8_t* errTarget  = m + 8 + *(int32_t*)(m + 4);   // dich cua js  -> nhanh bao loi
    uint8_t* contTarget = m + 8;                        // roi qua      -> tai su dung

    uint8_t* stub = (uint8_t*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE,
                                           PAGE_EXECUTE_READWRITE);
    if (!stub) return;
    g_AllocFailStub = stub;

    int k = 0;
    stub[k++] = 0x60;                                   // pushad
    stub[k++] = 0x9C;                                   // pushfd
    stub[k++] = 0x53;                                   // push ebx
    stub[k++] = 0xE8;                                   // call LogAllocFail
    *(int32_t*)(stub + k) = (int32_t)((uint8_t*)LogAllocFail - (stub + k + 4)); k += 4;
    stub[k++] = 0x83; stub[k++] = 0xC4; stub[k++] = 0x04;   // add esp,4
    stub[k++] = 0x9D;                                   // popfd
    stub[k++] = 0x61;                                   // popad
    stub[k++] = 0x85; stub[k++] = 0xDB;                 // test ebx,ebx
    stub[k++] = 0x0F; stub[k++] = 0x88;                 // js errTarget
    *(int32_t*)(stub + k) = (int32_t)(errTarget - (stub + k + 4)); k += 4;
    stub[k++] = 0xE9;                                   // jmp contTarget
    *(int32_t*)(stub + k) = (int32_t)(contTarget - (stub + k + 4)); k += 4;

    uint8_t patch[8];
    patch[0] = 0xE9;
    *(int32_t*)(patch + 1) = (int32_t)(stub - (m + 5));
    patch[5] = patch[6] = patch[7] = 0x90;              // nop
    WriteProtected(m, patch, sizeof(patch));

    EL_LOG("[EdictBudget] alloc-fail trap: cai tai %p (stub %p, loi->%p, tiep->%p)",
             m, stub, errTarget, contTarget);
}

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
static void BayCapPhat_OnLoad() { if (g_PatchTrap) PatchAllocFailTrap(); }
