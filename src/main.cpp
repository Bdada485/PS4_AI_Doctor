// PS4 AI-Doctor & Resource Optimizer
// Dev by Amir — Userland only (No Kernel, No Risk)
#include <orbis/libkernel.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>
#include <orbis/SysUtil.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

// ===================== بيانات =====================
struct ErrEntry {
    std::string code, title, meaning;
    std::vector<std::string> causes, fixes;
};

static std::vector<ErrEntry> gErrors;
static int gUser = -1, gPad = -1;

// ===================== إشعار النظام =====================
static void notify(const char* text) {
    // 222 = جميع المستخدمين — نفس نمط إشعارات GoldHEN
    sceSysUtilSendSystemNotificationWithText(222, text);
}

// ===================== أدوات نص =====================
static std::string readAll(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return ""; }
    std::string s; s.resize((size_t)n);
    fread(&s[0], 1, (size_t)n, f);
    fclose(f);
    return s;
}

static std::string field(const std::string& o, const std::string& key) {
    std::string k = "\"" + key + "\"";
    size_t p = o.find(k);
    if (p == std::string::npos) return "";
    p = o.find(':', p);
    if (p == std::string::npos) return "";
    size_t q1 = o.find('"', p);
    if (q1 == std::string::npos) return "";
    size_t q2 = o.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return o.substr(q1 + 1, q2 - q1 - 1);
}

static std::vector<std::string> arrayField(const std::string& o, const std::string& key) {
    std::vector<std::string> out;
    size_t p = o.find("\"" + key + "\"");
    if (p == std::string::npos) return out;
    size_t a = o.find('[', p);
    if (a == std::string::npos) return out;
    size_t b = o.find(']', a);
    if (b == std::string::npos) return out;
    std::string body = o.substr(a + 1, b - a - 1);
    size_t i = 0;
    while (true) {
        size_t q1 = body.find('"', i);
        if (q1 == std::string::npos) break;
        size_t q2 = body.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        out.push_back(body.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return out;
}

// ===================== تحميل قاعدة الأخطاء =====================
static void loadErrors() {
    gErrors.clear();
    std::string raw = readAll("/app0/data/errors.json");
    if (raw.empty()) raw = readAll("/data/ps4aidoctor/errors.json");
    if (raw.empty()) raw = readAll("data/errors.json");
    if (raw.empty()) return;

    size_t pos = 0;
    while (true) {
        size_t start = raw.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = raw.find('}', start);
        if (end == std::string::npos) break;
        std::string obj = raw.substr(start, end - start + 1);

        ErrEntry e;
        e.code    = field(obj, "code");
        e.title   = field(obj, "title");
        e.meaning = field(obj, "meaning");
        e.causes  = arrayField(obj, "causes");
        e.fixes   = arrayField(obj, "fixes");
        if (!e.code.empty()) gErrors.push_back(e);

        pos = end + 1;
    }
}

// ===================== فحص التخزين =====================
static void showStorage() {
    const char* mounts[] = {"/user", "/data", "/system_data", "/app", "/mnt/usb0"};
    printf("\n=== تحليل التخزين ===\n\n");
    for (int i = 0; i < 5; i++) {
        struct statvfs st;
        if (statvfs(mounts[i], &st) != 0) {
            printf("  [--] %-14s غير متاح\n", mounts[i]);
            continue;
        }
        double total = (double)st.f_blocks * st.f_frsize;
        double freeb = (double)st.f_bavail * st.f_frsize;
        double used  = total - freeb;
        int pct = total > 0 ? (int)((used / total) * 100.0) : 0;
        printf("  [OK] %-14s مستخدم %3d%%  |  فارغ %.1f GB / %.1f GB\n",
               mounts[i], pct,
               freeb / (1024.0 * 1024 * 1024),
               total / (1024.0 * 1024 * 1024));
    }
}

// ===================== تنظيف آمن =====================
static void cleanPath(const char* path, int* count, unsigned long long* freed) {
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        std::string full = std::string(path) + "/" + e->d_name;
        struct stat st;
        if (lstat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            cleanPath(full.c_str(), count, freed);
            rmdir(full.c_str());
        } else {
            *freed += (unsigned long long)st.st_size;
            if (unlink(full.c_str()) == 0) (*count)++;
        }
    }
    closedir(d);
}

static void runCleaner() {
    const char* targets[] = {"/user/temp", "/user/av_contents/temp", "/data/temp"};
    int count = 0;
    unsigned long long freed = 0;

    printf("\n=== تنظيف الكاش الآمن ===\n\n");
    for (int i = 0; i < 3; i++) {
        printf("  فحص: %s\n", targets[i]);
        cleanPath(targets[i], &count, &freed);
    }

    printf("\n  حُذف: %d ملف | حُرّر: %.2f MB\n", count, freed / (1024.0 * 1024));

    // إشعار النظام الحقيقي
    notify("Clean Cache Success - Dev by Amir");

    printf("\n  [ SUCCESS ] Clean Cache Success\n");
    printf("              Dev by Amir\n");
}

// ===================== إدخال =====================
static int pollPad() {
    if (gPad < 0) return 0;
    OrbisPadData d;
    memset(&d, 0, sizeof(d));
    if (scePadReadState(gPad, &d) != 0) return 0;
    if (d.buttons & ORBIS_PAD_BUTTON_UP)     return 1;
    if (d.buttons & ORBIS_PAD_BUTTON_DOWN)   return 2;
    if (d.buttons & ORBIS_PAD_BUTTON_CROSS)  return 3;
    if (d.buttons & ORBIS_PAD_BUTTON_CIRCLE) return 4;
    return 0;
}

static void waitRelease() {
    for (int i = 0; i < 12; i++) {
        sceKernelUsleep(30000);
        pollPad();
    }
}

static void waitCircle() {
    while (true) {
        sceKernelUsleep(50000);
        if (pollPad() == 4) return;
    }
}

// ===================== شاشة تفاصيل خطأ =====================
static void showError(const ErrEntry& e) {
    printf("\033[2J\033[H");
    printf("==========================================\n");
    printf("  %s\n", e.code.c_str());
    printf("  %s\n", e.title.c_str());
    printf("==========================================\n\n");
    printf("المعنى:\n  %s\n\n", e.meaning.c_str());

    printf("الأسباب المحتملة:\n");
    for (size_t i = 0; i < e.causes.size(); i++)
        printf("  %d) %s\n", (int)i + 1, e.causes[i].c_str());

    printf("\nالحلول:\n");
    for (size_t i = 0; i < e.fixes.size(); i++)
        printf("  %d) %s\n", (int)i + 1, e.fixes[i].c_str());

    printf("\n  [O] رجوع\n");
    waitCircle();
}

// ===================== قائمة الأخطاء =====================
static void errorListScreen() {
    printf("\033[2J\033[H");
    if (gErrors.empty()) {
        printf("تعذّر تحميل errors.json من /app0/data/\n");
        printf("\n  [O] رجوع\n");
        waitCircle();
        return;
    }

    int sel = 0, top = 0;
    const int WIN = 12;

    while (true) {
        printf("\033[2J\033[H");
        printf("=== محلل الأخطاء (%d كوداً) ===\n\n", (int)gErrors.size());

        for (int i = top; i < top + WIN && i < (int)gErrors.size(); i++) {
            printf("  %s %02d. %-14s %s\n",
                   (i == sel ? ">" : " "),
                   i + 1,
                   gErrors[i].code.c_str(),
                   gErrors[i].title.c_str());
        }

        printf("\n  [X] فتح   [Up/Down] تنقّل   [O] رجوع\n");

        int b = pollPad();
        if (b == 1 && sel > 0) sel--;
        else if (b == 2 && sel < (int)gErrors.size() - 1) sel++;
        else if (b == 3) { showError(gErrors[sel]); continue; }
        else if (b == 4) return;

        if (sel < top) top = sel;
        if (sel >= top + WIN) top = sel - WIN + 1;
        waitRelease();
    }
}

// ===================== القائمة الرئيسية =====================
static void menu() {
    int sel = 0;
    const int N = 3;
    const char* labels[3] = {
        "محلل أخطاء النظام",
        "تحليل مساحة التخزين",
        "تنظيف الكاش الآمن"
    };

    while (true) {
        printf("\033[2J\033[H");
        printf("==========================================\n");
        printf("   PS4 AI-Doctor & Resource Optimizer\n");
        printf("              Dev by Amir\n");
        printf("==========================================\n\n");

        for (int i = 0; i < N; i++)
            printf("   %s %s\n", (i == sel ? ">>" : "  "), labels[i]);

        printf("\n   الأكواد المحمّلة: %d\n", (int)gErrors.size());
        printf("\n   [X] اختيار   [Up/Down] تنقّل   [O] خروج\n");

        int b = pollPad();
        if (b == 1 && sel > 0) sel--;
        else if (b == 2 && sel < N - 1) sel++;
        else if (b == 3) {
            if (sel == 0) {
                errorListScreen();
            } else if (sel == 1) {
                printf("\033[2J\033[H");
                showStorage();
                printf("\n  [O] رجوع\n");
                waitCircle();
            } else {
                printf("\033[2J\033[H");
                runCleaner();
                printf("\n  [O] رجوع\n");
                waitCircle();
            }
        } else if (b == 4) {
            return;
        }
        waitRelease();
    }
}

// ===================== نقطة الدخول =====================
int main() {
    OrbisUserServiceInitializeParams up;
    memset(&up, 0, sizeof(up));
    up.priority = 256;
    sceUserServiceInitialize(&up);
    sceUserServiceGetForegroundUser(&gUser);

    scePadInit();
    gPad = scePadOpen(gUser, 0, 0, NULL);

    loadErrors();

    printf("\033[2J\033[H");
    printf("PS4 AI-Doctor v1.0\n");
    printf("Loading database... %d entries\n", (int)gErrors.size());
    sceKernelUsleep(800000);

    notify("PS4 AI-Doctor Loaded - Dev by Amir");

    menu();
    return 0;
}
