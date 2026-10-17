// ==========================================================================
//  giaodien - DIEM VAO cua cac module tinh nang. Module chinh (sample_mm.cpp)
//  CHI goi qua cac ham nay; thu tu goi va dieu kien stage nam o module chinh.
//  Them tinh nang moi: viet src/<ten>.cpp, khai bao diem vao o day, them
//  #include va loi goi trong sample_mm.cpp.
// ==========================================================================

// freegate.cpp   - patches.txt: freegate=0/1/2
static void FreeGate_OnLoad();
static void FreeGate_OnLevelInit();
static void FreeGate_OnHeartbeat();

// baycapphat.cpp - patches.txt: trap=0/1
static void BayCapPhat_OnLoad();

// nonetkill.cpp  - patches.txt: nonetkill=0/1 (DA LOAI BO)
static const char* NoNetKill_OnLevelInit(const char* pMapName, const char* pMapEntities);

// wipeclear.cpp  - patches.txt: wipeclear=0/1/2
static void WipeClear_OnServerActivate();
static void WipeClear_OnUnload();

// mapclear.cpp   - patches.txt: mapclear=0/1/2, mapclearmax, mapclearcarry
static void MapClear_OnServerActivate();
static void MapClear_OnUnload();

// noedict.cpp    - patches.txt: noedict=0/1
static void NoEdict_OnLoad();
static void NoEdict_OnServerActivate();
static void NoEdict_OnUnload();

// swap.cpp       - patches.txt: swap=0/1/2, swapmax
static void Swap_OnLoad();
static void Swap_OnLevelInit(const char* pMapName);
static void Swap_OnServerActivate();
static void Swap_OnUnload();

// spotlight.cpp  - patches.txt: spotlight=0/1/2 (quyet theo tung point_spotlight)
static void Spotlight_OnLoad();
static void Spotlight_OnLevelInit(const char* pMapName);
static void Spotlight_OnServerActivate();
static void Spotlight_OnUnload();

// hopbao.cpp     - patches.txt: hopbao=0/1 (va MarkSurroundingBoundsDirty cho thuc the server-only)
static void HopBao_OnLoad();
static void HopBao_OnServerActivate();
static void HopBao_OnUnload();

// doluong.cpp    - patches.txt: loadprobe=N, heartbeat=giay
static void DoLuong_OnServerActivate();
static void LoadProbe_OnGameFrame();
static bool Heartbeat_OnGameFrame();
