// ==========================================================================
//  nhatky - ghi addons/edictbudget/edictbudget.log
//  Chi duoc #include tu sample_mm.cpp (ca plugin la MOT don vi bien dich).
// ==========================================================================
// Ghi log ra file rieng addons/edictbudget/edictbudget.log. Xem docs/06-do-dac.md
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

// Goi trong Unload. Sau ham nay, EL_LOG bat ky se MO LAI file (g_LogOpened=false) ma
// khong ai dong nua - module go sau EL_LOG_CLOSE khong duoc ghi log (xem Unload).
static void EL_LOG_CLOSE() {
    if (g_LogFile) { fclose(g_LogFile); g_LogFile = NULL; }
    g_LogOpened = false;
}
