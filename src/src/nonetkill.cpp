// ==========================================================================
//  nonetkill - DA LOAI BO. Xem docs/05-huong-da-bo.md (muc nonetkill)
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// Hai cong tac xu ly thuc the khong dung mang: nonetkill (DA LOAI BO) va noedict.
// Khac biet sinh tu giua chung o docs/01-co-che.md va docs/05-huong-da-bo.md (muc nonetkill)
static bool g_NoNetKill = false;

// nonetkill: doi ten classname TAI CHO trong entity lump o LevelInit.
// DA LOAI BO - no giet luon Spawn()/Activate(), lam mat decal va sai anh sang.
// Giu lai vi co the huu ich cho lop khac. Xem docs/05-huong-da-bo.md (muc nonetkill)
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

// (spotlight.cpp DocKhoiSpotlight cung doc chuoi lump, nhung tung khoi va KHONG sua
//  chuoi - no moc MapEntity_ParseEntity. Hai bo doc doc lap.)
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

// ---- Diem vao cho module chinh (xem src/giaodien.h) ----
// Sua entity lump TRUOC KHI server.dll parse. Tra ve lump da sua - module chinh
// phai chuyen no vao LevelInit bang RETURN_META_VALUE_NEWPARAMS - hoac NULL.
static const char* NoNetKill_OnLevelInit(const char* pMapName, const char* pMapEntities) {
    if (!g_NoNetKill || !pMapEntities) return NULL;
    if (g_KillCount == 0) LoadKillList();

    int hits = 0;
    const char* fixed = RewriteLump(pMapEntities, &hits);
    if (fixed) {
        if (g_LumpCopy) free(g_LumpCopy);      // giai phong ban cua map truoc
        g_LumpCopy = (char*)fixed;
        EL_LOG("[EdictBudget] NONETKILL: map '%s' - da doi ten %d entity "
                 "(chuoi lump %u byte, do dai GIU NGUYEN)",
                 pMapName ? pMapName : "?", hits, (unsigned)strlen(g_LumpCopy));
        return g_LumpCopy;
    }
    EL_LOG("[EdictBudget] NONETKILL: map '%s' - khong co entity nao khop, "
             "khong doi gi", pMapName ? pMapName : "?");
    return NULL;
}
