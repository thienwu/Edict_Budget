// ===========================================================================
//  edictbudget - giu so entity DANG SONG duoi tran 2048 edict cho L4D2.
//
//  Y tuong, bai toan va dinh huong: thienwu - nguoi VAN HANH may chu that.
//  Ho quyet dinh moi huong di lon, va ca nhung huong KHONG duoc di (cam dong
//  vao ho phys, cam sua BSP, yeu cau CONG THUC CHUNG tu kiem luc chay). Ho chay
//  thu, chup log, do tren may chu that, va bac bo nhieu ket luan sai cua AI -
//  cac dong ghi "SAI, da sua" la dau vet cua nhung lan do.
//
//  Dich nguoc, viet ma, do dac va tai lieu: Claude (Anthropic), trong Claude
//  Code. Chi tiet day du o NOTICE.
//
//  Giay phep: GPLv3. Xem LICENSE.
//
//  ------------------------------------------------------------------------
//  MA NGUON: file nay la MODULE CHINH - chi giu vong doi plugin (Load, Unload,
//  LevelInit, GameFrame, ServerActivate) va goi DIEM VAO cua tung tinh nang.
//  Moi tinh nang mot file src/<ten>.cpp, diem vao khai bao o src/giaodien.h,
//  cong tac doc o src/congtac.cpp (patches.txt). Van la MOT don vi bien dich
//  (file nay #include src/*.cpp) nen build.bat khong doi.
//
//    noedict.cpp  freegate.cpp  wipeclear.cpp  mapclear.cpp  swap.cpp
//    spotlight.cpp (v2.1.0)  baycapphat.cpp (trap)  doluong.cpp (heartbeat,
//    loadprobe)  nonetkill.cpp (DA LOAI BO, giu de doc)
//
//  ------------------------------------------------------------------------
//  BAN DO MOC - module nao moc vao dau. Hai cho co HAI module cung moc (dau *)
//  la noi de nham nhat khi sua: doc ca hai module truoc khi dong vao.
//
//    server.dll
//    * PostConstructor (vtable slot 29 cua TUNG lop)
//        noedict    moi lop trong noedict.txt   - theo VTABLE (ca lop)
//        spotlight  point_spotlight, spotlight_end - theo TUNG entity (co mot lan)
//        Ca hai cung ghi EFL_SERVER_ONLY (m_iEFlags +0x138, bit 9). Khong giam
//        nhau: ben nao thay slot 29 khong con tro PostConstructor goc thi tu lui.
//        noedict cai TRUOC => noedict.txt co point_spotlight/spotlight_end thi
//        spotlight tu tat ca module (ghi ly do vao log).
//    * CEntityFactoryDictionary::Create (slot 1 cua dictionary)
//        swap       doi lop theo swap.txt (cai truoc)
//        spotlight  noi chuoi BEN NGOAI swap (cai sau). point_spotlight DOC TU
//                   MAP ma spotlight da quyet -> goi thang Create goc, bo qua swap;
//                   point_spotlight TAO LUC CHAY (plugin, script) -> van qua swap.
//      MapEntity_ParseEntity (2 lenh call)           spotlight
//      CPointSpotlight::SpotlightCreate (3 lenh call) spotlight
//      CTerrorGameRules vtable slot 178 RestartRound  wipeclear
//      CTerrorGameRules vtable slot 38 (chuyen man)   mapclear
//    engine.dll
//      IVEngineServer::RemoveEdict (slot 23)          freegate=1
//      ED_Alloc: 1 byte jae -> jmp                    freegate=2
//      ED_Alloc: nhanh bao loi -> stub ghi kiem ke    trap
//    LevelInit (chuoi entity lump)                    nonetkill (DA LOAI BO)
//
//  wipeclear va mapclear CUNG MOT KHUNG (duyet moi entity -> bo qua preserve
//  list + file giu rieng -> UTIL_Remove -> CleanupDeleteList) nhung chay o hai
//  su kien khac nhau (thua / chuyen man), moi ben mot file giu rieng
//  (wipekeep.txt / mapkeep.txt). Khong bao gio chay cung luc.
//
//  DOAN MA LAP LAI - CHUA GOP. Luc chia module (05/10/2026) giu nguyen tung ham
//  de so tung ham ban cu/ban moi ra "giong het". Sua MOT ban thi phai kiem cac
//  ban con lai (da co mot ban chep sai: mapclear doc mapkeep.txt, xem do):
//    doc file danh sach lop     noedict, wipeclear, freegate, mapclear (x2),
//                               nonetkill   (swap.txt doc cap "a -> b", khac khuon)
//    khop "ten_" = tien to      freegate, wipeclear, mapclear (x2)
//    bang dem theo classname    freegate, mapclear, doluong (heartbeat), baycapphat
//    dem slot trong             CountFreeEdicts (congcu_entity) + ban chep o baycapphat
//    duyet entity (NextEnt)     wipeclear, mapclear, doluong
//    lay base server.dll        noedict, wipeclear, mapclear, swap, spotlight
//    dictionary (base+0x20CA70) noedict (ResolveClassVtable), swap, spotlight
//    g_pGameRules (base+0x7F7F6C) wipeclear, mapclear
//    ghi vtable                 WriteProtected (tienich); wipeclear, mapclear
//                               tu goi VirtualProtect - tuong duong
//    doc chuoi entity lump      nonetkill (ca lump), spotlight (tung khoi)
//
//  ------------------------------------------------------------------------
//  TAI LIEU DAY DU nam trong docs/ - khong lap lai o day:
//
//    docs/01-co-che.md       cac co che: noedict, swap, spotlight, wipeclear +
//                            wipekeep.txt, freegate, mapclear
//    docs/02-noedict.md      noedict chi tiet: dieu kien, cach them va kiem mot
//                            lop, plugin SourceMod
//    docs/03-loi-da-biet.md  loi da biet
//    docs/04-dia-chi.md      !! BANG DIA CHI server.dll / engine.dll (ImageBase
//                            0x10000000, ban game ghi trong file) - kiem lai
//                            dia chi o day sau moi lan Valve cap nhat game
//    docs/05-huong-da-bo.md  cac huong da bo va vi sao: nonetkill, mapclear=2,
//                            phys, env_sprite ...
//    docs/06-do-dac.md       do dac: log, loadprobe, heartbeat, trap, ent_test
//    docs/07-du-lieu.md      du lieu phan loai 557 lop entity
//
//  Ket luan lay tu dich nguoc deu kem dia chi de kiem lai duoc.
//  Cai gi khong xac minh duoc thi ghi thang la KHONG XAC DINH.
//
//  ------------------------------------------------------------------------
//  TINH NANG MOI v2.1.0 (09/10/2026), MAC DINH TAT. Da do tren may chu CUC BO
//  (9 map, docs/06-do-dac.md), CHUA chay may chu dong nguoi:
//
//    src/spotlight.cpp       patches.txt: spotlight=0/1/2  (MAC DINH 0 = tat;
//                            1 = chi ghi log quyet dinh, 2 = lam that)
//                            Quyet THEO TUNG point_spotlight luc map doc khoi
//                            keyvalue cua no (MapEntity_ParseEntity):
//                              HaloScale = 60          -> beam_spotlight
//                              HaloScale != 60, ko cha -> giu lop Valve; no va
//                                    spotlight_end cua no KHONG edict
//                              HaloScale != 60, co cha -> giu nguyen
//                            Tiet kiem edict nhu swap.txt (point_spotlight ->
//                            beam_spotlight) nhung giu dung HaloScale (client
//                            ghi cung halo 60 cho beam_spotlight). Khong sua
//                            module nao khac; de 0 la chay y nhu truoc.
//                            Chi tiet: docs/01-co-che.md muc spotlight;
//                            dia chi: docs/04-dia-chi.md; so do: docs/06-do-dac.md
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
SH_DECL_HOOK1_void(IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool);

PLUGIN_EXPOSE(SamplePlugin, g_SamplePlugin);

// --------------------------------------------------------------------------
// MODULE CHINH - dieu khien. File nay chi giu vong doi plugin (Load/Unload/
// LevelInit/GameFrame/ServerActivate) va goi DIEM VAO cua tung module tinh
// nang trong src/ (danh sach o src/giaodien.h). Code cua tinh nang KHONG viet
// o day. Thu tu #include la thu tu phu thuoc: module sau duoc dung module truoc.
// --------------------------------------------------------------------------
#include "src/giaodien.h"
#include "src/tienich.cpp"        // ghi vung nho, mo file, tim chu ky
#include "src/nhatky.cpp"         // edictbudget.log
#include "src/loi.cpp"            // hang so, bien engine, stage
#include "src/congcu_entity.cpp"  // ham server.dll dung chung de duyet/go entity
#include "src/freegate.cpp"
#include "src/baycapphat.cpp"
#include "src/nonetkill.cpp"
#include "src/wipeclear.cpp"
#include "src/mapclear.cpp"
#include "src/noedict.cpp"
#include "src/swap.cpp"
#include "src/spotlight.cpp"     // dung ResolveClassVtable cua noedict.cpp; TU moc dictionary rieng
#include "src/doluong.cpp"
#include "src/congtac.cpp"        // doc patches.txt - SAU moi module

// ==========================================================================
// Vong doi plugin
// ==========================================================================
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
    // Dong trang thai - PHAI o day, ngoai moi nhanh dieu kien.
    EL_LOG("[EdictBudget] noedict=%d mapclear=%d mapclearmax=%d mapclearcarry=%d "
             "heartbeat=%d loadprobe=%d swap=%d swapmax=%d spotlight=%d logconsole=%d",
             g_NoEdict ? 1 : 0, g_MapClear, g_MapClearMax, g_MapClearCarryOnly,
             g_Heartbeat, g_LoadProbe, g_Swap, g_SwapMax, g_Spotlight, g_LogConsole ? 1 : 0);

    FreeGate_OnLoad();
    BayCapPhat_OnLoad();
    NoEdict_OnLoad();
    Swap_OnLoad();
    Spotlight_OnLoad();             // SAU Swap_OnLoad: moc dictionary noi chuoi ben ngoai moc cua swap
    return true;
}

bool SamplePlugin::Unload(char *error, size_t maxlen)
{
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, LevelInit, server, this, &SamplePlugin::Hook_LevelInit, false);
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, ServerActivate, server, this, &SamplePlugin::Hook_ServerActivate, false);
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameFrame, server, this, &SamplePlugin::Hook_GameFrame, false);
    WipeClear_OnUnload();
    MapClear_OnUnload();
    // Dong log O DAY: cac module go SAU dong nay KHONG duoc goi EL_LOG - goi la
    // EL_LOG mo lai edictbudget.log va khong ai dong nua.
    EL_LOG_CLOSE();
    NoEdict_OnUnload();
    Spotlight_OnUnload();           // TRUOC Swap_OnUnload
    Swap_OnUnload();
    return true;
}

bool SamplePlugin::Hook_LevelInit(char const *pMapName, char const *pMapEntities,
                                  char const *pOldLevel, char const *pLandmarkName,
                                  bool loadGame, bool background)
{
    if (g_Stage != 0) {
        FreeGate_OnLevelInit();
        Swap_OnLevelInit(pMapName);
        Spotlight_OnLevelInit(pMapName);
        const char* lump = NoNetKill_OnLevelInit(pMapName, pMapEntities);
        if (lump)
            RETURN_META_VALUE_NEWPARAMS(MRES_IGNORED, true,
                &IServerGameDLL::LevelInit,
                (pMapName, lump, pOldLevel, pLandmarkName, loadGame, background));
    }
    return true;
}

void SamplePlugin::Hook_GameFrame(bool simulating)
{
    if (g_Stage == 0) RETURN_META(MRES_IGNORED);
    LoadProbe_OnGameFrame();
    if (Heartbeat_OnGameFrame()) FreeGate_OnHeartbeat();
    RETURN_META(MRES_IGNORED);
}

void SamplePlugin::Hook_ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
    if (g_Stage != 0) {
        DoLuong_OnServerActivate();
        NoEdict_OnServerActivate();
        Swap_OnServerActivate();
        Spotlight_OnServerActivate();
        WipeClear_OnServerActivate();
        MapClear_OnServerActivate();
    }
    RETURN_META(MRES_IGNORED);
}

void SamplePlugin::OnVSPListening(IServerPluginCallbacks *iface) {}
void SamplePlugin::AllPluginsLoaded() {}
bool SamplePlugin::Pause(char *error, size_t maxlen) { return true; }
bool SamplePlugin::Unpause(char *error, size_t maxlen) { return true; }
const char *SamplePlugin::GetLicense() { return "GPLv3"; }
const char *SamplePlugin::GetVersion() { return "2.1.0"; }
const char *SamplePlugin::GetDate() { return __DATE__; }
const char *SamplePlugin::GetLogTag() { return "EDICTBUDGET"; }
// Toan bo ma nguon nay do Claude (Anthropic) viet. Xem khoi TAC GIA o dau file.
const char *SamplePlugin::GetAuthor() { return "Claude (Anthropic) - AI"; }
const char *SamplePlugin::GetDescription() { return "Giu so entity dang song duoi tran 2048 edict"; }
const char *SamplePlugin::GetName() { return "EdictBudget"; }
const char *SamplePlugin::GetURL() { return "http://www.sourcemm.net/"; }
