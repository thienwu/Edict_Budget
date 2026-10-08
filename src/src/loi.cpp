// ==========================================================================
//  loi - hang so, bien engine dung chung, stage
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------
#define NET_LIMIT   2048    // 11-bit protocol ceiling; slots 0..2047 only
#define QUET_TOI_DA 4096    // can tren an toan khi duyet sv.edicts (engine goc dung o 2048)
#define EDICT_SIZE  16      // sizeof(edict_t) on this 32-bit build
#define FL_FREE     2       // FL_EDICT_FREE = (1<<1), public/edict.h

// --------------------------------------------------------------------------
// Engine globals, resolved by signature at load time.
// sv layout: num_edicts(+0x214) max_edicts(+0x218) edicts(+0x21C) states(+0x220)
// --------------------------------------------------------------------------
static uint32_t* g_num_edicts = NULL;
static uint32_t* g_max_edicts = NULL;
static uint32_t* g_edicts     = NULL;
static uint32_t* g_edict_states = NULL;

static int  g_Stage           = 1;

// Resolve sv.num_edicts / max_edicts / edicts / edictchangeinfo.
static bool ResolveEngineGlobals() {
    const char* sig  = "\x8B\x0D\x00\x00\x00\x00\x6A\x01\xC1\xE1\x04\x68\x00\x00\x00\x00\x51\xE8\x00\x00\x00\x00\xA3\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\x15\x00\x00\x00\x00";
    const char* mask = "xx????xxxxxx????xx????x????x????xx????";
    uint8_t* m = FindPattern("engine.dll", sig, mask);
    if (!m) {
        EL_LOG("[EdictBudget] FATAL: engine globals signature not found.");
        return false;
    }
    g_max_edicts    = *(uint32_t**)(m + 2);
    g_edicts        = *(uint32_t**)(m + 23);
    g_edict_states  = g_edicts + 1;
    g_num_edicts    = g_max_edicts - 1;
    EL_LOG("[EdictBudget] globals: num=%p max=%p edicts=%p states=%p",
        g_num_edicts, g_max_edicts, g_edicts, g_edict_states);
    return true;
}

static void ReadStage() {
    FILE* f = OpenPluginFile("stage.txt", "r");
    if (!f) return;
    int v = -1;
    if (fscanf(f, "%d", &v) == 1 && v >= 0 && v <= 1) g_Stage = v;
    fclose(f);
}
