// ==========================================================================
//  congcu_entity - ham server.dll dung chung de duyet/go entity
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// Gan o InstallWipeClear() hoac InstallMapClear(), cai nao chay truoc.
// Dung chung boi wipeclear, mapclear, heartbeat.
// Chi CON TRO dung chung; DIA CHI van ghi hai lan (wipeclear.cpp va mapclear.cpp).
// Ca hai deu tat => heartbeat khong co g_NextEnt va tu bo qua.
typedef void  (__fastcall *fnCleanupDeleteList_t)(void*, void*);
typedef void* (__fastcall *fnNextEnt_t)(void*, void*, void*);
typedef void  (__cdecl    *fnUtilRemove_t)(void*);

static fnCleanupDeleteList_t g_CleanupDeleteList = NULL;
static fnNextEnt_t           g_NextEnt           = NULL;
static fnUtilRemove_t        g_UtilRemove        = NULL;
static void*                 g_EntList           = NULL;
static bool*                 g_InCleanupDelete   = NULL;
static const char**          g_PreserveList      = NULL;

static int CountFreeEdicts() {
    if (!gpGlobals || !gpGlobals->pEdicts || !g_num_edicts) return -1;
    int n = (int)*g_num_edicts;
    if (n > QUET_TOI_DA) n = QUET_TOI_DA;
    uint8_t* arr = (uint8_t*)gpGlobals->pEdicts;
    int f = 0;
    for (int i = 0; i < n; i++)
        if (*(uint32_t*)(arr + i * EDICT_SIZE) & FL_FREE) f++;
    return f;
}

// Preserve list that cua L4D2: 38 chuoi + mot chuoi RONG lam END marker.
// Khong ro classname => GIU (an toan hon la xoa nham).
static bool InPreserveList(const char* cls) {
    if (!g_PreserveList || !cls || !*cls) return true;
    for (int i = 0; i < 64; i++) {
        const char* s = g_PreserveList[i];
        if (!s || !*s) break;
        if (strcmp(s, cls) == 0) return true;
    }
    return false;
}
