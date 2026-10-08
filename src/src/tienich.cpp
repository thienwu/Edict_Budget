// ==========================================================================
//  tienich - ghi vung nho bao ve, mo file cau hinh, tim chu ky
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// ==========================================================================
// Ham phu nho
// ==========================================================================
// Dung boi freegate, baycapphat, noedict, swap, spotlight. wipeclear va mapclear
// KHONG dung ham nay - tu goi VirtualProtect(PAGE_READWRITE); ket qua tuong duong.
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
