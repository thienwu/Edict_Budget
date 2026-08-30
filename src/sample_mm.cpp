// ===========================================================================
//  edictbudget - giu so entity DANG SONG duoi tran 2048 edict cho L4D2.
//
//  Y tuong, bai toan va dinh huong: thienwu - nguoi VAN HANH may chu that.
//  Ho quyet dinh moi huong di lon, va ca nhung huong KHONG duoc di (cam huong
//  4096, cam dong vao ho phys, cam sua BSP, yeu cau CONG THUC CHUNG tu kiem
//  luc chay). Ho chay thu, chup log, do tren may chu that, va bac bo nhieu ket
//  luan sai cua AI - cac dong ghi "SAI, da sua" la dau vet cua nhung lan do.
//
//  Dich nguoc, viet ma, do dac va tai lieu: Claude (Anthropic), trong Claude
//  Code. Chi tiet day du o NOTICE.
//
//  Giay phep: GPLv3. Xem LICENSE.
//
//  ------------------------------------------------------------------------
//  TAI LIEU DAY DU nam trong docs/ - khong lap lai o day:
//
//    docs/00-tong-quan.md    nhiem vu, gioi han 11 bit, so lieu 3 chien dich,
//                            file cau hinh, huong dan build
//    docs/01-co-che.md       noedict, freegate, wipeclear, swap
//    docs/02-mapclear.md     mapclear + vi sao KHONG duoc xoa cai mang sang
//    docs/03-huong-4096.md   toan bo nhom cong tac 4096 - DA TAT, dung bat
//    docs/04-nonetkill.md    doi ten classname trong lump - DA LOAI BO
//    docs/04-cef.md          CEF - da go khoi ke hoach
//    docs/05-do-dac.md       log, kiem ke, bay, heartbeat, loadprobe
//    docs/06-dia-chi.md      !! BANG TRA DIA CHI DICH NGUOC DA XAC MINH cho
//                            TUNG tinh nang: RVA, so hieu vtable slot, chuoi
//                            neo, phien ban game + md5 nhi phan tham chieu, va
//                            CACH SUY LAI TAT CA sau khi Valve cap nhat game
//    docs/07-het-huong.md    !! DA DAT GIOI HAN (23/08/2026) - moi huong da
//                            tim, da do, da bac bo; va NHUNG GI CON CHUA CHAC
//                            ve ba lop noedict them ngay 21-22/08
//
//  Moi ket luan trong docs/ deu kem dia chi ham va doan lenh de kiem lai duoc.
//  Cai gi khong xac minh duoc thi ghi thang la KHONG XAC DINH.
// ===========================================================================

#include "sample_mm.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdint.h>
#include <windows.h>
#include <psapi.h>
#include <igameevents.h>

// windows.h dinh nghia GetClassName -> GetClassNameA (ham cua Win32 GUI), lam
// hong loi goi IServerNetworkable::GetClassName(). Go macro di.
#ifdef GetClassName
#undef GetClassName
#endif

SamplePlugin g_SamplePlugin;
ISmmPlugin *OOSM_api = &g_SamplePlugin;
IServerGameDLL *server = NULL;
IServerGameClients *gameclients = NULL;
IVEngineServer *engine = NULL;
IGameEventManager2 *gameevents = NULL;
CGlobalVars *gpGlobals = NULL;

SH_DECL_HOOK6(IServerGameDLL, LevelInit, SH_NOATTRIB, 0, bool, char const *, char const *, char const *, char const *, bool, bool);
SH_DECL_HOOK3_void(IServerGameDLL, ServerActivate, SH_NOATTRIB, 0, edict_t *, int, int);
SH_DECL_HOOK1(IVEngineServer, CreateEdict, SH_NOATTRIB, 0, edict_t *, int);
SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);

PLUGIN_EXPOSE(SamplePlugin, g_SamplePlugin);

// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------
#define NET_LIMIT   2048    // 11-bit protocol ceiling; slots 0..2047 only
#define EXT_LIMIT   4096    // total array size we make the engine allocate
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

// g_BigArrayOn = mang da noi rong 4096 chua | g_EngineArray4096 = co AN TOAN
// de dat thu gi tren 2047 khong. HAI cau hoi khac nhau - xem docs/03-huong-4096.md
static bool g_BigArrayOn      = false;
static bool g_EngineArray4096 = false;

// Chot mot lan cho bo canh moi frame (xem Hook_GameFrame).
static bool g_WarnedNum = false;
static bool g_WarnedMax = false;
static bool g_WarnedGlob = false;
static bool g_ExtReady        = false;  // dai mo rong da dung duoc chua?
static int  g_Cursor          = EXT_LIMIT - 1;  // cap phat DI XUONG, xem Hook_CreateEdict
static int  g_Stage           = 1;

uint8_t* FindPattern(const char* module, const char* pattern, const char* mask);

// Dinh nghia o duoi, canh bo canh danh sach SourceMod ma chung thuoc ve.
static uint32_t* g_SMListHead;
static void ResolveSMListHead();
edict_t* Hook_CreateEdict_Post(int forceIndex);   // dinh nghia o cuoi file

// ==========================================================================
// Ham phu nho
// ==========================================================================
static void WriteProtected(void* dst, const void* src, size_t len) {
    DWORD old;
    VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &old);
    memcpy(dst, src, len);
    VirtualProtect(dst, len, old, &old);
}

// Thu ca hai thu muc lam viec: srcds.exe chay tu THU MUC GOC cua may chu, con
// listen server hoac trinh khoi chay khac co the dang o trong left4dead2\.
static FILE* OpenPluginFile(const char* name, const char* mode) {
    char path[MAX_PATH];
    _snprintf(path, sizeof(path), "left4dead2\\addons\\edictbudget\\%s", name);
    path[sizeof(path)-1] = 0;
    FILE* f = fopen(path, mode);
    if (f) return f;
    _snprintf(path, sizeof(path), "addons\\edictbudget\\%s", name);
    path[sizeof(path)-1] = 0;
    return fopen(path, mode);
}

// Ghi log ra file rieng addons/edictbudget/edictbudget.log. Xem docs/05-do-dac.md
static FILE* g_LogFile   = NULL;
static bool  g_LogOpened = false;
// logconsole: 1 = in CA ra console server. Mac dinh 0 - chi ghi file.
static bool  g_LogConsole = false;

static void EL_LOG(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;

    if (!g_LogOpened) {
        g_LogOpened = true;
        g_LogFile = OpenPluginFile("edictbudget.log", "a");
        if (g_LogFile) {
            time_t t = time(NULL);
            struct tm* lt = localtime(&t);
            fprintf(g_LogFile,
                    "\n===== phien moi: %04d-%02d-%02d %02d:%02d:%02d =====\n",
                    lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                    lt->tm_hour, lt->tm_min, lt->tm_sec);
        }
    }
    if (g_LogFile) {
        time_t t = time(NULL);
        struct tm* lt = localtime(&t);
        fprintf(g_LogFile, "%02d:%02d:%02d  %s\n",
                lt->tm_hour, lt->tm_min, lt->tm_sec, buf);
        fflush(g_LogFile);
    }
    if (g_LogConsole && g_PLAPI) META_LOG(g_PLAPI, "%s", buf);
}

static void EL_LOG_CLOSE() {
    if (g_LogFile) { fclose(g_LogFile); g_LogFile = NULL; }
    g_LogOpened = false;
}

// ==========================================================================
// Cong tac cho tung ban va, doc tu patches.txt luc nap
// ==========================================================================
//
// Nam ban va byte doc lap di vao engine, va loi do BAT KY cai nao trong so do
// gay ra deu trong GIONG HET NHAU tu ben ngoai. Build lai de chia doi tim thu
// pham thi moi lan mat tron mot vong tat/chep/khoi dong lai, nen thay vao do
// moi ban va co mot cong tac rieng: ghi "ten=0" vao patches.txt roi khoi dong lai.
//
// Thieu file hoac thieu khoa deu coi la BAT, nen truong hop binh thuong khong
// can file nao ca.

static bool g_PatchFreetime    = false;   // nhom 4096 - CAM, mac dinh TAT
static bool g_PatchIndexBounds = false;   // nhom 4096 - CAM, mac dinh TAT
static bool g_PatchForcedIndex = false;   // nhom 4096 - CAM, mac dinh TAT
static bool g_PatchBigArray    = false;   // nhom 4096 - CAM, mac dinh TAT
static bool g_PatchSnapshot    = false;   // nhom 4096 - CAM, mac dinh TAT

// JMP noi tuyen 7 byte de nhay qua CreateEntityByName cua server.dll, cong mot
// trampoline tu viet. No CHI ton tai de biet classname phuc vu danh sach cho
// phep va ban kiem ke - khi tat phan phan tach thi no la chi phi thuan tuy.
// No KHONG co cong tac, nen da am tham hoat dong trong MOI lan chia doi tim loi
// va chua bao gio bi loai tru mot lan nao. Plugin goc 233 dong khong he detour
// kieu nay ma khoi dong rat vung - chinh dieu do dua no vao dien tinh nghi.
static bool g_PatchDetour      = false;   // nhom 4096 - CAM, mac dinh TAT

// Bo thoi gian cho 1 giay truoc khi mot edict vua giai phong duoc cap lai.
// Day la co che chinh chua duoc bai toan. Xem docs/01-co-che.md muc freegate.
static bool g_ImmediateReuse   = false;   // mac dinh TAT (patches.txt: reuse=0)

// Cho phep tai su dung edict ngay, va thang vao ED_Alloc.
//
// AllowImmediateEdictReuse() phai duoc goi TU BEN NGOAI, nen no chi cham toi
// nhung lan cap phat di qua IVEngineServer::CreateEdict. Da chung minh (mucA
// 0-AAC) rang lan cap phat that bai KHONG di qua duong do - hook chua bao gio
// duoc goi cho no. Va byte tai chinh diem quyet dinh thi khong co lo hong day.
static int  g_PatchFreeGate    = 0;      // 0 TAT (mac dinh) | 1 co danh sach | 2 vo dieu kien
                                         // MAC DINH TAT: co che it duoc nghiem thu nhat, va
                                         // che do 2 da xac minh la lam hong viec chuyen vat pham.
static int  g_MinFreeSeen      = 999999;
static int  g_EdgeLines        = 0;

// Dem so lan cap phat edict trong MOT frame. Xem docs/05-do-dac.md
static int  g_AllocThisFrame   = 0;
static int  g_MaxBurst         = 0;

// Thay doi goi la "mang lon" thuc ra la BA viec rieng biet tinh co di chung
// voi nhau. Khi hoa ra chinh nhom nay lam hong vong hoi sinh entity luc wipe,
// chung buoc phai tach ra de thu duoc rieng tung cai:
//   bigarray  - SV_AllocateEdicts cap 4096 edict thay vi 2048
//   pinlimits - LevelInit dat max_edicts / maxEntities ve lai 2048
//   markfree  - LevelInit dong co FL_EDICT_FREE len cac o 2048-4095
static bool g_PinMax     = false;  // nhom 4096 - CAM, mac dinh TAT
static bool g_PinGlobals = false;  // nhom 4096 - CAM, mac dinh TAT
static bool g_MarkFree   = false;  // nhom 4096 - CAM, mac dinh TAT

// wipeclear: don thuc the o dau CTerrorGameRules::RestartRound (vtable slot 178),
// TRUOC vong hoi sinh nguoi choi. Xem docs/01-co-che.md muc wipeclear.
static int g_WipeClear = 0;

// Bay ED_Alloc: ghi 0xE9 (JMP) de len 8 byte nhanh loi trong engine.dll roi dung
// lai hai nhanh goc trong stub. Truoc 07/08 no KHONG co cong tac - tu chay moi
// khi stage!=0. Nay tach ra vi:
//   - muc 8c ghi "khong crash khi stage=0", ma nay stage=1 chi con bay + wipeclear
//     => bay la nghi can duy nhat con lai cho con crash sourcemod+0x13b63
//   - va vi no LA mot ban va byte vao engine.dll, dung de no chay ngam khong cong tac
// Khi wipeclear da chay dung thi bay chi con de chan doan - tat duoc.
static bool g_PatchTrap = true;

// Hai cong tac xu ly thuc the khong dung mang: nonetkill (DA LOAI BO) va noedict.
// Khac biet sinh tu giua chung o docs/01-co-che.md va docs/04-nonetkill.md
static bool g_NoNetKill = false;
static bool g_NoNetHigh = false;

// noedict: bat EFL_SERVER_ONLY cho cac lop trong noedict.txt => chung KHONG
// duoc cap edict, nam o dai 2049-4095 (thiet ke GOC cua engine).
// XXX KHONG lien quan 4096. Xem khoi giai thich day du gan InstallNoEdict().
static bool g_NoEdict   = false;

// mapclear: don entity luc CHUYEN MAN, giu lai do nguoi choi mang sang duoc.
// 0 tat | 1 chi quan sat | 2 don that. Xem khoi giai thich gan InstallMapClear().
static int  g_MapClear = 0;
// Tran so entity mapclear duoc xoa moi lan chuyen man. 0 = khong gioi han.
// Dung de TIM NGUONG: hai lan xoa >1300 deu chet cam.
static int  g_MapClearMax = 0;
// 1 = CHI xoa entity co FCAP_ACROSS_TRANSITION. 0 = xoa tat ca (mac dinh).
// !! DE MAC DINH 0. Che do 1 da duoc thu 14/08 va GIET SERVER trong 12 giay.
// Xem docs/02-mapclear.md - co bang ba lan chet va co che POST-hook.
static int  g_MapClearCarryOnly = 0;
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
// loadprobe: ghi so edict trong N frame dau sau khi nap map, de bat dinh tam
// thoi. MOC CO SO ghi tai ServerActivate, luc do nhieu entity CHUA spawn xong.
// Xem docs/05-do-dac.md
static int  g_Swap = 0;
// Tran so lan doi. 0 = khong gioi han. Dung de thu vai chuc cai truoc khi doi het.
static int  g_SwapMax = 0;
static int  g_LoadProbe = 8;
static int  g_LoadProbeLeft = 0;
static int  g_LoadProbeFrame = 0;
static int  g_LoadProbePeak = 0;
// heartbeat: so GIAY giua hai lan ghi so lieu thuc the vao log. 0 = tat.
// Chi ghi log, khong dong vao entity nao. Xem HeartbeatSample().
static int  g_Heartbeat = 0;

// nonetkill: doi ten classname TAI CHO trong entity lump o LevelInit.
// DA LOAI BO - no giet luon Spawn()/Activate(), lam mat decal va sai anh sang.
// Giu lai vi co the huu ich cho lop khac. Xem docs/04-nonetkill.md
#define KILL_MAX 64
static char g_KillList[KILL_MAX][40];
static int  g_KillCount = 0;
static char* g_LumpCopy = NULL;

static void LoadKillList() {
    g_KillCount = 0;
    // RONG co y. Xem khoi cam o tren: mac dinh cu {infodecal,light,light_spot}
    // lam sai anh sang ch04_pripyat03. Khong co nonetkill.txt = khong cat gi.
    static const char* kDefault[] = { NULL };

    FILE* f = OpenPluginFile("nonetkill.txt", "r");
    bool coFile = (f != NULL);
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && g_KillCount < KILL_MAX) {
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
            strncpy(g_KillList[g_KillCount], p, sizeof(g_KillList[0])-1);
            g_KillList[g_KillCount][sizeof(g_KillList[0])-1] = 0;
            g_KillCount++;
        }
        fclose(f);
    } else {
        for (int i = 0; kDefault[i] && g_KillCount < KILL_MAX; i++) {
            strncpy(g_KillList[g_KillCount], kDefault[i], sizeof(g_KillList[0])-1);
            g_KillList[g_KillCount][sizeof(g_KillList[0])-1] = 0;
            g_KillCount++;
        }
    }
    // Truoc day cho ngay OpenPluginFile vao doi so cua EL_LOG => mo file lan hai
    // ma khong bao gio fclose. Dung co san coFile o tren.
    EL_LOG("[EdictBudget] NONETKILL: %d lop trong danh sach%s",
             g_KillCount, coFile ? "" : " (mac dinh)");
    for (int i = 0; i < g_KillCount; i++)
        EL_LOG("[EdictBudget] NONETKILL:   [%d] '%s'", i, g_KillList[i]);
}

// Tra ve chuoi lump DA SUA (cap phat moi), hoac NULL neu khong sua gi.
// Chi ghi de gia tri classname, giu nguyen do dai va moi ky tu khac.
static const char* RewriteLump(const char* lump, int* outHits) {
    *outHits = 0;
    if (!lump || g_KillCount <= 0) return NULL;

    size_t len = strlen(lump);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, lump, len + 1);

    // Tim moi cap:  "classname" "<gia tri>"
    const char* KEY = "\"classname\"";
    size_t klen = strlen(KEY);
    for (size_t i = 0; i + klen < len; i++) {
        if (memcmp(buf + i, KEY, klen) != 0) continue;
        size_t j = i + klen;
        while (j < len && (buf[j]==' '||buf[j]=='\t')) j++;
        if (j >= len || buf[j] != '"') continue;
        size_t vs = ++j;                       // dau gia tri
        while (j < len && buf[j] != '"') j++;
        if (j >= len) break;
        size_t vlen = j - vs;
        if (vlen == 0 || vlen >= 40) continue;

        char cls[40];
        memcpy(cls, buf + vs, vlen); cls[vlen] = 0;
        for (int k = 0; k < g_KillCount; k++) {
            if (strcmp(cls, g_KillList[k]) == 0) {
                buf[vs] = '~';                 // DOI DUNG MOT KY TU, giu do dai
                (*outHits)++;
                break;
            }
        }
    }

    if (*outHits == 0) { free(buf); return NULL; }
    return buf;
}

// CEF - DA GO KHOI KE HOACH. Xem docs/04-cef.md truoc khi ai do them lai.

static void LoadPatchSwitches() {
    FILE* f = OpenPluginFile("patches.txt", "r");
    // Khong tim thay = MOI cong tac ve mac dinh, im lang. Bay thuong gap nhat:
    // chep ban moi vao ma van giu ten thu muc cu. Thu muc PHAI la addons\edictbudget\.
    if (!f) {
        EL_LOG("[EdictBudget] !! KHONG DOC DUOC patches.txt - dung TOAN BO mac dinh !!");
        EL_LOG("[EdictBudget] !! thu muc phai la addons\\edictbudget\\ (khong phai ten cu) !!");
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || !*p) continue;

        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;

        char* end = eq - 1;                       // trim the key
        while (end >= p && (*end == ' ' || *end == '\t')) *end-- = 0;
        bool on = (atoi(eq + 1) != 0);

        if      (!_stricmp(p, "freetime"))    g_PatchFreetime    = on;
        else if (!_stricmp(p, "indexbounds")) g_PatchIndexBounds = on;
        else if (!_stricmp(p, "forcedindex")) g_PatchForcedIndex = on;
        else if (!_stricmp(p, "bigarray"))    g_PatchBigArray    = on;
        else if (!_stricmp(p, "snapshot"))    g_PatchSnapshot    = on;
        else if (!_stricmp(p, "detour"))      g_PatchDetour      = on;
        else if (!_stricmp(p, "reuse"))       g_ImmediateReuse   = on;
        else if (!_stricmp(p, "freegate"))    g_PatchFreeGate    = atoi(eq + 1);  // 0/1/2
        else if (!_stricmp(p, "pinmax"))      g_PinMax           = on;
        else if (!_stricmp(p, "pinglobals"))  g_PinGlobals       = on;
        else if (!_stricmp(p, "markfree"))    g_MarkFree         = on;
        else if (!_stricmp(p, "wipeclear"))   g_WipeClear        = atoi(eq + 1);  // 0/1/2
        else if (!_stricmp(p, "trap"))        g_PatchTrap        = on;
        else if (!_stricmp(p, "nonetkill"))   g_NoNetKill        = on;
        else if (!_stricmp(p, "nonethigh"))   g_NoNetHigh        = on;
        else if (!_stricmp(p, "noedict"))     g_NoEdict          = on;
        else if (!_stricmp(p, "mapclear"))    g_MapClear         = atoi(eq + 1);  // 0/1/2
        else if (!_stricmp(p, "mapclearmax")) g_MapClearMax      = atoi(eq + 1);  // 0 = khong gioi han
        else if (!_stricmp(p, "mapclearcarry")) g_MapClearCarryOnly = on;         // 1 = chi xoa cai mang sang
        else if (!_stricmp(p, "loadprobe"))   g_LoadProbe        = atoi(eq + 1);  // so frame lay mau sau khi nap
        else if (!_stricmp(p, "swap"))        g_Swap             = atoi(eq + 1);  // 0/1/2, xem InstallSwap()
        else if (!_stricmp(p, "swapmax"))     g_SwapMax          = atoi(eq + 1);  // 0 = khong gioi han
        else if (!_stricmp(p, "heartbeat"))   g_Heartbeat        = atoi(eq + 1);  // giay, 0 = tat
        else if (!_stricmp(p, "logconsole"))  g_LogConsole       = on;
    }
    fclose(f);

    EL_LOG("[EdictBudget] patches: freetime=%d indexbounds=%d forcedindex=%d "
             "bigarray=%d snapshot=%d pinmax=%d pinglobals=%d markfree=%d detour=%d reuse=%d freegate=%d wipeclear=%d trap=%d nonetkill=%d nonethigh=%d",
             g_PatchFreetime, g_PatchIndexBounds, g_PatchForcedIndex,
             g_PatchBigArray, g_PatchSnapshot, g_PinMax, g_PinGlobals, g_MarkFree,
             g_PatchDetour, g_ImmediateReuse, g_PatchFreeGate, g_WipeClear, g_PatchTrap,
             g_NoNetKill, g_NoNetHigh);

    if (g_NoNetHigh && !(g_PatchBigArray && g_PatchSnapshot))
        EL_LOG("[EdictBudget] !!  nonethigh=1 nhung bigarray=%d snapshot=%d "
                 "- can CA HAI =1, neu khong no KHONG lam gi ca",
                 g_PatchBigArray, g_PatchSnapshot);
    if (g_NoNetKill && g_NoNetHigh)
        EL_LOG("[EdictBudget] !!  bat CA HAI nonetkill va nonethigh - chung mau "
                 "thuan. nonetkill xoa entity truoc khi no kip duoc cap edict.");
}

// ==========================================================================
// Danh sach cho phep: classname nao duoc nam o tren 2047
// ==========================================================================
#define MAX_PREFIX 64
static char g_Prefix[MAX_PREFIX][32];
static int  g_PrefixCount = 0;

static void AddPrefix(const char* s) {
    if (g_PrefixCount >= MAX_PREFIX || !s || !*s) return;
    strncpy(g_Prefix[g_PrefixCount], s, sizeof(g_Prefix[0]) - 1);
    g_Prefix[g_PrefixCount][sizeof(g_Prefix[0]) - 1] = 0;
    g_PrefixCount++;
}

static void LoadAllowList() {
    g_PrefixCount = 0;

    FILE* f = OpenPluginFile("serveronly.txt", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            char* p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
            char* e = p;
            while (*e && *e != '\r' && *e != '\n' && *e != ' ' && *e != '\t' && *e != '#') e++;
            *e = 0;
            AddPrefix(p);
        }
        fclose(f);
        EL_LOG("[EdictBudget] allow-list: %d prefixes from serveronly.txt", g_PrefixCount);
        if (g_PrefixCount) return;
    }

    // Mac dinh gan san, chon THAN TRONG. Cac ho nay hoat dong hoan toan qua I/O
    // theo TEN; khong co EHANDLE cua lop co mang nao tro toi chung, nen chi so
    // tren 2047 khong the lam hong mot handle. Muon mo rong thi ghi vao
    // serveronly.txt sau khi da xac minh.
    AddPrefix("logic_");
    AddPrefix("math_");
    AddPrefix("ai_");
    EL_LOG("[EdictBudget] allow-list: built-in default (%d prefixes)", g_PrefixCount);
}

// Muc ket thuc bang '_' la TIEN TO CUA CA HO ("logic_" bao ca logic_relay va
// moi thu khac trong ho do). Moi thu khac phai khop classname CHINH XAC.
//
// Phan biet nay quan trong: neu coi "light" la tien to thi no se nuot luon ca
// light_dynamic - lop nay khac voi den nuong san, no CO MANG, va phai nam duoi 2048.
static bool MayLiveHigh(const char* cls) {
    if (!cls) return false;
    for (int i = 0; i < g_PrefixCount; i++) {
        size_t len = strlen(g_Prefix[i]);
        if (len == 0) continue;
        if (g_Prefix[i][len - 1] == '_') {
            if (strncmp(cls, g_Prefix[i], len) == 0) return true;
        } else {
            if (strcmp(cls, g_Prefix[i]) == 0) return true;
        }
    }
    return false;
}

// ==========================================================================
// Audit: record exactly what we put above 2047, dump once per level
// ==========================================================================
#define AUDIT_MAX 64
static char g_AuditName[AUDIT_MAX][40];
static int  g_AuditCount[AUDIT_MAX];
static int  g_AuditUsed = 0, g_AuditOverflow = 0, g_AuditTotal = 0;

static void AuditReset() { g_AuditUsed = g_AuditOverflow = g_AuditTotal = 0; }

static void AuditAdd(const char* name) {
    if (!name) name = "(null)";
    g_AuditTotal++;
    for (int i = 0; i < g_AuditUsed; i++) {
        if (strcmp(g_AuditName[i], name) == 0) { g_AuditCount[i]++; return; }
    }
    if (g_AuditUsed >= AUDIT_MAX) { g_AuditOverflow++; return; }
    strncpy(g_AuditName[g_AuditUsed], name, sizeof(g_AuditName[0]) - 1);
    g_AuditName[g_AuditUsed][sizeof(g_AuditName[0]) - 1] = 0;
    g_AuditCount[g_AuditUsed] = 1;
    g_AuditUsed++;
}

static void AuditDump() {
    EL_LOG("[EdictBudget] AUDIT: %d entities in %d-%d across %d classes%s",
        g_AuditTotal, NET_LIMIT, EXT_LIMIT - 1, g_AuditUsed,
        g_AuditOverflow ? " (list truncated)" : "");
    for (int i = 0; i < g_AuditUsed; i++) {
        EL_LOG("[EdictBudget]    x%-4d %s", g_AuditCount[i], g_AuditName[i]);
    }
}

// Kiem ke MOI lop map tao ra. Chon danh sach cho phep bang truc giac khong an
// thua - phai biet map thuc su sinh gi. Xem docs/05-do-dac.md
#define CENSUS_MAX 192
#define CENSUS_TRIP 1900          // do ra truoc khi cham buc tuong 2047
static bool g_Tripped = false;
static char g_CensusName[CENSUS_MAX][40];
static int  g_CensusCount[CENSUS_MAX];
static int  g_CensusUsed = 0, g_CensusTotal = 0, g_CensusDropped = 0;

static void CensusReset() { g_CensusUsed = g_CensusTotal = g_CensusDropped = 0; }

static void CensusAdd(const char* name) {
    if (!name || !*name) return;
    g_CensusTotal++;
    for (int i = 0; i < g_CensusUsed; i++) {
        if (strcmp(g_CensusName[i], name) == 0) { g_CensusCount[i]++; return; }
    }
    if (g_CensusUsed >= CENSUS_MAX) { g_CensusDropped++; return; }
    strncpy(g_CensusName[g_CensusUsed], name, sizeof(g_CensusName[0]) - 1);
    g_CensusName[g_CensusUsed][sizeof(g_CensusName[0]) - 1] = 0;
    g_CensusCount[g_CensusUsed] = 1;
    g_CensusUsed++;
}

static void CensusDump() {
    EL_LOG("[EdictBudget] CENSUS: %d entities created, %d distinct classes%s (sorted by count)",
        g_CensusTotal, g_CensusUsed, g_CensusDropped ? " (some dropped)" : "");

    // Sap xep chon don gian tren chi so - bang nho va ham nay chi chay mot lan
    // moi map.
    for (int a = 0; a < g_CensusUsed && a < 60; a++) {
        int best = a;
        for (int b = a + 1; b < g_CensusUsed; b++) {
            if (g_CensusCount[b] > g_CensusCount[best]) best = b;
        }
        if (best != a) {
            int c = g_CensusCount[a]; g_CensusCount[a] = g_CensusCount[best]; g_CensusCount[best] = c;
            char t[40];
            strncpy(t, g_CensusName[a], sizeof(t));
            strncpy(g_CensusName[a], g_CensusName[best], sizeof(g_CensusName[0]));
            strncpy(g_CensusName[best], t, sizeof(g_CensusName[0]));
        }
        EL_LOG("[EdictBudget]   %5d  %s%s", g_CensusCount[a], g_CensusName[a],
                 MayLiveHigh(g_CensusName[a]) ? "   <-- relocated" : "");
    }
}

// ==========================================================================
// engine.dll patches
// ==========================================================================

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

// Mang freetime la bang 2048 so thuc co dinh, danh chi so theo chi so edict,
// duoc ED_Alloc dung de quyet dinh khi nao mot o vua giai phong duoc tai dung.
// Tro no sang mot vung dem 4096 muc, va noi rong lenh memset xoa no (kich thuoc
// dang bi ghi cung la 0x2000).
static void PatchFreetime() {
    HMODULE h = GetModuleHandle("engine.dll");
    if (!h) return;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* base = (uint8_t*)mi.lpBaseOfDll;

    uint32_t oldPtr = (uint32_t)base + 0x6b3a58;
    float* fresh = (float*)_aligned_malloc(EXT_LIMIT * sizeof(float), 16);
    memset(fresh, 0, EXT_LIMIT * sizeof(float));

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    uint8_t* text = NULL; size_t textLen = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (strncmp((char*)sec[i].Name, ".text", 5) == 0) {
            text = base + sec[i].VirtualAddress;
            textLen = sec[i].Misc.VirtualSize;
            break;
        }
    }
    if (!text) return;

    // Noi rong "push 0x2000" trong ham xoa bang, phai lam TRUOC khi con tro
    // ben duoi no bi ghi de (chu ky dung lai truoc con tro do).
    const char* sizeSig  = "\x68\x00\x20\x00\x00\x6A\x00\x68";
    const char* sizeMask = "xxxxxxxx";
    uint8_t* sz = FindPattern("engine.dll", sizeSig, sizeMask);
    if (sz) {
        uint32_t v = EXT_LIMIT * sizeof(float);
        WriteProtected(sz + 1, &v, sizeof(v));
        EL_LOG("[EdictBudget] freetime clear size 0x2000 -> 0x%X", v);
    }

    int n = 0;
    for (size_t i = 0; i + 4 <= textLen; i++) {
        uint32_t* p = (uint32_t*)(text + i);
        if (*p == oldPtr) {
            uint32_t v = (uint32_t)fresh;
            WriteProtected(p, &v, sizeof(v));
            n++; i += 3;
        }
    }
    EL_LOG("[EdictBudget] freetime: repointed %d references to a %d-entry table", n, EXT_LIMIT);
}

// Doi hai bang theo-entity cua CFrameSnapshotManager ra khoi doi tuong engine.
// Day la ban va lam cho chi so tren 2047 song duoc. BAY lan truy cap, phai sua
// DU CA BAY - thieu mot la engine doc mot bang trong khi ghi bang kia.
// Bang do doi va ly do o docs/03-huong-4096.md

static uint32_t* g_PackedTable = NULL;   // 4096 muc
static uint32_t* g_SerialTable = NULL;   // 4096 muc

struct SnapAccess { const char* sig; const char* head; bool serial; };

static bool PatchSnapshotTables() {
    static const SnapAccess kSites[] = {
        { "\x8B\xBC\xB1\x9C\x00\x00\x00", "\x8B\x3C\xB5", false },
        { "\x8B\x84\xB1\x9C\x20\x00\x00", "\x8B\x04\xB5", true  },
        { "\x8B\x84\x91\x9C\x00\x00\x00", "\x8B\x04\x95", false },
        { "\x8B\x8C\x91\x9C\x20\x00\x00", "\x8B\x0C\x95", true  },
        { "\x8B\x94\xB9\x9C\x00\x00\x00", "\x8B\x14\xBD", false },
        { "\x89\x94\xB9\x9C\x00\x00\x00", "\x89\x14\xBD", false },
        { "\x89\x94\xB9\x9C\x20\x00\x00", "\x89\x14\xBD", true  },
    };
    const int kCount = sizeof(kSites) / sizeof(kSites[0]);

    // Tim du HET truoc khi dong vao bat cu thu gi.
    uint8_t* found[kCount];
    for (int i = 0; i < kCount; i++) {
        found[i] = FindPattern("engine.dll", kSites[i].sig, "xxxxxxx");
        if (!found[i]) {
            EL_LOG("[EdictBudget] snapshot table access %d/%d not found - "
                     "extended range stays DISABLED.", i + 1, kCount);
            return false;
        }
    }

// Hai bang phai duoc xoa moi map, NHUNG KHONG duoc sua ham xoa cua engine:
// lam vay tung pha chu ky cua extension cutlrbtreefix. Xem docs/03-huong-4096.md

    g_PackedTable = (uint32_t*)_aligned_malloc(EXT_LIMIT * sizeof(uint32_t), 16);
    g_SerialTable = (uint32_t*)_aligned_malloc(EXT_LIMIT * sizeof(uint32_t), 16);
    if (!g_PackedTable || !g_SerialTable) return false;
    memset(g_PackedTable, 0, EXT_LIMIT * sizeof(uint32_t));
    memset(g_SerialTable, 0, EXT_LIMIT * sizeof(uint32_t));

    // Bat dau tu ZERO la DUNG chu khong chi la tien: Metamod nap plugin luc may
    // chu khoi dong, TRUOC map dau tien, nen bang goc khong giu gi dang mang theo.
    // Chep lai co nghia la tin vao mot phong doan xem singleton nao so huu chung,
    // ma doan sai thi bang cua ta se duoc gieo bang con tro packed-entity treo.

    for (int i = 0; i < kCount; i++) {
        uint8_t buf[7];
        memcpy(buf, kSites[i].head, 3);
        *(uint32_t*)(buf + 3) = (uint32_t)(kSites[i].serial ? g_SerialTable : g_PackedTable);
        WriteProtected(found[i], buf, sizeof(buf));
    }

    EL_LOG("[EdictBudget] snapshot tables relocated: packed=%p serial=%p "
             "(%d entries each, %d accesses rewritten, cleared from our own hook)",
             g_PackedTable, g_SerialTable, EXT_LIMIT, kCount);
    return true;
}


// ==========================================================================
//  FREEGATE - ba che do. Xem docs/01-co-che.md va tools/freegate-hong-gear-transfer.md
//
//    0 = TAT. Cong 1 giay cua engine nguyen ven.
//    1 = CO DANH SACH (mac dinh). Moc IVEngineServer::RemoveEdict (vtable slot
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


// trap: bay tai CHINH nhanh loi cua ED_Alloc, in ban kiem ke truoc khi chet.
// Chi ghi log, khong doi hanh vi. Xem docs/05-do-dac.md
static uint8_t* g_AllocFailStub = NULL;
static int g_AllocFailReports = 0;

static void __cdecl LogAllocFail(int ebxVal)
{
    if (g_AllocFailReports >= 4) return;
    g_AllocFailReports++;

    int freeCount = -1;
    if (gpGlobals && gpGlobals->pEdicts && g_num_edicts) {
        int n = (int)*g_num_edicts;
        if (n > EXT_LIMIT) n = EXT_LIMIT;
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

// Kiem ke tai thoi diem het edict: cai gi dang chiem 2048 slot. docs/05-do-dac.md
    if (gpGlobals && gpGlobals->pEdicts && g_num_edicts && g_AllocFailReports == 1) {
        int n = (int)*g_num_edicts;
        if (n > EXT_LIMIT) n = EXT_LIMIT;

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

// SV_AllocateEdicts mo dau bang "mov eax, 0x800" roi lay so do de dinh kich thuoc
// ca mang edict lan mang changeinfo. Nang mot gia tri tuc thoi do la CHINH ENGINE
// se cap phat mot mang 4096 muc that su, dung thoi diem, khong can ta lam tro gi
// voi con tro.
// Dword o cuoi la dia chi tuyet doi DA DUOC DOI THEO BASE, nen phai dat mat na.
static bool PatchEngineAllocSize() {
    const char* sig  = "\xB8\x00\x08\x00\x00\x89\x86\x18\x02\x00\x00\xA3\x00\x00\x00\x00";
    const char* mask = "xxxxxxxxxxxx....";
    uint8_t* m = FindPattern("engine.dll", sig, mask);
    if (!m) {
        EL_LOG("[EdictBudget] SV_AllocateEdicts signature not found - staying at 2048.");
        return false;
    }
    uint32_t v = EXT_LIMIT;
    WriteProtected(m + 1, &v, sizeof(v));
    EL_LOG("[EdictBudget] SV_AllocateEdicts now allocates %d edicts (patched at %p)", EXT_LIMIT, m);
    return true;
}

// Hai ham tra chi so cua engine kiem bien theo num_edicts / max_edicts, va co
// BON cho bo cuoc nam trong BA ham prologue khac nhau. Neo theo chuoi loi chu
// KHONG neo theo prologue. Xem docs/03-huong-4096.md
static int PatchAbortsFor(uint8_t* base, size_t size, const char* text) {
    size_t tlen = strlen(text);

    // Tim chuoi truoc, roi san lenh "push <dia chi luc chay cua no>". Doc dia chi
    // ra tu chinh anh module thi khong so bi doi theo base.
    uint8_t* str = NULL;
    for (size_t i = 0; i + tlen + 1 <= size; i++) {
        if (memcmp(base + i, text, tlen + 1) == 0) { str = base + i; break; }
    }
    if (!str) return 0;

    int done = 0;
    for (size_t i = 3; i + 5 <= size; i++) {
        if (base[i] != 0x68) continue;
        if (*(uint8_t**)(base + i + 1) != str) continue;

// Co HAI bo cuc cua lenh canh (lui 2 byte va lui 3 byte). docs/03-huong-4096.md
        size_t at;
        if (base[i - 2] == 0x7C)                                   at = i - 2;
        else if (base[i - 3] == 0x7C && base[i - 1] >= 0x50
                                     && base[i - 1] <= 0x57)       at = i - 3;
        else continue;

        uint8_t jmp = 0xEB;
        WriteProtected(base + at, &jmp, 1);
        EL_LOG("[EdictBudget] bounds abort at %p disabled (%s)", base + at, text);
        done++;
    }
    return done;
}

static void PatchIndexOfEdictBounds() {
    HMODULE h = GetModuleHandle("engine.dll");
    if (!h) return;
    MODULEINFO mi;
    if (!GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) return;

    uint8_t* base = (uint8_t*)mi.lpBaseOfDll;
    size_t size = mi.SizeOfImage;

    // EDICT_NUM la phep tra chi so -> edict_t* nam sau PEntityOfEntIndex, ma
    // SourceMod, cac plugin va ban than game goi lien tuc. No so chi so voi
    // max_edicts, ma ta thi CO Y giu con so do o 2048 - nen moi lan tra cuu mot
    // entity ta da dat vao 2048..4095 deu giet may chu.
    int n = PatchAbortsFor(base, size, "NUM_FOR_EDICT: bad pointer")
          + PatchAbortsFor(base, size, "NUM_FOR_EDICTINFO: bad pointer")
          + PatchAbortsFor(base, size, "EDICT_NUM: bad number %i");

    if (n == 0) EL_LOG("[EdictBudget] no index-lookup bounds aborts found!");
    else        EL_LOG("[EdictBudget] index-lookup: %d bounds aborts disabled, "
                         "index preserved at every one (expect 6)", n);

    // Kiem xem gpGlobals->maxEntities THUC SU roi vao dau. Bo cuc CGlobalVars
    // trong SDK chi la mot GIA DINH, va hai truong nam hai ben maxEntities la
    // nTimestampNetworkingBase (100) va nTimestampRandomizeWindow (32) - chung
    // dieu khien cach ma hoa thuoc tinh theo so tick. Ghi 2048 de len mot trong
    // hai truong do la lam hong ma hoa tren toan may chu - nen phai KIEM chu
    // khong duoc GIA DINH.
    if (gpGlobals) {
        void* p = &gpGlobals->maxEntities;
        EL_LOG("[EdictBudget] gpGlobals=%p  &maxEntities=%p (engine+0x%X)  value=%d",
                 gpGlobals, p, (uint32_t)((uint8_t*)p - base), gpGlobals->maxEntities);
    }
}

// Nhanh ep-chi-so cua ED_Alloc kiem theo num_edicts nen luon tu choi 2048+.
// So voi mot hang so thay vi nang max_edicts. Xem docs/03-huong-4096.md
static void PatchForcedIndexCheck() {
    const char* sig  = "\x55\x8B\xEC\x8B\x45\x08\x56\x85\xC0\x78\x00\x3B\x05\x00\x00\x00\x00\x7C";
    const char* mask = "xxxxxxxxxx.xx....x";
    uint8_t* m = FindPattern("engine.dll", sig, mask);
    if (!m) {
        EL_LOG("[EdictBudget] ED_Alloc forced-index signature not found.");
        return;
    }
    uint8_t buf[6] = { 0x3D, 0, 0, 0, 0, 0x90 };
    *(uint32_t*)(buf + 1) = EXT_LIMIT;
    WriteProtected(m + 11, buf, sizeof(buf));
    EL_LOG("[EdictBudget] ED_Alloc forced-index now compares against the "
             "constant %d - no engine global is touched at runtime (at %p)", EXT_LIMIT, m);
}

// ==========================================================================
// Detour CreateEntityByName - cach duy nhat de biet classname cua entity ma lan
// goi CreateEdict sap toi thuoc ve.
//
// Ban server.dll nay KHONG co chuoi "CreateEntityByName\0" doc lap, nen ta neo
// theo thong bao loi duy nhat nam ben trong chinh ham do. Prologue that su dai
// 7 byte (55 8B EC 56 8B 75 0C); cuop 5 byte nhu thong le se cat doi "8B 75 0C"
// va thuc thi rac.
// ==========================================================================
typedef void* (__cdecl *CreateEntityByName_t)(const char*, int);
static CreateEntityByName_t g_OrigCreateEntity = NULL;
static uint8_t* g_DetourAt = NULL;
static uint8_t  g_DetourSaved[7];
static uint8_t* g_Trampoline = NULL;
static const char* g_PendingClass = NULL;

static void* __cdecl Detour_CreateEntityByName(const char* cls, int forceIndex) {
    const char* prev = g_PendingClass;   // loi tao long nhau khong duoc ke thua
    g_PendingClass = cls;
    CensusAdd(cls);
    void* r = g_OrigCreateEntity(cls, forceIndex);
    g_PendingClass = prev;

    // Mot map het edict thi khong bao gio toi duoc ServerActivate, nen ban kiem ke
    // se khong bao gio duoc in ra cho DUNG cai map ma ta can no nhat.
    // Thay vao do, do ra MOT LAN ngay khi dai co mang bat dau day.
    if (!g_Tripped && g_num_edicts && *g_num_edicts >= CENSUS_TRIP) {
        g_Tripped = true;
        EL_LOG("[EdictBudget] --- num_edicts reached %d, early census ---",
                 (int)*g_num_edicts);
        CensusDump();
    }
    return r;
}

// WIPECLEAR - don thuc the o DAU chuoi restart, TRUOC vong hoi sinh player.
// Dung nguyen preserve list 38 lop cua game. Xem docs/01-co-che.md

// Nghe su kien: CHI DE CHAN DOAN, khong con quyen chan. Xem docs/01-co-che.md
static bool  g_LossPending   = false;
static float g_LastLossTime  = -1.0f;
static char  g_LastLossName[32] = "";
static int   g_LossSignals   = 0;

// WIPE = doi survivor THUA = su kien 'mission_lost'.
// CHI 'mission_lost' duoc mo cong. 'round_end' cung ban khi THANG (qua chuong,
// finale) nen chi ghi log de doi chieu thu tu, KHONG mo cong.
class CLossListener : public IGameEventListener2 {
public:
    virtual void FireGameEvent(IGameEvent *ev) {
        if (!ev) return;
        const char* n = ev->GetName();
        if (!n) return;
        float t = gpGlobals ? gpGlobals->curtime : 0.0f;

        if (_stricmp(n, "mission_lost") == 0) {
            g_LossPending  = true;
            g_LastLossTime = t;
            strncpy(g_LastLossName, n, sizeof(g_LastLossName) - 1);
            g_LastLossName[sizeof(g_LastLossName) - 1] = 0;
            g_LossSignals++;
            EL_LOG("[EdictBudget] WIPECLEAR: *** WIPE (mission_lost) *** t=%.2f "
                     "(lan thu %d) - MO CONG", t, g_LossSignals);
        } else {
            // chi doi chieu thu tu, khong mo cong
            EL_LOG("[EdictBudget] WIPECLEAR: (doi chieu) su kien '%s' t=%.2f "
                     "- KHONG mo cong", n, t);
        }
    }
    virtual int GetEventDebugID() { return 42; }
};
static CLossListener g_LossListener;
static bool g_LossHooked = false;

typedef void  (__fastcall *fnCleanupDeleteList_t)(void*, void*);
typedef void* (__fastcall *fnNextEnt_t)(void*, void*, void*);
typedef void  (__cdecl    *fnUtilRemove_t)(void*);
typedef void  (__fastcall *fnRestartRound_t)(void*, void*);

static fnCleanupDeleteList_t g_CleanupDeleteList = NULL;
static fnNextEnt_t           g_NextEnt           = NULL;
static fnUtilRemove_t        g_UtilRemove        = NULL;
static void*                 g_EntList           = NULL;
static bool*                 g_InCleanupDelete   = NULL;
static const char**          g_PreserveList      = NULL;
static fnRestartRound_t      g_OrigRestartRound  = NULL;
static void**                g_RestartSlot       = NULL;
static int                   g_WipeClearRuns     = 0;

static int CountFreeEdicts() {
    if (!gpGlobals || !gpGlobals->pEdicts || !g_num_edicts) return -1;
    int n = (int)*g_num_edicts;
    if (n > EXT_LIMIT) n = EXT_LIMIT;
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

// Danh sach giu bo sung - wipekeep.txt. DE TRONG moi dung: o wipe, entity bi
// xoa se DUOC DUNG LAI tu entity lump. Xem docs/01-co-che.md
#define KEEP_MAX 128
static char g_KeepExtra[KEEP_MAX][48];
static int  g_KeepCount = 0;

static void LoadWipeKeep() {
    g_KeepCount = 0;
    FILE* f = OpenPluginFile("wipekeep.txt", "r");
    if (!f) {
        EL_LOG("[EdictBudget] WIPECLEAR: khong co wipekeep.txt - chi dung preserve list cua game");
        return;
    }
    // Buffer PHAI du rong, va phai xa phan du neu dong dai hon buffer.
    // line[64] lam dong chu thich dai bi cat doi; phan duoi khong
    // bat dau bang '#' nen lot vao danh sach nhu mot ten lop rac.
    // File 15 dong ma nap ra 31 muc.
    char line[256];
    while (fgets(line, sizeof(line), f) && g_KeepCount < KEEP_MAX) {
        size_t L = strlen(line);
        if (L > 0 && line[L - 1] != '\n') {          // dong bi cat -> xa het dong
            int c; while ((c = fgetc(f)) != EOF && c != '\n') {}
        }
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\r' || *p == '\n' || !*p) continue;
        char* e = p + strlen(p) - 1;
        while (e >= p && (*e == '\r' || *e == '\n' || *e == ' ' || *e == '\t')) *e-- = 0;
        if (!*p) continue;
        // Chi nhan ky tu hop le cho classname - chan not moi thu rac con sot
        bool ok = true;
        for (const char* q = p; *q; q++) {
            if (!((*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') ||
                  (*q >= '0' && *q <= '9') || *q == '_')) { ok = false; break; }
        }
        if (!ok) {
            EL_LOG("[EdictBudget] WIPECLEAR: wipekeep.txt bo qua dong khong hop le: '%s'", p);
            continue;
        }
        strncpy(g_KeepExtra[g_KeepCount], p, sizeof(g_KeepExtra[0]) - 1);
        g_KeepExtra[g_KeepCount][sizeof(g_KeepExtra[0]) - 1] = 0;
        g_KeepCount++;
    }
    fclose(f);

    for (int i = 0; i < g_KeepCount; i++)
        EL_LOG("[EdictBudget] WIPECLEAR:   giu them [%d] '%s'", i, g_KeepExtra[i]);
    EL_LOG("[EdictBudget] WIPECLEAR: wipekeep.txt - giu them %d lop", g_KeepCount);
}

static bool InKeepExtra(const char* cls) {
    if (g_KeepCount <= 0 || !cls || !*cls) return false;
    for (int i = 0; i < g_KeepCount; i++) {
        const char* k = g_KeepExtra[i];
        size_t n = strlen(k);
        if (n == 0) continue;
        if (k[n - 1] == '_') {                       // khop tien to ca ho
            if (strncmp(cls, k, n) == 0) return true;
        } else if (strcmp(cls, k) == 0) {            // khop chinh xac
            return true;
        }
    }
    return false;
}

static void __fastcall Hook_RestartRound(void* thisptr, void* edx) {
    if (!g_OrigRestartRound) return;

// Cong chan wipeclear (khoi phuc 07/08 sau khi do that te). docs/01-co-che.md
    float now = gpGlobals ? gpGlobals->curtime : 0.0f;
    float ago = (g_LastLossTime >= 0.0f) ? (now - g_LastLossTime) : -1.0f;

    EL_LOG("[EdictBudget] WIPECLEAR: RestartRound goi luc t=%.2f | "
             "mission_lost gan nhat: %s (cach %.2fs, tong %d) | cong: %s | slot trong=%d",
             now, g_LastLossName[0] ? g_LastLossName : "(chua bao gio ban)",
             ago, g_LossSignals, g_LossPending ? "MO -> SE DON" : "DONG -> BO QUA",
             CountFreeEdicts());

    if (!g_LossPending) { g_OrigRestartRound(thisptr, edx); return; }
    g_LossPending = false;               // dung mot lan cho moi wipe

    // wipeclear=1: chi quan sat. Van log day du hai dau moc quanh RestartRound
    // de biet num_edicts cham 2048 truoc hay sau, nhung KHONG xoa gi.
    if (g_WipeClear < 2) {
        EL_LOG("[EdictBudget] WIPECLEAR[quan sat]: TRUOC RestartRound "
                 "slot trong=%d num_edicts=%d", CountFreeEdicts(),
                 g_num_edicts ? (int)*g_num_edicts : -1);
        g_OrigRestartRound(thisptr, edx);
        EL_LOG("[EdictBudget] WIPECLEAR[quan sat]: SAU RestartRound "
                 "slot trong=%d num_edicts=%d", CountFreeEdicts(),
                 g_num_edicts ? (int)*g_num_edicts : -1);
        return;
    }

    int before = CountFreeEdicts();
    int removed = 0, kept = 0, keptExtra = 0;
    bool did = false;

    // Tai nhap: CleanupDeleteList KHONG co bao ve tai nhap tren ban retail
    // (g_fInCleanupDelete chi duoc doc trong #ifdef DEBUG). Phai tu kiem.
    if (g_InCleanupDelete && *g_InCleanupDelete) {
        EL_LOG("[EdictBudget] WIPECLEAR: dang trong CleanupDeleteList -> bo qua luot nay");
    } else if (g_NextEnt && g_UtilRemove && g_EntList && g_CleanupDeleteList) {
        void* e = g_NextEnt(g_EntList, NULL, NULL);          // FirstEnt = NextEnt(NULL)
        while (e) {
            void* next = g_NextEnt(g_EntList, NULL, e);      // lay truoc khi go
            const char* cls = *(const char**)((uint8_t*)e + 0x74);   // m_iClassname
            if (InPreserveList(cls))      kept++;            // game von giu
            else if (InKeepExtra(cls))  { kept++; keptExtra++; }  // ta giu them
            else { g_UtilRemove(e); removed++; }
            e = next;
        }
        g_CleanupDeleteList(g_EntList, NULL);                // tra edict ngay
        if (engine) engine->AllowImmediateEdictReuse();      // bo thoi gian cho
        did = true;
    }

    int after = CountFreeEdicts();
    g_WipeClearRuns++;
    EL_LOG("[EdictBudget] WIPECLEAR #%d %s: slot trong %d -> %d (chenh %d) | "
             "go %d, giu %d (trong do %d do wipekeep.txt) | num_edicts=%d",
             g_WipeClearRuns, did ? "DA DON" : "BO QUA", before, after,
             (before >= 0 && after >= 0) ? (after - before) : -1,
             removed, kept, keptExtra, g_num_edicts ? (int)*g_num_edicts : -1);

    g_OrigRestartRound(thisptr, edx);

    EL_LOG("[EdictBudget] WIPECLEAR #%d: SAU RestartRound slot trong=%d num_edicts=%d",
             g_WipeClearRuns, CountFreeEdicts(), g_num_edicts ? (int)*g_num_edicts : -1);
}

// MAPCLEAR - don entity TRUOC KHI ENGINE don, luc CHUYEN MAN.
// De mapclear=1 (CHI QUAN SAT). Muc 2 chua chung minh duoc loi ich va da
// tung lam sap server. Xem docs/02-mapclear.md

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
// toan, nguoi choi, cong toan bo preserve list cua game. docs/02-mapclear.md
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
    if (!g_CleanupDeleteList) g_CleanupDeleteList = (fnCleanupDeleteList_t)(b + 0x0B5D10);
    if (!g_NextEnt)           g_NextEnt           = (fnNextEnt_t)          (b + 0x0B4270);
    if (!g_UtilRemove)        g_UtilRemove        = (fnUtilRemove_t)       (b + 0x2071E0);
    if (!g_EntList)           g_EntList           = (void*)                (b + 0x7E0760);
    if (!g_PreserveList)      g_PreserveList      = (const char**)         (b + 0x7ACE40);

// Cong an toan 1: prologue cua 0x102B8140 phai dung. Prologue chua dia chi
// tuyet doi DA DOI THEO BASE nen phai dung MAT NA. Xem docs/02-mapclear.md
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


// NOEDICT - khien mot so lop KHONG BAO GIO duoc cap edict, bang cach dat
// EFL_SERVER_ONLY (bit 9 cua m_iEFlags) trong PostConstructor (vtable slot 29).
// Day la co che MANH NHAT va mat mat bang 0. Xem docs/01-co-che.md
typedef void (__fastcall *PostCtor_t)(void*, void*, const char*);
static PostCtor_t g_OrigPostCtor = NULL;

#define NOEDICT_MAX 32
static char  g_NoEdictList[NOEDICT_MAX][40];
static int   g_NoEdictCount = 0;
static void* g_PatchedVt[NOEDICT_MAX];      // vtable da sua, de go khi unload
static int   g_PatchedVtCount = 0;

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
    while (fgets(line, sizeof(line), f) && g_NoEdictCount < NOEDICT_MAX) {
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
        strncpy(g_NoEdictList[g_NoEdictCount], p, sizeof(g_NoEdictList[0])-1);
        g_NoEdictList[g_NoEdictCount][sizeof(g_NoEdictList[0])-1] = 0;
        g_NoEdictCount++;
    }
    fclose(f);
    EL_LOG("[EdictBudget] NOEDICT: %d lop trong danh sach", g_NoEdictCount);
    for (int i = 0; i < g_NoEdictCount; i++)
        EL_LOG("[EdictBudget] NOEDICT:   [%d] '%s'", i, g_NoEdictList[i]);
}

static int g_NoEdictHits = 0;

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
static void** ScanForVtableStore(uint8_t* base, uint8_t* fn) {
    void** found = NULL;
    for (int i = 0; i < 80; i++) {
        if (fn[i] != 0xC7) continue;
        uint8_t modrm = fn[i+1];
        if (modrm > 0x07 || modrm == 0x04 || modrm == 0x05) continue;   // bo SIB/disp32
        unsigned int imm = *(unsigned int*)(fn + i + 2);
        if (imm < (unsigned int)(uintptr_t)base) continue;
        if (found) return NULL;
        found = (void**)(uintptr_t)imm;
    }
    return found;
}

// Tim vtable cua mot lop qua factory cua no. Tra NULL neu khong chac chan.
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
    if (!fvt) return NULL;
    uint8_t* create = (uint8_t*)fvt[0];                     // slot 0 = Create
    if (!create) return NULL;

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
            if (target >= base) {
                void** viaCall = ScanForVtableStore(base, target);
                if (viaCall) return viaCall;                  // thay thi lay, khong thi THU TIEP
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


static bool InstallWipeClear() {
    if (g_OrigRestartRound) return true;              // da cai roi
    if (g_WipeClear <= 0) return false;               // 0 = no-op that su, khong dung vtable

    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* b = (uint8_t*)mi.lpBaseOfDll;

    // Xac minh prologue RestartRound truoc khi dong vao vtable.
    // 0x2E0650: 55 8B EC 0F 57 C0 83 EC 08 53 33 DB 56 57 8B F9
    static const uint8_t kSig[] = {
        0x55,0x8B,0xEC,0x0F,0x57,0xC0,0x83,0xEC,0x08,0x53,0x33,0xDB,0x56,0x57,0x8B,0xF9 };
    uint8_t* fn = b + 0x2E0650;
    if (memcmp(fn, kSig, sizeof(kSig)) != 0) {
        EL_LOG("[EdictBudget] WIPECLEAR: prologue RestartRound KHONG khop "
                 "- server.dll khac ban? Bo qua, khong dong gi.");
        return false;
    }

    void*** grPtr = (void***)(b + 0x7F7F6C);          // g_pGameRules
    void** gr = *grPtr;
    if (!gr) { EL_LOG("[EdictBudget] WIPECLEAR: g_pGameRules con NULL, hoan lai"); return false; }

    void** vtbl = *(void***)gr;
    void** slot = &vtbl[178];                         // CTerrorGameRules::RestartRound
    if (*slot != (void*)fn) {
        EL_LOG("[EdictBudget] WIPECLEAR: vtable slot 178 = %p, khong phai RestartRound %p "
                 "- co plugin khac da moc? Bo qua.", *slot, fn);
        return false;
    }

    g_CleanupDeleteList = (fnCleanupDeleteList_t)(b + 0x0B5D10);
    g_NextEnt           = (fnNextEnt_t)          (b + 0x0B4270);
    g_UtilRemove        = (fnUtilRemove_t)       (b + 0x2071E0);
    g_EntList           = (void*)                (b + 0x7E0760);
    g_InCleanupDelete   = (bool*)                (b + 0x7E0730);
    g_PreserveList      = (const char**)         (b + 0x7ACE40);

    // Kiem preserve list doc dung khong: phan tu 0 phai la "ai_network",
    // phan tu 33 phai la "predicted_viewmodel".
    const char* p0  = g_PreserveList[0];
    const char* p33 = g_PreserveList[33];
    if (!p0 || !p33 || strcmp(p0, "ai_network") != 0 || strcmp(p33, "predicted_viewmodel") != 0) {
        EL_LOG("[EdictBudget] WIPECLEAR: preserve list sai ([0]=%s [33]=%s) - bo qua.",
                 p0 ? p0 : "(null)", p33 ? p33 : "(null)");
        g_PreserveList = NULL;
        return false;
    }

    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    g_OrigRestartRound = (fnRestartRound_t)*slot;
    *slot = (void*)&Hook_RestartRound;
    VirtualProtect(slot, sizeof(void*), old, &old);
    g_RestartSlot = slot;

    LoadWipeKeep();

    // Dang ky nghe tin hieu thua. mission_lost = "survivor team that bai"
    // (chu thich cua Valve trong modevents.res). round_end kem theo de doi chieu.
    if (gameevents && !g_LossHooked) {
        bool a = gameevents->AddListener(&g_LossListener, "mission_lost", true);
        bool b = gameevents->AddListener(&g_LossListener, "round_end",    true);
        g_LossHooked = (a || b);
        EL_LOG("[EdictBudget] WIPECLEAR: nghe su kien mission_lost=%d round_end=%d", a, b);
    }

    EL_LOG("[EdictBudget] WIPECLEAR: da moc vtable slot 178 (RestartRound @ %p), "
             "preserve list OK, wipeclear=%d, cong = co mot-lan tu mission_lost",
             fn, g_WipeClear);
    return true;
}

static void RemoveWipeClear() {
    if (gameevents && g_LossHooked) {
        gameevents->RemoveListener(&g_LossListener);
        g_LossHooked = false;
    }
    if (g_RestartSlot && g_OrigRestartRound) {
        DWORD old;
        if (VirtualProtect(g_RestartSlot, sizeof(void*), PAGE_READWRITE, &old)) {
            *g_RestartSlot = (void*)g_OrigRestartRound;
            VirtualProtect(g_RestartSlot, sizeof(void*), old, &old);
        }
    }
    g_RestartSlot = NULL;
    g_OrigRestartRound = NULL;
}

static bool InstallDetour() {
    HMODULE h = GetModuleHandle("server.dll");
    if (!h) return false;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* base = (uint8_t*)mi.lpBaseOfDll;
    size_t size = mi.SizeOfImage;

    const char* anchor = "CreateEntityByName( %s, %d ) - CreateEdict failed.";
    size_t alen = strlen(anchor);
    uint8_t* str = NULL;
    for (size_t i = 0; i + alen + 1 < size; i++) {
        if (memcmp(base + i, anchor, alen + 1) == 0) { str = base + i; break; }
    }
    if (!str) {
        EL_LOG("[EdictBudget] detour: anchor string not found.");
        return false;
    }

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    uint8_t* text = NULL; size_t textLen = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (strncmp((char*)sec[i].Name, ".text", 5) == 0) {
            text = base + sec[i].VirtualAddress;
            textLen = sec[i].Misc.VirtualSize;
            break;
        }
    }
    if (!text) return false;

    uint32_t strVal = (uint32_t)str;
    uint8_t* fn = NULL;
    for (size_t i = 0; i + 5 < textLen; i++) {
        if (text[i] == 0x68 && *(uint32_t*)(text + i + 1) == strVal) {
            for (int back = 1; back < 200 && (size_t)back <= i; back++) {
                uint8_t* c = text + i - back;
                if (c[0] == 0x55 && c[1] == 0x8B && c[2] == 0xEC) { fn = c; break; }
            }
            if (fn) break;
        }
    }
    if (!fn) {
        EL_LOG("[EdictBudget] detour: prologue not located.");
        return false;
    }

    static const uint8_t expect[7] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75, 0x0C };
    if (memcmp(fn, expect, sizeof(expect)) != 0) {
        EL_LOG("[EdictBudget] detour: prologue mismatch at %p - aborted (safe).", fn);
        return false;
    }

    g_DetourAt = fn;
    memcpy(g_DetourSaved, fn, sizeof(g_DetourSaved));

    g_Trampoline = (uint8_t*)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_Trampoline) return false;
    memcpy(g_Trampoline, g_DetourSaved, sizeof(g_DetourSaved));
    g_Trampoline[7] = 0xE9;
    *(uint32_t*)(g_Trampoline + 8) = (uint32_t)(fn + 7) - (uint32_t)(g_Trampoline + 12);
    g_OrigCreateEntity = (CreateEntityByName_t)g_Trampoline;

    uint8_t patch[7];
    patch[0] = 0xE9;
    *(uint32_t*)(patch + 1) = (uint32_t)Detour_CreateEntityByName - (uint32_t)(fn + 5);
    patch[5] = 0x90;
    patch[6] = 0x90;
    WriteProtected(fn, patch, sizeof(patch));

    EL_LOG("[EdictBudget] detour installed at %p", fn);
    return true;
}

static void RemoveDetour() {
    if (!g_DetourAt || !g_OrigCreateEntity) return;
    WriteProtected(g_DetourAt, g_DetourSaved, sizeof(g_DetourSaved));
    if (g_Trampoline) { VirtualFree(g_Trampoline, 0, MEM_RELEASE); g_Trampoline = NULL; }
    g_OrigCreateEntity = NULL;
    g_DetourAt = NULL;
}

// ==========================================================================
// Hook CreateEdict - toan bo muc dich cua nhom cong tac 4096
// ==========================================================================
static edict_t* Hook_CreateEdict(int forceIndex) {
    // Ben goi da yeu cau MOT o cu the; tuyet doi khong doan lai y ho.
    if (forceIndex > 0) RETURN_META_VALUE(MRES_IGNORED, nullptr);

    // Moi thu khong nam trong danh sach cho phep deu de nguyen cho engine lo,
    // ma bo cap phat cua no bi chan boi max_edicts = 2048 - tuc hanh vi goc.
    if (!g_ExtReady || !MayLiveHigh(g_PendingClass)) {
        RETURN_META_VALUE(MRES_IGNORED, nullptr);
    }
    if (!gpGlobals || !gpGlobals->pEdicts) RETURN_META_VALUE(MRES_IGNORED, nullptr);

// Cap phat DI XUONG tu dinh cua dai, de khong dam vao be khong-co-mang cua
// server.dll dang tieu thu di LEN tu 2048. Xem docs/03-huong-4096.md
    for (int n = 0; n < EXT_LIMIT - NET_LIMIT; n++) {
        int i = g_Cursor - n;
        if (i < NET_LIMIT) i += (EXT_LIMIT - NET_LIMIT);
        edict_t* e = gpGlobals->pEdicts + i;
        if (!e->IsFree()) continue;

        // O day KHONG con nang gioi han nao nua. Nhanh ep-chi-so cua ED_Alloc gio
        // so chi so voi mot hang so nuong thang vao ma (xem PatchForcedIndexCheck),
        // nen max_edicts giu nguyen 2048 suot doi may chu. Nang no du chi trong
        // choc lat cung tung du de num_edicts bo qua 2048 va tran VINH VIEN danh
        // sach snapshot 2048 muc cua engine nam tren ngan xep.
        edict_t* got = SH_CALL(engine, &IVEngineServer::CreateEdict)(i);

        if (got) {
            AuditAdd(g_PendingClass);
            g_Cursor = (i - 1 < NET_LIMIT) ? (EXT_LIMIT - 1) : (i - 1);
            RETURN_META_VALUE(MRES_SUPERCEDE, got);
        }
    }

    // Extension range full - fall back to the engine. Costs a networked slot
    // but never corrupts anything.
    RETURN_META_VALUE(MRES_IGNORED, nullptr);
}

// ==========================================================================
// Plugin lifecycle
// ==========================================================================
static void ReadStage() {
    FILE* f = OpenPluginFile("stage.txt", "r");
    if (!f) return;
    int v = -1;
    if (fscanf(f, "%d", &v) == 1 && v >= 0 && v <= 1) g_Stage = v;
    fclose(f);
}

bool SamplePlugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
    PLUGIN_SAVEVARS();
    GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
    GET_V_IFACE_ANY(GetServerFactory, server, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
    GET_V_IFACE_ANY(GetServerFactory, gameclients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
    GET_V_IFACE_CURRENT(GetEngineFactory, gameevents, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2);
    gpGlobals = ismm->GetCGlobals();

    ReadStage();
    EL_LOG("[EdictBudget] ===== stage %d (0=inert, 1=active) =====", g_Stage);

    SH_ADD_HOOK_MEMFUNC(IServerGameDLL, LevelInit, server, this, &SamplePlugin::Hook_LevelInit, false);
    SH_ADD_HOOK_MEMFUNC(IServerGameDLL, ServerActivate, server, this, &SamplePlugin::Hook_ServerActivate, false);
    SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameFrame, server, this, &SamplePlugin::Hook_GameFrame, false);

    if (g_Stage == 0) return true;

    if (!ResolveEngineGlobals()) return true;

    LoadPatchSwitches();
    LoadAllowList();
    // Dong trang thai - PHAI o day, ngoai moi nhanh dieu kien.
    // Truoc do no nam trong khoi "if (mo duoc serveronly.txt)" nen khi khong
    // co file do thi khong bao gio in ra.
    EL_LOG("[EdictBudget] noedict=%d mapclear=%d mapclearmax=%d mapclearcarry=%d "
             "heartbeat=%d loadprobe=%d swap=%d swapmax=%d logconsole=%d",
             g_NoEdict ? 1 : 0, g_MapClear, g_MapClearMax, g_MapClearCarryOnly,
             g_Heartbeat, g_LoadProbe, g_Swap, g_SwapMax, g_LogConsole ? 1 : 0);
    if (g_PatchFreetime)    PatchFreetime();
    if (g_PatchIndexBounds) PatchIndexOfEdictBounds();
    if (g_PatchForcedIndex) PatchForcedIndexCheck();
    if      (g_PatchFreeGate == 1) InstallFreeGateList();   // co danh sach freekeep.txt
    else if (g_PatchFreeGate >= 2) PatchFreetimeGate();     // vo dieu kien (che do cu)
    if (g_PatchTrap) PatchAllocFailTrap();

    // NOEDICT phai cai o DAY (luc Load), khong phai ServerActivate: vtable cua
    // server.dll co san ngay khi module nap, va thunk phai co mat TRUOC khi
    // entity dau tien cua map duoc tao.
    if (g_NoEdict) { LoadNoEdictList(); InstallNoEdict(); }
    // SWAP phai cai TRUOC khi map dau tien phan tich lump entity, vi chinh bo phan
    // tich lump cung di qua dictionary slot 1. Cai o day la som nhat co the.
    if (g_Swap > 0) { LoadSwapList(); InstallSwap(); }

    // CA HAI deu phai thanh cong. Mot mang edict 4096 muc di kem bang snapshot chi
    // 2048 muc thi TE HON HAN viec khong lam gi ca: nhung edict them ra se dung
    // duoc binh thuong cho toi dung luc chung am tham ghi de len trang thai engine
    // nam ke ben.
    g_BigArrayOn = g_PatchBigArray ? PatchEngineAllocSize() : false;
    bool bigTables = g_PatchSnapshot ? PatchSnapshotTables() : false;
    g_EngineArray4096 = g_BigArrayOn && bigTables;

    if (g_EngineArray4096 || g_ImmediateReuse) {
        SH_ADD_HOOK(IVEngineServer, CreateEdict, engine, SH_STATIC(Hook_CreateEdict), false);
        SH_ADD_HOOK(IVEngineServer, CreateEdict, engine, SH_STATIC(Hook_CreateEdict_Post), true);
    }
    if (!g_EngineArray4096) {
        EL_LOG("[EdictBudget] segregation disabled (edict array %s, snapshot tables %s).",
                 g_BigArrayOn ? "ok" : "FAILED", bigTables ? "ok" : "FAILED");
    }
    return true;
}

bool SamplePlugin::Unload(char *error, size_t maxlen)
{
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, LevelInit, server, this, &SamplePlugin::Hook_LevelInit, false);
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, ServerActivate, server, this, &SamplePlugin::Hook_ServerActivate, false);
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameFrame, server, this, &SamplePlugin::Hook_GameFrame, false);
    if (g_EngineArray4096 || g_ImmediateReuse) {
        SH_REMOVE_HOOK(IVEngineServer, CreateEdict, engine, SH_STATIC(Hook_CreateEdict), false);
        SH_REMOVE_HOOK(IVEngineServer, CreateEdict, engine, SH_STATIC(Hook_CreateEdict_Post), true);
    }
    RemoveDetour();
    RemoveWipeClear();
    RemoveMapClear();
    EL_LOG_CLOSE();
    UninstallNoEdict();
    UninstallSwap();
    return true;
}

bool SamplePlugin::Hook_LevelInit(char const *pMapName, char const *pMapEntities,
                                  char const *pOldLevel, char const *pLandmarkName,
                                  bool loadGame, bool background)
{
    // Bao cao freegate cua MAN VUA XONG truoc khi dat lai bo dem - de doc log
    // tren may chu that biet tung map giu cach ly bao nhieu.
    if (g_Stage != 0 && g_PatchFreeGate == 1 && (g_FreeReleased || g_FreeQuarantined)) {
        FreeGateReport("het map");
        g_FreeReleased = g_FreeQuarantined = 0;
        g_FkNames = 0; g_FreeKeepLogged = 0; g_SameTickPeak = 0;
    }

// Dat lai bo dem SWAP tai DAY, khong phai o SwapReport(). docs/01-co-che.md
    if (g_Stage != 0 && g_Swap > 0) {
        for (int i = 0; i < g_SwapCount; i++) { g_SwapSeen[i] = 0; g_SwapHits[i] = 0; }
        g_SwapDone = 0;
        g_SwapMapName = pMapName ? pMapName : "?";
    }

    // ---- NONETKILL: sua entity lump TRUOC KHI server.dll parse ----
    // Phai dat TRUOC chot g_BigArrayOn ben duoi - viec nay khong lien quan gi
    // toi huong 4096, va g_BigArrayOn dang la 0.
    if (g_Stage != 0 && g_NoNetKill && pMapEntities) {
        if (g_KillCount == 0) LoadKillList();

        int hits = 0;
        const char* fixed = RewriteLump(pMapEntities, &hits);
        if (fixed) {
            if (g_LumpCopy) free(g_LumpCopy);      // giai phong ban cua map truoc
            g_LumpCopy = (char*)fixed;
            EL_LOG("[EdictBudget] NONETKILL: map '%s' - da doi ten %d entity "
                     "(chuoi lump %u byte, do dai GIU NGUYEN)",
                     pMapName ? pMapName : "?", hits, (unsigned)strlen(g_LumpCopy));
            RETURN_META_VALUE_NEWPARAMS(MRES_IGNORED, true,
                &IServerGameDLL::LevelInit,
                (pMapName, g_LumpCopy, pOldLevel, pLandmarkName, loadGame, background));
        }
        EL_LOG("[EdictBudget] NONETKILL: map '%s' - khong co entity nao khop, "
                 "khong doi gi", pMapName ? pMapName : "?");
    }

    // Chu y dieu kien: viec GHIM la mon no phai tra moi khi mang duoc noi rong,
    // ke ca khi ban than phan tach dang tat.
    if (g_Stage == 0 || !g_BigArrayOn) return true;

    // Den luc nay server.dll da duoc anh xa day du, khac voi luc Load().
    if (g_PatchDetour && !g_OrigCreateEntity) InstallDetour();
    if (!g_SMListHead) ResolveSMListHead();

    AuditReset();
    CensusReset();
    g_Tripped = false;
    g_WarnedNum = g_WarnedMax = g_WarnedGlob = false;
    g_MinFreeSeen = 999999;
    g_EdgeLines   = 0;
    g_MaxBurst    = 0;

    // Lam thay phan xoa m_pPackedData moi map cua engine. Ta KHONG con sua
    // CFrameSnapshotManager::LevelChanged nua, vi mot extension cua ben thu ba
    // moc vao dung ham do va cac byte ta sua khien no khong nap duoc.
    // Serial number thi CO Y de nguyen - engine cung chua bao gio xoa chung, boi
    // mot serial chi duoc doc khi con tro packed cua no khac NULL.
    if (g_PackedTable) memset(g_PackedTable, 0, EXT_LIMIT * sizeof(uint32_t));
    g_Cursor = EXT_LIMIT - 1;
    g_ExtReady = false;

// Danh dau nua tren la TRONG. FL_EDICT_FREE la bit BAT nen o chua khoi tao
// doc ra thanh DANG CHIEM DUNG. Xem docs/03-huong-4096.md
    if (g_MarkFree && gpGlobals && gpGlobals->pEdicts && *g_max_edicts >= EXT_LIMIT) {
        uint8_t* arr = (uint8_t*)gpGlobals->pEdicts;
        for (int i = NET_LIMIT; i < EXT_LIMIT; i++) {
            memset(arr + i * EDICT_SIZE, 0, EDICT_SIZE);
            *(uint32_t*)(arr + i * EDICT_SIZE) = FL_FREE;
        }
        // Gio cac o da AN TOAN de nhin vao, nhung chi DUNG DUOC khi bang snapshot
        // cung da duoc doi cho.
        g_ExtReady = g_EngineArray4096;
    }

    // Giu CA HAI gioi han o gia tri goc. Mang van la 4096 muc, nhung cac vong lap
    // cua engine, server.dll va SourceMod deu nhin thay dung 2048 - va do la con
    // so ma tat ca bon ho duoc xay dung de lam viec cung.
    // Bao cao xem engine THUC SU dang co gi TRUOC khi dong vao bat cu thu gi. Viec
    // ep maxEntities ve 2048 la mot GIA DINH, chua bao gio la mot PHEP DO, va no
    // la cau lenh duy nhat chi chay khi bigarray dang bat - dung cai nhom lam hong
    // vong hoi sinh luc wipe.
    EL_LOG("[EdictBudget] before pin: sv.max_edicts=%d sv.num_edicts=%d "
             "gpGlobals->maxEntities=%d",
             (int)*g_max_edicts, g_num_edicts ? (int)*g_num_edicts : -1,
             gpGlobals ? gpGlobals->maxEntities : -1);

    if (g_PinMax)     *g_max_edicts = NET_LIMIT;
    if (g_PinGlobals && gpGlobals) gpGlobals->maxEntities = NET_LIMIT;

    EL_LOG("[EdictBudget] LevelInit(%s): array=%p size=%d, limits held at %d, extension %s",
        pMapName, gpGlobals ? gpGlobals->pEdicts : NULL, EXT_LIMIT, NET_LIMIT,
        g_ExtReady ? "ready" : "UNAVAILABLE");
    return true;
}

// Bo canh cho DUY NHAT mot tinh huong giet tien trinh ma khong de lai dump:
// TakeTickSnapshot ghi tran stack cookie khi num_edicts vuot 2048.
// Xem docs/03-huong-4096.md
static bool g_WarnedSM = false;
// (khai bao o dau file)

static void ResolveSMListHead() {
    HMODULE h = GetModuleHandle("sourcemod.2.l4d2.dll");
    if (!h) return;
    g_SMListHead = (uint32_t*)((uint8_t*)h + 0xAADFC);
    EL_LOG("[EdictBudget] watching SourceMod PlayerManager::m_hooks head at %p",
             g_SMListHead);
}

static bool Readable(const void* p, size_t n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return (uint8_t*)p + n <= (uint8_t*)mbi.BaseAddress + mbi.RegionSize;
}

static void CheckSMList() {
    if (g_WarnedSM || !g_SMListHead) return;
    if (!Readable(g_SMListHead, 4)) return;

    uint32_t head = *g_SMListHead;
    const char* why = NULL;

    if (head == 0)                            why = "head is NULL";
    else if (!Readable((void*)head, 12))      why = "head points at unreadable memory";
    else {
        uint32_t next = ((uint32_t*)head)[1];
        uint32_t prev = ((uint32_t*)head)[2];
        if (!Readable((void*)next, 12))       why = "head->next unreadable";
        else if (!Readable((void*)prev, 12))  why = "head->prev unreadable";
        else if (((uint32_t*)prev)[1] != head) why = "ring broken: prev->next is not head";
    }

    if (why) {
        g_WarnedSM = true;
        EL_LOG("[EdictBudget] *** SourceMod m_hooks list CORRUPTED (%s), head=%08X - "
                 "the server will die in PlayerManager::MaxPlayersChanged the next time "
                 "maxplayers changes ***", why, head);
    }
}

// Nha thoi gian cho tai dung, NHUNG chi khi dai co mang thuc su dang bi ep, va
// nhieu lam vai lan mot giay. Goi vo dieu kien moi frame se lam tai dung chi so
// ngay khoanh khac bat cu thu gi chet, va lam ngap client bang cac lenh xoa tuong
// minh ma khong duoc loi gi.
static int g_LastReuseTick = -1000;

// (khai bao o dau file)

static void ReleaseReuseCooldown()
{
    if (!g_ImmediateReuse || !engine || !g_num_edicts) return;
    if (*g_num_edicts < NET_LIMIT - 192) return;          // con rat nhieu cho

    // Tiet che thi khong sao khi con cho, nhung chinh no da CHE GIAU cu hong: moi
    // lan lay mau deu thay 844+ o trong, roi may chu chet GIUA hai lan lay mau.
    // Error() ben trong ED_Alloc khong bao gio tra ve, nen mot POST hook cung khong
    // the nao quan sat duoc loi goi that bai - cho duy nhat con lai de nhin la NGAY
    // TRUOC moi lan cap phat, va bo tiet che khi da gan mep.
    int tick = gpGlobals ? gpGlobals->tickcount : 0;
    bool nearEdge = (g_MinFreeSeen < 64);
    if (!nearEdge) {
        if (tick - g_LastReuseTick < 15) return;
        g_LastReuseTick = tick;
    }

// Phep do truoc do MAU THUAN voi ma may: 2048 edict ma 880 o trong van bao het.
// Loi giai: vong quet khong he chay. Xem docs/05-do-dac.md
    uint8_t* svObj = (uint8_t*)g_num_edicts - 0x214;
    int engMaxClients = *(int*)(svObj + 0x104);
    int toolzSlots    = *(int*)(svObj + 0x180);

    int scanStart = engMaxClients + 1;
    int n = (int)*g_num_edicts;
    if (n > NET_LIMIT) n = NET_LIMIT;

    // Dem qua sv.edicts (0x10645774) - dung con tro ma chinh ED_Alloc duyet - chu
    // KHONG phai gpGlobals->pEdicts. Hai cai duoc GIA DINH la bang nhau vi
    // SV_AllocateEdicts gan ca hai, nhung dieu do chua bao gio duoc xac minh luc
    // chay; va neu chung khac nhau thi moi con so "o trong" do duoc hom nay deu
    // dang dem tren SAI mang.
    uint8_t* svEdicts  = g_edicts ? (uint8_t*)*g_edicts : NULL;
    uint8_t* glbEdicts = gpGlobals ? (uint8_t*)gpGlobals->pEdicts : NULL;

    int freeBelow = 0, freeInWindow = 0;
    if (svEdicts) {
        uint8_t* arr = svEdicts;
        for (int i = 0; i < n; i++) {
            if (!(*(uint32_t*)(arr + i * EDICT_SIZE) & FL_FREE)) continue;
            if (i < scanStart) freeBelow++; else freeInWindow++;
        }
    }

    if (freeInWindow < 64) g_MinFreeSeen = freeInWindow;   // arm unthrottled mode

    if (freeInWindow < g_MinFreeSeen || (freeInWindow < 64 && g_EdgeLines++ < 60)) {
        if (freeInWindow < g_MinFreeSeen) g_MinFreeSeen = freeInWindow;
        EL_LOG("[EdictBudget] pressure: num_edicts=%d | sv[0x104]=%d -> engine quet %d..%d | "
                 "TRONG trong cua so=%d, TRONG duoi cua so=%d | sv.edicts=%p gpGlobals->pEdicts=%p%s",
                 (int)*g_num_edicts, engMaxClients, scanStart, n - 1,
                 freeInWindow, freeBelow, svEdicts, glbEdicts,
                 (svEdicts != glbEdicts) ? "  <<< HAI CON TRO KHAC NHAU!" : "");
    }

    engine->AllowImmediateEdictReuse();
}

// Bat DUNG khoanh khac ED_Alloc bo cuoc bang mot POST hook tren CreateEdict.
// Xem docs/05-do-dac.md
static int g_NullReports = 0;

edict_t* Hook_CreateEdict_Post(int forceIndex)
{
    edict_t* ret = META_RESULT_ORIG_RET(edict_t*);
    if (ret == NULL && g_NullReports < 8 && g_num_edicts && g_max_edicts) {
        g_NullReports++;

        uint8_t* svObj = (uint8_t*)g_num_edicts - 0x214;
        int maxClients = *(int*)(svObj + 0x104);
        int scanStart  = maxClients + 1;
        int n = (int)*g_num_edicts;
        if (n > EXT_LIMIT) n = EXT_LIMIT;

        int freeWin = 0, freeLow = 0, freeHigh = 0;
        if (gpGlobals && gpGlobals->pEdicts) {
            uint8_t* arr = (uint8_t*)gpGlobals->pEdicts;
            for (int i = 0; i < n; i++) {
                if (!(*(uint32_t*)(arr + i * EDICT_SIZE) & FL_FREE)) continue;
                if (i < scanStart)      freeLow++;
                else if (i < NET_LIMIT) freeWin++;
                else                    freeHigh++;
            }
        }
        EL_LOG("[EdictBudget] *** CreateEdict(%d) TRA VE NULL *** "
                 "num_edicts=%d max_edicts=%d maxClients=%d | TRONG: duoi=%d trong-cua-so=%d tren2047=%d",
                 forceIndex, (int)*g_num_edicts, (int)*g_max_edicts, maxClients,
                 freeLow, freeWin, freeHigh);
    }
    RETURN_META_VALUE(MRES_IGNORED, ret);
}

// HEARTBEAT - ghi dinh ky so lieu thuc the vao log. Chi ghi, khong dong vao
// entity nao. Xem docs/05-do-dac.md
#define HB_MAX 96
static char  g_HbName[HB_MAX][40];
static int   g_HbCount[HB_MAX];
static int   g_HbN       = 0;
static bool  g_HbHasPrev = false;
static float g_HbNext    = 0.0f;

static void HeartbeatSample() {
    if (!g_NextEnt || !g_EntList) return;

    char  name[HB_MAX][40];
    int   cnt[HB_MAX];
    int   n = 0, live = 0;

    void* e = g_NextEnt(g_EntList, NULL, NULL);
    while (e) {
        live++;
        const char* cls = *(const char**)((uint8_t*)e + 0x74);
        if (cls) {
            int i = 0;
            for (; i < n; i++) if (strcmp(name[i], cls) == 0) { cnt[i]++; break; }
            if (i == n && n < HB_MAX) {
                strncpy(name[n], cls, sizeof(name[0])-1);
                name[n][sizeof(name[0])-1] = 0;
                cnt[n] = 1; n++;
            }
        }
        e = g_NextEnt(g_EntList, NULL, e);
    }

    int num  = g_num_edicts ? (int)*g_num_edicts : -1;
    int free = CountFreeEdicts();
    EL_LOG("[EdictBudget] NHIP: song=%d num_edicts=%d trong=%d bien do=%d lop=%d",
             live, num, free, NET_LIMIT - live, n);

    if (g_HbHasPrev) {
        // chenh lech theo lop, chi in cai co thay doi
        char dn[HB_MAX][40];
        int  dv[HB_MAX];
        int  dn_n = 0;
        for (int i = 0; i < n && dn_n < HB_MAX; i++) {
            int old = 0;
            for (int j = 0; j < g_HbN; j++)
                if (strcmp(g_HbName[j], name[i]) == 0) { old = g_HbCount[j]; break; }
            if (cnt[i] != old) {
                strncpy(dn[dn_n], name[i], sizeof(dn[0])-1);
                dn[dn_n][sizeof(dn[0])-1] = 0;
                dv[dn_n] = cnt[i] - old; dn_n++;
            }
        }
        for (int j = 0; j < g_HbN && dn_n < HB_MAX; j++) {
            bool still = false;
            for (int i = 0; i < n; i++) if (strcmp(name[i], g_HbName[j]) == 0) { still = true; break; }
            if (!still) {
                strncpy(dn[dn_n], g_HbName[j], sizeof(dn[0])-1);
                dn[dn_n][sizeof(dn[0])-1] = 0;
                dv[dn_n] = -g_HbCount[j]; dn_n++;
            }
        }
        for (int a = 0; a < dn_n && a < 12; a++) {
            int best = a;
            for (int b = a + 1; b < dn_n; b++) if (dv[b] > dv[best]) best = b;
            if (best != a) {
                int t = dv[a]; dv[a] = dv[best]; dv[best] = t;
                char tn[40]; strncpy(tn, dn[a], sizeof(tn)); tn[sizeof(tn)-1]=0;
                strncpy(dn[a], dn[best], sizeof(dn[0])); dn[a][sizeof(dn[0])-1]=0;
                strncpy(dn[best], tn, sizeof(dn[0])); dn[best][sizeof(dn[0])-1]=0;
            }
            if (dv[a] == 0) break;
            EL_LOG("[EdictBudget] NHIP:   %+5d  %s", dv[a], dn[a]);
        }
    }

    for (int i = 0; i < n; i++) {
        strncpy(g_HbName[i], name[i], sizeof(g_HbName[0])-1);
        g_HbName[i][sizeof(g_HbName[0])-1] = 0;
        g_HbCount[i] = cnt[i];
    }
    g_HbN = n; g_HbHasPrev = true;
}

void SamplePlugin::Hook_GameFrame(bool simulating)
{
    if (g_Stage == 0) RETURN_META(MRES_IGNORED);
    CheckSMList();
    ReleaseReuseCooldown();

    // Lay mau TUNG FRAME ngay sau khi nap map. Xem khoi giai thich o g_LoadProbe.
    // Doc hai so: num_edicts (moc cao nhat, KHONG BAO GIO GIAM) va so slot trong.
    //   song = num_edicts - trong
    // num_edicts tang ma trong cung tang  => co cap phat roi tra lai (dinh tam thoi)
    // num_edicts tang ma trong dung yen   => entity that su duoc sinh them
    if (g_LoadProbeLeft > 0) {
        int ne   = g_num_edicts ? (int)*g_num_edicts : -1;
        int free = CountFreeEdicts();
        if (ne > g_LoadProbePeak) g_LoadProbePeak = ne;
        EL_LOG("[EdictBudget] NAP[frame %d]: num_edicts=%d trong=%d song=%d (dinh=%d)",
                 g_LoadProbeFrame, ne, free, ne - free, g_LoadProbePeak);
        g_LoadProbeFrame++;
        if (--g_LoadProbeLeft == 0)
            EL_LOG("[EdictBudget] NAP: het %d frame lay mau, dinh num_edicts=%d",
                     g_LoadProbeFrame, g_LoadProbePeak);
    }

    if (g_Heartbeat > 0 && gpGlobals) {
        float now = gpGlobals->curtime;
        if (g_HbNext <= 0.0f) g_HbNext = now + (float)g_Heartbeat;
        else if (now >= g_HbNext) { g_HbNext = now + (float)g_Heartbeat; HeartbeatSample();
                                    FreeGateReport("nhip"); }
    }

    if (g_AllocThisFrame > g_MaxBurst) {
        g_MaxBurst = g_AllocThisFrame;
        if (g_MaxBurst >= 32) {
            EL_LOG("[EdictBudget] BURST: %d lan cap phat trong mot frame "
                     "(num_edicts=%d)", g_MaxBurst,
                     g_num_edicts ? (int)*g_num_edicts : -1);
        }
    }
    g_AllocThisFrame = 0;
    if (!g_BigArrayOn) RETURN_META(MRES_IGNORED);

    if (!g_WarnedNum && g_num_edicts && *g_num_edicts > NET_LIMIT) {
        g_WarnedNum = true;
        EL_LOG("[EdictBudget] *** num_edicts=%d exceeded %d - the engine's "
                 "snapshot list holds only %d entries, so the next snapshot will "
                 "overwrite the stack cookie and kill the process with no dump ***",
                 (int)*g_num_edicts, NET_LIMIT, NET_LIMIT);
    }
    if (!g_WarnedMax && g_max_edicts && *g_max_edicts != NET_LIMIT) {
        g_WarnedMax = true;
        EL_LOG("[EdictBudget] *** max_edicts=%d outside a frame hook "
                 "(expected %d) - num_edicts is free to grow past the limit ***",
                 (int)*g_max_edicts, NET_LIMIT);
    }
    // LevelInit ghim gia tri nay MOT LAN moi map. Neu ve sau engine ghi lai no,
    // SourceMod va moi extension se bat dau nhin thay 4096 - ma viec ghim thi chua
    // bao gio duoc xac minh la giu duoc qua khoi dung khoanh khac do.
    if (!g_WarnedGlob && g_PinGlobals && gpGlobals && gpGlobals->maxEntities != NET_LIMIT) {
        g_WarnedGlob = true;
        EL_LOG("[EdictBudget] *** gpGlobals->maxEntities=%d during a frame "
                 "(pinned to %d at LevelInit) - the engine reset it ***",
                 gpGlobals->maxEntities, NET_LIMIT);
    }
    RETURN_META(MRES_IGNORED);
}

void SamplePlugin::Hook_ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
    if (g_Stage != 0 && g_BigArrayOn) {
        EL_LOG("[EdictBudget] ServerActivate: num_edicts=%d (networked budget %d)",
            g_num_edicts ? (int)*g_num_edicts : -1, NET_LIMIT - 1);
        AuditDump();
        CensusDump();
    }

    // Duong co so cho phep do WIPECLEAR: dem thuc the theo lop NGAY SAU khi nap
    // map, truoc bat ky lan wipe nao. So nay la moc de so voi bang kiem ke lucA
    // chet. Chay ca khi wipeclear=0 - no chi doc, khong dong gi.
    if (g_Stage != 0) {
        // Map moi => xoa co thua con sot lai, tranh don nham o vong choi dau tien.
        g_LossPending = false;
        EL_LOG("[EdictBudget] MOC CO SO (sau nap map, chua wipe lan nao): "
                 "num_edicts=%d slot trong=%d",
                 g_num_edicts ? (int)*g_num_edicts : -1, CountFreeEdicts());
        if (g_NoEdict)
            EL_LOG("[EdictBudget] NOEDICT: %d entity da duoc dat EFL_SERVER_ONLY "
                     "(khong ton edict) tren %d vtable", g_NoEdictHits, g_PatchedVtCount);
        SwapReport();       // bao cao sau khi lump da phan tich xong
        // Bat dau lay mau tung frame. Nhieu entity chua spawn xong o thoi diem nay.
        g_LoadProbeLeft  = g_LoadProbe;
        g_LoadProbeFrame = 0;
        g_LoadProbePeak  = g_num_edicts ? (int)*g_num_edicts : 0;
        // g_pGameRules chi ton tai sau khi level da chay => cai o day, khong o Load()
        InstallWipeClear();
        InstallMapClear();
    }
    RETURN_META(MRES_IGNORED);
}

void SamplePlugin::OnVSPListening(IServerPluginCallbacks *iface) {}
void SamplePlugin::AllPluginsLoaded() {}
bool SamplePlugin::Pause(char *error, size_t maxlen) { return true; }
bool SamplePlugin::Unpause(char *error, size_t maxlen) { return true; }
const char *SamplePlugin::GetLicense() { return "GPLv3"; }
const char *SamplePlugin::GetVersion() { return "2.0"; }
const char *SamplePlugin::GetDate() { return __DATE__; }
const char *SamplePlugin::GetLogTag() { return "EDICTBUDGET"; }
// Toan bo ma nguon nay do Claude (Anthropic) viet. Xem khoi TAC GIA o dau file.
const char *SamplePlugin::GetAuthor() { return "Claude (Anthropic) - AI"; }
const char *SamplePlugin::GetDescription() { return "Giu so entity dang song duoi tran 2048 edict"; }
const char *SamplePlugin::GetName() { return "EdictBudget"; }
const char *SamplePlugin::GetURL() { return "http://www.sourcemm.net/"; }

uint8_t* FindPattern(const char* module, const char* pattern, const char* mask) {
    HMODULE h = GetModuleHandle(module);
    if (!h) return nullptr;
    MODULEINFO mi;
    GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi));
    uint8_t* base = (uint8_t*)mi.lpBaseOfDll;
    size_t size = mi.SizeOfImage;
    size_t len = strlen(mask);
    for (size_t i = 0; i + len < size; i++) {
        bool ok = true;
        for (size_t j = 0; j < len; j++) {
            if (mask[j] == 'x' && base[i + j] != (uint8_t)pattern[j]) { ok = false; break; }
        }
        if (ok) return base + i;
    }
    return nullptr;
}
