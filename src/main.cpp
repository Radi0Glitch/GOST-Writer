#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct PTemplate {
    const char* label_ru;
    const char* label_en;
    const char* lang;
    const char* text_ru;
    const char* text_en;
};

static const PTemplate kMathTemplates[] = {
    {"Дробь", "Fraction", "latex", "\\frac{a}{b}\n", "\\frac{a}{b}\n"},
    {"Квадратное уравнение", "Quadratic equation", "latex", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}\n", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}\n"},
    {"Система уравнений", "System of equations", "latex", "\\begin{cases}\nx + y = 0,\\\\\n2x - y = 3\n\\end{cases}\n", "\\begin{cases}\nx + y = 0,\\\\\n2x - y = 3\n\\end{cases}\n"},
    {"Матрица", "Matrix", "latex", "\\begin{bmatrix}\na&b\\\\\nc&d\n\\end{bmatrix}\n", "\\begin{bmatrix}\na&b\\\\\nc&d\n\\end{bmatrix}\n"},
    {"Сумма", "Sum", "latex", "\\sum_{i=1}^{n} x_i\n", "\\sum_{i=1}^{n} x_i\n"},
    {"Интеграл", "Integral", "latex", "\\int_{a}^{b} f(x)\\,dx\n", "\\int_{a}^{b} f(x)\\,dx\n"},
    {"Предел", "Limit", "latex", "\\lim_{x\\to 0}\\frac{\\sin x}{x}=1\n", "\\lim_{x\\to 0}\\frac{\\sin x}{x}=1\n"},
    {"Степень/индекс", "Power / Subscript", "latex", "x^{n},\\quad a_{i,j}\n", "x^{n},\\quad a_{i,j}\n"},
};

static const PTemplate kDiagramTemplates[] = {
    {"Схема: Процесс", "Flow: Process", "mermaid",
     "graph TD\n  A[\"Начало\"] --> B[\"Действие\"]\n  B --> C[\"Конец\"]\n",
     "graph TD\n  A[\"Start\"] --> B[\"Action\"]\n  B --> C[\"End\"]\n"},
    {"Схема: Ветвление", "Flow: Branching", "mermaid",
     "graph TD\n  A[\"Начало\"] --> B{\"Условие\"}\n  B -->|Да| C[\"Действие\"]\n  B -->|Нет| D[\"Конец\"]\n",
     "graph TD\n  A[\"Start\"] --> B{\"Condition\"}\n  B -->|Yes| C[\"Action\"]\n  B -->|No| D[\"End\"]\n"},
    {"Схема: Слева направо", "Flow: Left to Right", "mermaid",
     "graph LR\n  A[\"Идея\"] --> B[\"План\"]\n  B --> C[\"Результат\"]\n",
     "graph LR\n  A[\"Idea\"] --> B[\"Plan\"]\n  B --> C[\"Result\"]\n"},
    {"Схема: Иерархия", "Flow: Hierarchy", "mermaid",
     "graph TD\n  A[\"Раздел\"] --> B[\"Тема\"]\n  A --> C[\"Пример\"]\n  B --> D[\"Вопрос\"]\n  C --> D\n",
     "graph TD\n  A[\"Section\"] --> B[\"Topic\"]\n  A --> C[\"Example\"]\n  B --> D[\"Question\"]\n  C --> D\n"},
    {"Диалог: Последовательность", "Sequence: Dialog", "mermaid",
     "sequenceDiagram\n  participant A as Пользователь\n  participant B as Сервер\n  A->>B: Запрос\n  B-->>A: Ответ\n",
     "sequenceDiagram\n  participant A as User\n  participant B as Server\n  A->>B: Request\n  B-->>A: Response\n"},
    {"Обмен: Клиент-сервер", "Sequence: Client-Server", "mermaid",
     "sequenceDiagram\n  participant C as Клиент\n  participant S as Сервер\n  C->>S: Привет\n  S-->>C: Привет!\n  C->>S: Данные\n  S-->>C: Готово\n",
     "sequenceDiagram\n  participant C as Client\n  participant S as Server\n  C->>S: Hello\n  S-->>C: Hi!\n  C->>S: Data\n  S-->>C: Done\n"},
};

static const PTemplate kChartTemplates[] = {
    {"График: Столбчатый", "Chart: Bar", "mermaid",
     "xychart-beta\n    title \"Продажи\"\n    x-axis [\"Янв\", \"Фев\", \"Мар\", \"Апр\"]\n    y-axis 0 --> 40\n    bar [12, 18, 25, 34]\n",
     "xychart-beta\n    title \"Sales\"\n    x-axis [\"Jan\", \"Feb\", \"Mar\", \"Apr\"]\n    y-axis 0 --> 40\n    bar [12, 18, 25, 34]\n"},
    {"График: Линейный", "Chart: Line", "mermaid",
     "xychart-beta\n    title \"Динамика\"\n    x-axis [\"Пн\", \"Вт\", \"Ср\", \"Чт\", \"Пт\"]\n    y-axis 0 --> 20\n    line [4, 8, 6, 12, 16]\n",
     "xychart-beta\n    title \"Trend\"\n    x-axis [\"Mon\", \"Tue\", \"Wed\", \"Thu\", \"Fri\"]\n    y-axis 0 --> 20\n    line [4, 8, 6, 12, 16]\n"},
    {"График: Круговой", "Chart: Pie", "mermaid",
     "pie title \"Распределение\"\n    \"A\" : 40\n    \"B\" : 25\n    \"C\" : 20\n    \"D\" : 15\n",
     "pie title \"Distribution\"\n    \"A\" : 40\n    \"B\" : 25\n    \"C\" : 20\n    \"D\" : 15\n"},
};

} // namespace


#if defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include "chart_draw.hpp"
#include "chart_model.hpp"
#include "diag_draw.hpp"
#include "diag_model.hpp"
#include "dialogs.hpp"
#include "window_state.hpp"
#include "document.hpp"

// ---------- Minimal PNG encoder + offscreen rendering for diagrams/charts ----------

namespace {
uint32_t png_crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc & 1) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
    }
    return crc;
}

void png_write_be32(std::ofstream& f, uint32_t v) {
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    f.write((const char*)b, 4);
}

void png_chunk(std::ofstream& f, const char* type, const uint8_t* data, size_t len) {
    png_write_be32(f, (uint32_t)len);
    f.write(type, 4);
    if (len > 0) f.write((const char*)data, len);
    uint32_t crc = png_crc32(reinterpret_cast<const uint8_t*>(type), 4, 0xFFFFFFFFu);
    if (len > 0) crc = png_crc32(data, len, crc);
    png_write_be32(f, crc ^ 0xFFFFFFFFu);
}

std::vector<uint8_t> png_zlib_compress(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;
    out.reserve(len + (len / 65535 + 1) * 5 + 6);
    out.push_back(0x78);
    out.push_back(0x01); // zlib header (no compression)
    size_t pos = 0;
    while (pos < len) {
        const size_t block = std::min<size_t>(len - pos, 65535);
        const bool last = (pos + block == len);
        out.push_back(static_cast<uint8_t>(last ? 1 : 0)); // BFINAL | BTYPE=00 (stored)
        const uint16_t b_len = static_cast<uint16_t>(block);
        const uint16_t b_nlen = static_cast<uint16_t>(~b_len);
        out.push_back(static_cast<uint8_t>(b_len & 0xFF));
        out.push_back(static_cast<uint8_t>(b_len >> 8));
        out.push_back(static_cast<uint8_t>(b_nlen & 0xFF));
        out.push_back(static_cast<uint8_t>(b_nlen >> 8));
        out.insert(out.end(), data + pos, data + pos + block);
        pos += block;
    }
    // Adler-32 checksum in big-endian
    uint32_t s1 = 1, s2 = 0;
    for (size_t i = 0; i < len; ++i) {
        s1 = (s1 + data[i]) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    const uint32_t adler = (s2 << 16) | s1;
    out.push_back(static_cast<uint8_t>(adler >> 24));
    out.push_back(static_cast<uint8_t>(adler >> 16));
    out.push_back(static_cast<uint8_t>(adler >> 8));
    out.push_back(static_cast<uint8_t>(adler));
    return out;
}

bool save_png_rgb(const std::string& path, int w, int h, const uint8_t* rgb, uint32_t dpi = 0) {
    std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) return false;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    f.write((const char*)sig, 8);
    {
        uint8_t ihdr[13];
        const uint32_t uw = (uint32_t)w, uh = (uint32_t)h;
        ihdr[0] = (uint8_t)(uw >> 24); ihdr[1] = (uint8_t)(uw >> 16);
        ihdr[2] = (uint8_t)(uw >> 8);  ihdr[3] = (uint8_t)uw;
        ihdr[4] = (uint8_t)(uh >> 24); ihdr[5] = (uint8_t)(uh >> 16);
        ihdr[6] = (uint8_t)(uh >> 8);  ihdr[7] = (uint8_t)uh;
        ihdr[8] = 8;
        ihdr[9] = 2;
        ihdr[10] = 0;
        ihdr[11] = 0;
        ihdr[12] = 0;
        png_chunk(f, "IHDR", ihdr, 13);
    }
    if (dpi > 0) {
        // pHYs chunk: pixels per meter (1 inch = 0.0254 m) so Word/Pandoc sizes A4 page to 210x297 mm
        const uint32_t ppm = static_cast<uint32_t>(std::round(static_cast<double>(dpi) / 0.0254));
        uint8_t phys[9];
        phys[0] = (uint8_t)(ppm >> 24); phys[1] = (uint8_t)(ppm >> 16);
        phys[2] = (uint8_t)(ppm >> 8);  phys[3] = (uint8_t)ppm;
        phys[4] = (uint8_t)(ppm >> 24); phys[5] = (uint8_t)(ppm >> 16);
        phys[6] = (uint8_t)(ppm >> 8);  phys[7] = (uint8_t)ppm;
        phys[8] = 1; // unit is meter
        png_chunk(f, "pHYs", phys, 9);
    }
    {
        size_t row_size = (size_t)w * 3 + 1;
        size_t raw_len = row_size * h;
        std::vector<uint8_t> raw(raw_len);
        for (int y = 0; y < h; ++y) {
            raw[y * row_size] = 0;
            memcpy(raw.data() + y * row_size + 1, rgb + (size_t)y * (size_t)w * 3, (size_t)w * 3);
        }
        auto comp = png_zlib_compress(raw.data(), raw.size());
        if (comp.empty()) return false;
        png_chunk(f, "IDAT", comp.data(), comp.size());
    }
    png_chunk(f, "IEND", nullptr, 0);
    return f.good();
}

bool save_surface_png(SDL_Surface* surf, const std::string& path, uint32_t dpi = 0) {
    if (!surf) return false;
    SDL_Surface* rgb24 = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGB24);
    if (!rgb24) return false;
    std::vector<uint8_t> packed((size_t)rgb24->w * (size_t)rgb24->h * 3);
    const uint8_t* src = static_cast<const uint8_t*>(rgb24->pixels);
    for (int y = 0; y < rgb24->h; ++y) {
        std::memcpy(packed.data() + (size_t)y * (size_t)rgb24->w * 3,
                    src + (size_t)y * (size_t)rgb24->pitch,
                    (size_t)rgb24->w * 3);
    }
    const bool ok = save_png_rgb(path, rgb24->w, rgb24->h, packed.data(), dpi);
    SDL_DestroySurface(rgb24);
    return ok;
}
} // namespace

#include "editor.hpp"
#include "fontbook.hpp"
#include "history.hpp"
#include "layout.hpp"
#include "markdown_io.hpp"
#include "math_draw.hpp"
#include "math_model.hpp"
#include "render.hpp"
#include "search.hpp"
#include "text.hpp"
#include "ui.hpp"

#include <filesystem>

namespace {

int run_utf8_cmd(const std::string& cmd) {
#if defined(_WIN32)
    std::wstring wcmd;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
    if (wlen > 0) {
        wcmd.resize(static_cast<size_t>(wlen - 1));
        MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], wlen);
    }
    std::wstring full_wcmd = L"\"" + wcmd + L"\"";
    return _wsystem(full_wcmd.c_str());
#else
    return std::system(cmd.c_str());
#endif
}

std::string find_pandoc_path() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        std::string dir(base);
        candidates.push_back(dir + "tools/pandoc/linux-x86_64/pandoc");
        candidates.push_back(dir + "tools/pandoc/windows-x86_64/pandoc.exe");
        candidates.push_back(dir + "pandoc");
        candidates.push_back(dir + "pandoc.exe");
        candidates.push_back(dir + "../tools/pandoc/linux-x86_64/pandoc");
        candidates.push_back(dir + "../tools/pandoc/windows-x86_64/pandoc.exe");
    }
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string exe(buf);
        size_t slash = exe.find_last_of('/');
        if (slash != std::string::npos) {
            std::string dir = exe.substr(0, slash);
            candidates.push_back(dir + "/tools/pandoc/linux-x86_64/pandoc");
            candidates.push_back(dir + "/tools/pandoc/windows-x86_64/pandoc.exe");
            candidates.push_back(dir + "/pandoc");
            candidates.push_back(dir + "/pandoc.exe");
        }
    }
#elif defined(_WIN32)
    wchar_t wbuf[MAX_PATH];
    DWORD wlen = GetModuleFileNameW(nullptr, wbuf, MAX_PATH);
    if (wlen > 0) {
        std::filesystem::path exe_p(wbuf);
        std::string dir = exe_p.parent_path().u8string();
        candidates.push_back(dir + "/tools/pandoc/windows-x86_64/pandoc.exe");
        candidates.push_back(dir + "/pandoc.exe");
    }
#endif
    candidates.push_back("tools/pandoc/linux-x86_64/pandoc");
    candidates.push_back("tools/pandoc/windows-x86_64/pandoc.exe");
    candidates.push_back("../tools/pandoc/linux-x86_64/pandoc");
    candidates.push_back("../tools/pandoc/windows-x86_64/pandoc.exe");
    candidates.push_back("pandoc");
    candidates.push_back("pandoc.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (c != "pandoc" && c != "pandoc.exe") {
            if (!std::filesystem::exists(std::filesystem::u8path(c), ec)) continue;
            return c;
        }
#if defined(_WIN32)
        std::string test = "\"" + c + "\" --version > nul 2>&1";
#else
        std::string test = "\"" + c + "\" --version > /dev/null 2>&1";
#endif
        if (run_utf8_cmd(test) == 0) {
            return c;
        }
    }
    return "";
}

std::string find_typst_path() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        std::string dir(base);
        candidates.push_back(dir + "tools/typst/linux-x86_64/typst");
        candidates.push_back(dir + "tools/typst/windows-x86_64/typst.exe");
        candidates.push_back(dir + "typst");
        candidates.push_back(dir + "typst.exe");
        candidates.push_back(dir + "../tools/typst/linux-x86_64/typst");
        candidates.push_back(dir + "../tools/typst/windows-x86_64/typst.exe");
    }
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string exe(buf);
        size_t slash = exe.find_last_of('/');
        if (slash != std::string::npos) {
            std::string dir = exe.substr(0, slash);
            candidates.push_back(dir + "/tools/typst/linux-x86_64/typst");
            candidates.push_back(dir + "/tools/typst/windows-x86_64/typst.exe");
            candidates.push_back(dir + "/typst");
            candidates.push_back(dir + "/typst.exe");
        }
    }
#elif defined(_WIN32)
    wchar_t wbuf[MAX_PATH];
    DWORD wlen = GetModuleFileNameW(nullptr, wbuf, MAX_PATH);
    if (wlen > 0) {
        std::filesystem::path exe_p(wbuf);
        std::string dir = exe_p.parent_path().u8string();
        candidates.push_back(dir + "/tools/typst/windows-x86_64/typst.exe");
        candidates.push_back(dir + "/typst.exe");
    }
#endif
    candidates.push_back("tools/typst/linux-x86_64/typst");
    candidates.push_back("tools/typst/windows-x86_64/typst.exe");
    candidates.push_back("../tools/typst/linux-x86_64/typst");
    candidates.push_back("../tools/typst/windows-x86_64/typst.exe");
    candidates.push_back("typst");
    candidates.push_back("typst.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (c != "typst" && c != "typst.exe") {
            if (!std::filesystem::exists(std::filesystem::u8path(c), ec)) continue;
            return c;
        }
#if defined(_WIN32)
        std::string test = "\"" + c + "\" --version > nul 2>&1";
#else
        std::string test = "\"" + c + "\" --version > /dev/null 2>&1";
#endif
        if (run_utf8_cmd(test) == 0) {
            return c;
        }
    }
    return "";
}

std::string find_fonts_dir() {
    std::vector<std::string> dirs;
    if (const char* base = SDL_GetBasePath()) {
        std::string b(base);
        dirs.push_back(b + "fonts");
        dirs.push_back(b + "../fonts");
        dirs.push_back(b + "../share/marked-text-down/fonts");
    }
    dirs.push_back("fonts");
    dirs.push_back("../fonts");
    dirs.push_back("/usr/share/marked-text-down/fonts");
    dirs.push_back("/usr/local/share/marked-text-down/fonts");
    dirs.push_back(std::string(PROJECT_SOURCE_DIR) + "/fonts");
    for (const auto& d : dirs) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::u8path(d + "/FiraSans-Regular.ttf"), ec)) {
            return d;
        }
    }
    return std::string(PROJECT_SOURCE_DIR) + "/fonts";
}

std::string find_reference_docx() {
    std::vector<std::string> candidates;
    if (const char* base = SDL_GetBasePath()) {
        std::string b(base);
        candidates.push_back(b + "templates/gost/reference-a4.docx");
        candidates.push_back(b + "../templates/gost/reference-a4.docx");
        candidates.push_back(b + "../share/marked-text-down/reference-a4.docx");
    }
    candidates.push_back("templates/gost/reference-a4.docx");
    candidates.push_back("../templates/gost/reference-a4.docx");
    candidates.push_back("/usr/share/marked-text-down/reference-a4.docx");
    candidates.push_back(std::string(PROJECT_SOURCE_DIR) + "/templates/gost/reference-a4.docx");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::u8path(c), ec)) {
            return c;
        }
    }
    return "";
}

const std::vector<std::string> kDialects = {
    "CommonMark",
    "GitHub Flavored Markdown",
};

// Languages offered by the menu on code blocks. The value is what follows the opening fence.
struct CodeLang {
    const char* value;
    const char* label;
};

const CodeLang kCodeLangs[] = {
    {"", ""}, // the first entry clears the language; its label is translated at runtime
    {"python", "Python"}, {"javascript", "JavaScript"}, {"typescript", "TypeScript"},
    {"java", "Java"}, {"c", "C"}, {"cpp", "C++"}, {"csharp", "C#"}, {"go", "Go"},
    {"rust", "Rust"}, {"php", "PHP"}, {"sql", "SQL"}, {"bash", "Bash"}, {"json", "JSON"},
    {"html", "HTML"}, {"css", "CSS"},
};

// Formulas, diagrams and charts are code blocks too, but their language sets their type.
bool is_visual_lang(const std::string& lang) {
    return lang == "math" || lang == "latex" || lang == "tex" || md::is_diagram_lang(lang);
}

const std::vector<std::string> kFileItemsRu = {
    "Новый   Ctrl+N",
    "Открыть...   Ctrl+O",
    "Сохранить   Ctrl+S",
    "Сохранить как...   Ctrl+Shift+S",
    "О редакторе и возможностях",
    "Оглавление   Ctrl+Shift+T",
    "Экспорт   ›",
};

const std::vector<std::string> kFileItemsEn = {
    "New   Ctrl+N",
    "Open...   Ctrl+O",
    "Save   Ctrl+S",
    "Save As...   Ctrl+Shift+S",
    "Welcome & Features",
    "Outline   Ctrl+Shift+T",
    "Export   ›",
};

std::string data_file(const std::string& name) {
    char* pref = SDL_GetPrefPath("MarkedTextDown", "Editor");
    if (pref) {
        std::string p = std::string(pref) + name;
        SDL_free(pref);
        return p;
    }
    return "marked_text_down_" + name;
}

std::string settings_file_path() {
    return data_file("settings.cfg");
}

// Unsaved work is kept here between runs until it is saved or discarded.
std::string autosave_file_path() {
    return data_file("autosave.md");
}

std::string autosave_meta_path() {
    return data_file("autosave.cfg");
}

// Cuts `text` with an ellipsis so it fits in `max_w`, never splitting a UTF-8 character.
std::string fit_text(TTF_Font* font, std::string text, float max_w) {
    if (ui::text_width(font, text) <= max_w) return text;
    while (!text.empty() && ui::text_width(font, text + "...") > max_w) {
        size_t start = text.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) {
            --start;
        }
        text.erase(start);
    }
    return text.empty() ? std::string() : text + "...";
}

struct App {
    static constexpr float kTitleH = 44.0f;
    int ctx_menu_block = -1;
    std::string ctx_menu_lang;
    float ctx_menu_x = 0, ctx_menu_y = 0;
    bool show_ctx_menu = false;
    bool formula_editor_open = false;
    bool formula_just_opened = false;              // swallow the click that opened it
    int formula_block = -1;                        // block being edited, -1 = new
    md::MathNode formula_edit = md::math_seq({});  // the formula tree
    std::vector<int> formula_path;                 // path to the current Text slot
    int formula_off = 0;                           // byte caret offset inside that Text
    std::vector<md::mathdraw::Slot> formula_slots; // hit-test rects from the last draw
    bool diag_editor_open = false;
    bool diag_just_opened = false;              // swallow the click that opened it
    int diag_block = -1;                        // block being edited, -1 = new
    md::DiagModel diag_edit;                    // the diagram being edited
    int diag_sel = -1;                          // selected node (or edge) index
    bool diag_sel_edge = false;                 // the index counts edges when set
    int diag_off = 0;                           // byte caret offset inside its label
    std::vector<md::diagdraw::Slot> diag_slots; // hit-test rects from the last draw

    // footnote editor (по ГОСТ Р 2.105 п. 6.13) — текст редактируется в отдельной панели
    bool footnote_editor_open = false;
    int active_footnote_id = 0;
    int footnote_off = 0;
    bool chart_editor_open = false;
    bool chart_just_opened = false;             // swallow the click that opened it
    int chart_block = -1;                       // block being edited, -1 = new
    md::ChartModel chart_edit;
    int chart_sel = -1;                         // selected data row
    bool chart_sel_value = false;               // true when the number cell is selected
    int chart_off = 0;
    std::vector<md::chartdraw::Slot> chart_slots;
    bool pandoc_help_open = false;
    bool welcome_open = false;
    bool show_welcome_on_startup = true;
    int ui_lang = 0; // 0 = RU, 1 = EN

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    ui::Fonts fonts;
    ui::Context ui;
    md::FontBook fontbook;
    TTF_Font* heading_fonts[3] = {nullptr, nullptr, nullptr}; // H1, H2, H3 button labels
    md::Document doc;
    md::Editor editor{doc};
    md::Layout layout;
    md::History history;
    md::StampField active_stamp_field = md::StampField::None;
    std::string* active_stamp_target = nullptr;
    std::vector<md::StampSlot> stamp_slots;

    std::string* stamp_field_str(md::StampField f) {
        if (active_stamp_target != nullptr && f == active_stamp_field) {
            return active_stamp_target;
        }
        switch (f) {
            case md::StampField::Code:             return &doc.meta.code;
            case md::StampField::Name:             return &doc.meta.name;
            case md::StampField::Org:              return &doc.meta.org;
            case md::StampField::Dev:              return &doc.meta.dev;
            case md::StampField::Check:            return &doc.meta.check;
            case md::StampField::TCheck:           return &doc.meta.tcheck;
            case md::StampField::NCheck:           return &doc.meta.ncheck;
            case md::StampField::Approve:          return &doc.meta.approve;
            case md::StampField::TitleOrg:         return &doc.meta.title_org;
            case md::StampField::TitleOkpd:        return &doc.meta.title_okpd;
            case md::StampField::TitleAgreeRole:   return &doc.meta.title_agree_role;
            case md::StampField::TitleAgreeName:   return &doc.meta.title_agree_name;
            case md::StampField::TitleApproveRole: return &doc.meta.title_approve_role;
            case md::StampField::TitleApproveName: return &doc.meta.title_approve_name;
            case md::StampField::TitleProduct:     return &doc.meta.title_product;
            case md::StampField::TitleDocKind:     return &doc.meta.title_doc_kind;
            case md::StampField::TitleYear:        return &doc.meta.title_year;
            default: return active_stamp_target;
        }
    }

    bool is_editing_stamp() const {
        return active_stamp_field != md::StampField::None || active_stamp_target != nullptr;
    }

    void clear_stamp_focus() {
        active_stamp_field = md::StampField::None;
        active_stamp_target = nullptr;
    }

    void step_stamp_field(int dir) {
        if (!is_editing_stamp()) return;
        std::vector<const md::StampSlot*> edit_slots;
        int cur = -1;
        for (const md::StampSlot& s : stamp_slots) {
            if (s.action > 0) continue;
            if ((active_stamp_target != nullptr && s.target == active_stamp_target)
                || (active_stamp_target == nullptr && s.field != md::StampField::None && s.field == active_stamp_field)) {
                cur = static_cast<int>(edit_slots.size());
            }
            edit_slots.push_back(&s);
        }
        if (edit_slots.empty()) return;
        int next = (cur < 0) ? 0 : (cur + dir);
        const int n = static_cast<int>(edit_slots.size());
        next = ((next % n) + n) % n;
        active_stamp_field = edit_slots[static_cast<size_t>(next)]->field;
        active_stamp_target = edit_slots[static_cast<size_t>(next)]->target;
        editor.blink_tick = SDL_GetTicks();
    }
    float toolbar_h = 52.0f;
    ui::Rect file_rect{};
    bool file_export_mode = false; // the File menu shows the export formats instead
    ui::Rect dialect_rect{};
    ui::Rect theme_rect{};
    ui::Rect lang_rect{};
    ui::Rect help_rect{};
    ui::Rect caption_rects[3]{}; // minimize, maximize, close
    ui::Rect doc_viewport{};
    ui::Rect scrollbar_track{};
    ui::Rect scrollbar_thumb{};
    bool dragging = false;
    bool scrollbar_dragging = false;
    float scrollbar_drag_offset = 0.0f;
    bool doc_dirty = true;
    bool light_theme = false;
    float scroll_y = 0;
    float laid_out_width = -1;
    float laid_out_zoom = -1; // zoom that `layout` was built with
    md::MathReveal math_reveal_shown; // formula shown as source in `layout`
    float zoom = 1.0f;        // document zoom, 0.5 … 2.0
    // Window placement kept between runs; see winstate. Only the restored rectangle is stored.
    winstate::Geometry geometry;
    bool geometry_known = false;
    bool geometry_dirty = false;
    Uint64 geometry_tick = 0;
    // Link dialog, opened by Ctrl+K or the toolbar button.
    bool link_open = false;
    int link_field = 0; // 0 = text, 1 = address
    std::string link_text_value;
    std::string link_href_value;
    ui::Rect link_card{}, link_text_box{}, link_href_box{};
    SDL_Cursor* hand_cursor = nullptr;
    float max_scroll = 0;

    // Pending zoom anchor: the document point that should stay under the
    // cursor after the next relayout. Negative when no anchor is armed.
    float zoom_anchor_doc_y = -1.0f;    // document y under the cursor (old zoom)
    float zoom_anchor_view_y = 0.0f;    // cursor offset from viewport top
    float zoom_anchor_old_zoom = 1.0f;  // zoom the anchor was captured at

    // Find and replace bar (Ctrl+F / Ctrl+H).
    bool find_open = false;
    bool find_focus = false;      // keyboard input goes to the bar
    bool find_replace_mode = false;
    int find_field = 0;           // 0 = search text, 1 = replacement text
    std::string find_query;
    std::string find_replacement;
    std::vector<md::Match> find_matches;
    int find_current = -1;
    ui::Rect find_bar_rect{};
    ui::Rect find_field_rect[2]{}; // search and replacement fields, set by draw

    // Outline sidebar (Ctrl+Shift+T): clickable headings, row rect + block index.
    bool outline_open = false;
    static constexpr float kOutlineW = 220.0f;
    std::vector<std::pair<ui::Rect, int>> outline_rows;

    // Recent files, newest first.
    std::vector<std::string> recent_files;

    // Refreshed together with the layout.
    std::size_t word_count = 0;

    // Autosave: the serial counts edits, so unchanged text is not written again.
    unsigned edit_serial = 0;
    unsigned autosaved_serial = 0;
    Uint64 last_autosave_ms = 0;

    std::string file_path; // empty until the document is opened or saved
    bool modified = false; // unsaved changes

    int dialect = 1;
    std::string status = "Готово";
    int width = 0;
    int height = 0;
    // Text and the document column grow with the window; the sizes above are the minimum.
    float window_scale() const {
        return std::clamp(std::min(width / 1100.0f, height / 720.0f), 1.0f, 4.0f);
    }
    float text_zoom() const {
        return zoom * window_scale();
    }
    bool has_pandoc = false;
    std::string pandoc_path;
    std::string typst_path;
    std::string fonts_dir;
    std::vector<std::string> pending_drops;
    bool drop_gesture_active = false;

    bool gfm() const { return true; }
    bool en() const { return ui_lang == 1; }
    const char* tr(const char* ru, const char* en_str) const { return en() ? en_str : ru; }

    // Takes the window's placement; a maximized window keeps the rectangle it was restored to.
    void remember_window() {
        winstate::Geometry g;
        if (!window || selftest_mode || !winstate::read(window, g)) return;
        geometry.maximized = g.maximized;
        if (g.has_rect) {
            geometry = g;
            geometry_known = true;
        }
    }

    void save_settings() {
        if (selftest_mode) return; // test runs must not change the user's settings
        remember_window();
        std::string content;
        content += "show_welcome=" + std::string(show_welcome_on_startup ? "1" : "0") + "\n";
        content += "lang=" + std::string(ui_lang == 1 ? "en" : "ru") + "\n";
        content += "theme=" + std::string(light_theme ? "light" : "dark") + "\n";
        if (geometry_known) {
            content += "win_x=" + std::to_string(geometry.x) + "\n";
            content += "win_y=" + std::to_string(geometry.y) + "\n";
            content += "win_w=" + std::to_string(geometry.w) + "\n";
            content += "win_h=" + std::to_string(geometry.h) + "\n";
            content += "win_max=" + std::string(geometry.maximized ? "1" : "0") + "\n";
        }
        content += "zoom=" + std::to_string(static_cast<int>(zoom * 100.0f + 0.5f)) + "\n";
        for (const std::string& path : recent_files) content += "recent=" + path + "\n";
        md::write_text_file(settings_file_path(), content);
    }

    void load_settings() {
        std::string content;
        if (!md::read_text_file(settings_file_path(), content)) return;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (line == "show_welcome=0") show_welcome_on_startup = false;
            else if (line == "show_welcome=1") show_welcome_on_startup = true;
            else if (line == "lang=en") ui_lang = 1;
            else if (line == "lang=ru") ui_lang = 0;
            else if (line == "theme=light") light_theme = true;
            else if (line == "theme=dark") light_theme = false;
            else if (line.rfind("recent=", 0) == 0 && recent_files.size() < kMaxRecent) {
                recent_files.push_back(line.substr(7));
            } else if (!selftest_mode && line.rfind("win_", 0) == 0 && line.find('=') != std::string::npos) {
                const std::string key = line.substr(0, line.find('='));
                const int value = std::atoi(line.c_str() + key.size() + 1);
                if (key == "win_x") geometry.x = value;
                else if (key == "win_y") geometry.y = value;
                else if (key == "win_w") geometry.w = value;
                else if (key == "win_h") geometry.h = value;
                else if (key == "win_max") geometry.maximized = value != 0;
                geometry_known = true;
            } else if (!selftest_mode && line.rfind("zoom=", 0) == 0) {
                zoom = std::clamp(std::atoi(line.c_str() + 5) / 100.0f, 0.5f, 2.0f);
            }
        }
    }

    static constexpr size_t kMaxRecent = 8;
    bool selftest_mode = false; // keeps test runs away from the user's files

    void add_recent(const std::string& path) {
        if (path.empty() || selftest_mode) return;
        recent_files.erase(std::remove(recent_files.begin(), recent_files.end(), path),
                           recent_files.end());
        recent_files.insert(recent_files.begin(), path);
        if (recent_files.size() > kMaxRecent) recent_files.resize(kMaxRecent);
        save_settings();
    }

    std::string document_name() const {
        return file_path.empty() ? tr("Без названия", "Untitled")
                                 : std::filesystem::u8path(file_path).filename().u8string();
    }

    void toggle_theme() {
        light_theme = !light_theme;
        ui.palette = light_theme ? ui::light_palette() : ui::dark_palette();
        status = light_theme ? tr("Тема: Светлая", "Theme: Light")
                             : tr("Тема: Тёмная", "Theme: Dark");
        save_settings();
    }

    void toggle_lang() {
        ui_lang = ui_lang == 0 ? 1 : 0;
        status = en() ? "Language: English" : "Язык: Русский";
        update_title();
        save_settings();
    }

    void update_title() {
        SDL_SetWindowTitle(window, ((modified ? "* " : "") + document_name()
                                    + " — GOST-Writer").c_str());
    }

    void mark_edited() {
        doc_dirty = true;
        ++edit_serial;
        if (!modified) {
            modified = true;
            update_title();
        }
        editor.blink_tick = SDL_GetTicks();
    }

    void ensure_caret_visible() {
        if (layout.lines.empty() || doc_viewport.h <= 0.0f) return;
        float cx = 0.0f;
        const int li = md::locate_caret(doc, layout, fontbook, editor.caret(), &cx);
        if (li < 0 || li >= static_cast<int>(layout.lines.size())) return;
        const auto& ln = layout.lines[li];
        const float line_top = ln.y;
        const float line_h = ln.height > 0.0f ? ln.height : 18.0f;
        const float line_bottom = line_top + line_h;
        const float view_top = scroll_y;
        const float view_bottom = scroll_y + doc_viewport.h;
        if (line_top < view_top + 6.0f) {
            scroll_y = std::max(0.0f, line_top - 12.0f);
        } else if (line_bottom > view_bottom - 6.0f) {
            scroll_y = std::min(max_scroll, line_bottom - doc_viewport.h + 16.0f);
        }
        scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);
    }

    void new_document() {
        if (!confirm_discard()) return;
        md::Document fresh;
        fresh.blocks.push_back(md::Block{});
        fresh.blocks[0].inlines.push_back(md::Inline{});
        doc = std::move(fresh);
        editor.set_caret(md::Pos{0, 0, 0});
        scroll_y = 0;
        file_path.clear();
        modified = false;
        doc_dirty = true;
        history.reset(doc, editor.caret(), editor.anchor());
        status = tr("Новый документ", "New document");
        update_title();
    }

    // Saves to file_path, asking for a name first when there is none.
    bool save() {
        std::string path = file_path;
        if (path.empty()) {
            path = dialogs::save_markdown("untitled.md");
            if (path.empty()) return false;
            if (std::filesystem::u8path(path).extension().empty()) path += ".md";
        }
        if (!md::write_text_file(path, md::export_markdown(doc, gfm()))) {
            dialogs::show_error(std::string(tr("Не удалось записать файл:\n", "Could not write file:\n")) + path);
            return false;
        }
        file_path = path;
        modified = false;
        remove_autosave();
        add_recent(path);
        status = std::string(tr("Сохранено: ", "Saved: ")) + document_name();
        update_title();
        return true;
    }

    // Writes the unsaved text to the autosave file, at most every 15 seconds
    // and only when something changed since the last copy.
    void autosave_tick() {
        if (!modified || edit_serial == autosaved_serial) return;
        const Uint64 now = SDL_GetTicks();
        if (now - last_autosave_ms < 15000) return;
        last_autosave_ms = now;
        const std::string meta = "path=" + file_path + "\ndialect=" + std::to_string(dialect) + "\n";
        if (md::write_text_file(autosave_file_path(), md::export_markdown(doc, gfm()))
            && md::write_text_file(autosave_meta_path(), meta)) {
            autosaved_serial = edit_serial;
        }
    }

    void remove_autosave() {
        if (selftest_mode) return;
        std::error_code ec;
        std::filesystem::remove(std::filesystem::u8path(autosave_file_path()), ec);
        std::filesystem::remove(std::filesystem::u8path(autosave_meta_path()), ec);
    }

    // Called once at startup: brings back the copy left by an unfinished session.
    void offer_recovery() {
        std::string text;
        if (!md::read_text_file(autosave_file_path(), text)) return;
        if (!dialogs::ask_restore_autosave()) {
            remove_autosave();
            return;
        }
        std::string meta;
        std::string path;
        int saved_dialect = 0;
        if (md::read_text_file(autosave_meta_path(), meta)) {
            std::istringstream iss(meta);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.rfind("path=", 0) == 0) path = line.substr(5);
                else if (line == "dialect=1") saved_dialect = 1;
            }
        }
        dialect = saved_dialect;
        md::Document loaded;
        md::import_markdown(text, gfm(), loaded);
        doc = std::move(loaded);
        editor.set_caret(md::Pos{0, 0, 0});
        scroll_y = 0;
        file_path = path;
        modified = true;
        doc_dirty = true;
        ++edit_serial;
        autosaved_serial = edit_serial;
        history.reset(doc, editor.caret(), editor.anchor());
        status = tr("Восстановлена несохранённая копия", "Unsaved copy restored");
        update_title();
    }

    bool save_as() {
        const std::string previous = file_path;
        file_path.clear();
        if (!save()) {
            file_path = previous;
            return false;
        }
        return true;
    }

    // Returns true when it is safe to replace or close the document.
    bool confirm_discard() {
        if (!modified) return true;
        const int answer = dialogs::ask_save_changes(document_name());
        if (answer == 0) return false;
        if (answer == 1) return save();
        remove_autosave();
        return true;
    }

    enum class OpenResult { Opened, Cancelled, Error };

    static bool has_supported_markdown_ext(const std::string& path) {
        std::string ext = std::filesystem::u8path(path).extension().u8string();
        for (char& c : ext) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return ext == ".md" || ext == ".markdown" || ext == ".txt";
    }

    OpenResult open_from_path(const std::string& path, bool from_external, bool show_dialogs = true) {
        if (path.empty()) return OpenResult::Cancelled;

        std::error_code ec;
        const std::filesystem::path fspath = std::filesystem::u8path(path);
        if (!std::filesystem::exists(fspath, ec) || std::filesystem::is_directory(fspath, ec)) {
            const std::string msg = std::string(tr("Не удалось открыть файл:\n", "Could not open file:\n")) + path;
            if (show_dialogs) dialogs::show_error(msg);
            status = tr("Ошибка открытия файла", "Error opening file");
            return OpenResult::Error;
        }

        // First verify the file is readable before asking to discard/save current work.
        std::string preview_text;
        if (!md::read_text_file(path, preview_text)) {
            const std::string msg = std::string(tr("Не удалось открыть файл:\n", "Could not open file:\n")) + path;
            if (show_dialogs) dialogs::show_error(msg);
            status = tr("Ошибка чтения файла", "Error reading file");
            return OpenResult::Error;
        }

        if (!confirm_discard()) {
            return OpenResult::Cancelled;
        }

        // Re-read after confirm_discard() in case the user chose to save into the same file.
        std::string text;
        if (!md::read_text_file(path, text)) {
            const std::string msg = std::string(tr("Не удалось открыть файл:\n", "Could not open file:\n")) + path;
            if (show_dialogs) dialogs::show_error(msg);
            status = tr("Ошибка чтения файла", "Error reading file");
            return OpenResult::Error;
        }

        md::Document loaded;
        md::import_markdown(text, true, loaded);
        dialect = 1;

        doc = std::move(loaded);
        editor.set_caret(md::Pos{0, 0, 0});
        scroll_y = 0;
        file_path = path;
        modified = false;
        doc_dirty = true;
        formula_editor_open = false;
        diag_editor_open = false;
        chart_editor_open = false;
        pandoc_help_open = false;
        ui.open_popup.clear();
        if (from_external) {
            welcome_open = false;
        }
        history.reset(doc, editor.caret(), editor.anchor());
        add_recent(path);
        find_matches.clear();
        find_current = -1;
        status = std::string(tr("Открыто: ", "Opened: ")) + document_name();
        update_title();
        return OpenResult::Opened;
    }

    void open() {
        const std::string path = dialogs::open_markdown();
        if (path.empty()) return;
        open_from_path(path, false, true);
    }

    // Index into the export formats offered in the File menu.
    void export_format(size_t index) {
        struct Fmt { const char* label; const char* ext; const char* filter; const char* to; };
        static const Fmt fmts[] = {
            {"DOCX", "document.docx", "*.docx", "docx"},
            {"PDF",  "document.pdf",  "*.pdf",  "pdf"},
            {"HTML", "document.html", "*.html", "html5"},
            {"ODT",  "document.odt",  "*.odt",  "odt"},
            {"EPUB", "document.epub", "*.epub", "epub"},
        };
        if (index >= sizeof(fmts) / sizeof(fmts[0])) return;
        const Fmt& f = fmts[index];
        if (std::string(f.to) == "pdf") {
            if (typst_path.empty()) {
                dialogs::show_error(tr("Typst не найден", "Typst not found"));
                return;
            }
            const std::string out = dialogs::save_export(f.ext, f.filter, f.label);
            if (out.empty()) return;
            if (export_gost_pdf(out)) {
                dialogs::show_info(tr("Экспорт ГОСТ PDF", "GOST PDF Export"),
                                   tr("Документ PDF по ГОСТ сформирован", "GOST PDF exported"));
                status = std::string(tr("Экспортировано по ГОСТ: ", "Exported GOST: ")) + f.label;
            } else {
                dialogs::show_error(tr("Ошибка сборки PDF через Typst", "Typst PDF build failed"));
            }
            return;
        }
        if (!has_pandoc || pandoc_path.empty()) {
            dialogs::show_error(tr("Pandoc не найден", "Pandoc not found"));
            return;
        }
        const std::string out = dialogs::save_export(f.ext, f.filter, f.label);
        if (out.empty()) return;
        if (export_with_pandoc(f.to, out)) {
            dialogs::show_info(tr("Экспорт", "Export"), tr("Файл экспортирован", "File exported"));
            status = std::string(tr("Экспортировано: ", "Exported: ")) + f.label;
        } else {
            dialogs::show_error(tr("Ошибка экспорта через Pandoc", "Pandoc export failed"));
        }
    }

    static std::string typst_escape_str(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\\' || c == '"') out.push_back('\\');
            if (c != '\r' && c != '\n') out.push_back(c);
        }
        return out;
    }

    static std::string typst_escape_content(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '#' || c == '*' || c == '_' || c == '`' || c == '$'
                || c == '<' || c == '>' || c == '@' || c == '\\' || c == '[' || c == ']') {
                out.push_back('\\');
            }
            out.push_back(c);
        }
        return out;
    }

    static std::string typst_inlines(const std::vector<md::Inline>& inls) {
        std::string out;
        for (const md::Inline& inl : inls) {
            if (inl.footnote_id > 0) {
                out += "#footnote[__FOOTNOTE_" + std::to_string(inl.footnote_id) + "__]";
                continue;
            }
            if (inl.text.empty()) continue;
            if (inl.style & md::StyleImage) {
                // Изображение: вставляем как картинку Typst, а fallback — ссылка.
                out += "#image(\"" + typst_escape_str(inl.href) + "\", width: 60%)";
                continue;
            }
            if (inl.style & md::StyleLink) {
                // Ссылка: гиперссылка Typst с текстом в качестве анкора.
                std::string piece = typst_escape_content(inl.text.empty() ? inl.href : inl.text);
                out += "#link(\"" + typst_escape_str(inl.href) + "\")[" + piece + "]";
                continue;
            }
            if (inl.style & md::StyleCode) {
                out += "`" + inl.text + "`";
                continue;
            }
            // Встроенная формула $...$: в Typst экспортируем как inline math.
            if (!inl.text.empty() && inl.text.front() == '$' && inl.text.back() == '$'
                && inl.text.size() >= 2) {
                out += "$" + typst_escape_content(inl.text.substr(1, inl.text.size() - 2)) + "$";
                continue;
            }
            std::string piece = typst_escape_content(inl.text);
            if (inl.style & md::StyleBold) piece = "#strong[" + piece + "]";
            if (inl.style & md::StyleItalic) piece = "#emph[" + piece + "]";
            if (inl.style & md::StyleStrike) piece = "#strike[" + piece + "]";
            out += piece;
        }
        return out;
    }

    // Рендерит визуальный блок (формулу, диаграмму, график) в PNG-файл в светлой ГОСТ-палитре.
    std::string render_visual_block_to_png(const md::Block& b, const std::string& temp_dir, size_t idx) {
        if (!renderer) return std::string();
        std::string code;
        for (const md::Inline& inl : b.inlines) code += inl.text;
        const std::string& lang = b.lang;
        const bool is_math = (lang == "math" || lang == "latex" || lang == "tex");
        const bool is_diag = md::is_diagram_lang(lang) && !is_math;
        const bool is_chart = is_diag && md::is_chart_text(code);
        if (!is_math && !is_diag) return std::string();

        const float saved_zoom = fontbook.zoom();
        fontbook.set_zoom(2.0f);
        const float max_w = 1300.0f;
        float vis_w = 0.0f, vis_h = 0.0f;
        if (is_math) {
            const md::MathNode node = md::math_from_latex(code);
            const auto sz = md::mathdraw::render(nullptr, fontbook, node, 0.0f, 0.0f, 36.0f, nullptr, nullptr);
            vis_w = sz.w + 16.0f;
            vis_h = sz.h + 16.0f;
        } else if (is_chart) {
            bool ok = false;
            (void)md::chart_from_text(code, &ok);
            if (!ok) { fontbook.set_zoom(saved_zoom); return std::string(); }
            const auto sz = md::chartdraw::render_block(nullptr, fontbook, code, 0.0f, 0.0f, max_w);
            vis_w = sz.w + 16.0f;
            vis_h = sz.h + 16.0f;
        } else {
            const auto sz = md::diagdraw::render_block(nullptr, fontbook, code, 0.0f, 0.0f, max_w);
            vis_w = sz.w + 16.0f;
            vis_h = sz.h + 16.0f;
        }
        if (vis_w <= 4.0f || vis_h <= 4.0f) {
            fontbook.set_zoom(saved_zoom);
            return std::string();
        }
        const int pw = std::max(64, static_cast<int>(std::ceil(vis_w)));
        const int ph = std::max(36, static_cast<int>(std::ceil(vis_h)));

        SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, pw, ph);
        if (!tex) {
            fontbook.set_zoom(saved_zoom);
            return std::string();
        }
        SDL_Texture* prev_target = SDL_GetRenderTarget(renderer);
        SDL_SetRenderTarget(renderer, tex);
        SDL_SetRenderClipRect(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        ui::Context ctx{};
        ctx.renderer = renderer;
        ctx.fonts = &fonts;
        ctx.palette = ui::light_palette();

        if (is_math) {
            const md::MathNode node = md::math_from_latex(code);
            md::mathdraw::render(&ctx, fontbook, node, 8.0f, 8.0f, 36.0f, nullptr, nullptr);
        } else if (is_chart) {
            md::chartdraw::render_block(&ctx, fontbook, code, 8.0f, 8.0f, static_cast<float>(pw) - 16.0f);
        } else {
            md::diagdraw::render_block(&ctx, fontbook, code, 8.0f, 8.0f, static_cast<float>(pw) - 16.0f);
        }

        SDL_FlushRenderer(renderer);
        SDL_Surface* surf = SDL_RenderReadPixels(renderer, nullptr);
        SDL_SetRenderTarget(renderer, prev_target);
        SDL_DestroyTexture(tex);
        fontbook.set_zoom(saved_zoom);

        if (!surf) return std::string();
        const std::string png_path = temp_dir + "visual_block_" + std::to_string(idx) + ".png";
        const bool ok = save_surface_png(surf, png_path);
        SDL_DestroySurface(surf);
        return ok ? png_path : std::string();
    }

    // Рендерит каждый лист документа (с рамкой А4, боковым штампом, основной надписью,
    // титульным листом, содержанием, таблицами, графиками, диаграммами, формулами и сносками)
    // ровно так, как он отображается в редакторе, в набор PNG-файлов.
    std::vector<std::string> render_all_pages_to_png(const std::string& temp_dir, float scale = 2.0f) {
        std::vector<std::string> paths;
        if (!renderer) return paths;

        const float saved_zoom = fontbook.zoom();
        const int saved_pt = fontbook.base_pt();
        fontbook.set_base_pt(doc.meta.font_pt);
        fontbook.set_zoom(scale);

        const float content_w = 700.0f * scale;
        md::Layout export_layout;
        md::layout_document(doc, fontbook, content_w, export_layout, md::MathReveal{});

        const float mm = std::max(1.0f, export_layout.mm_px);
        const int page_w = std::max(100, static_cast<int>(std::round(210.0f * mm)));
        const int page_h = std::max(100, static_cast<int>(std::round(297.0f * mm)));
        const int total_pages = std::max(1, static_cast<int>(export_layout.page_tops.size()));

        SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_TARGET, page_w, page_h);
        if (!tex) {
            fontbook.set_base_pt(saved_pt);
            fontbook.set_zoom(saved_zoom);
            return paths;
        }

        SDL_Texture* prev_target = SDL_GetRenderTarget(renderer);
        ui::Context ctx{};
        ctx.renderer = renderer;
        ctx.fonts = &fonts;
        ctx.palette = ui::light_palette();
        ctx.input.mouse_x = -10000.0f;
        ctx.input.mouse_y = -10000.0f;

        for (int p = 0; p < total_pages; ++p) {
            const float pt = export_layout.page_tops.empty()
                ? 0.0f
                : export_layout.page_tops[static_cast<size_t>(p)];
            SDL_SetRenderTarget(renderer, tex);
            SDL_SetRenderClipRect(renderer, nullptr);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);

            const ui::Rect vp{25.0f * mm, 0.0f, content_w, static_cast<float>(page_h)};
            md::render_document(ctx, doc, export_layout, fontbook, vp, pt, nullptr);

            SDL_FlushRenderer(renderer);
            SDL_Surface* surf = SDL_RenderReadPixels(renderer, nullptr);
            if (surf) {
                const std::string png_path = temp_dir + "gost_page_" + std::to_string(p + 1) + ".png";
                // Записываем физическое разрешение (DPI), чтобы лист А4 (210 мм = 8.268 дюйма)
                // занимал всю ширину страницы в Word / LibreOffice / Pandoc без уменьшения.
                const uint32_t page_dpi = static_cast<uint32_t>(std::max(72.0, std::round(surf->w / 8.2677)));
                if (save_surface_png(surf, png_path, page_dpi)) {
                    paths.push_back(png_path);
                }
                SDL_DestroySurface(surf);
            }
        }

        SDL_SetRenderTarget(renderer, prev_target);
        SDL_DestroyTexture(tex);
        fontbook.set_base_pt(saved_pt);
        fontbook.set_zoom(saved_zoom);
        doc_dirty = true;
        return paths;
    }

    bool export_gost_pdf(const std::string& out_path) {
        if (typst_path.empty()) return false;
        std::string temp_dir;
        if (char* pref = SDL_GetPrefPath("MarkedTextDown", "Editor")) {
            temp_dir = pref;
            SDL_free(pref);
        }
        const std::string tmp_typ = temp_dir + "gost_export_tmp.typ";

        // Рендерим каждый лист А4 через md::render_document, чтобы 100% совпадали:
        // рамка 185x287 мм, боковой штамп, основная надпись Форма 2 / 2а, титульный лист,
        // содержание с отточием, нумерация разделов, таблицы, формулы, диаграммы, графики и сноски.
        std::vector<std::string> page_pngs = render_all_pages_to_png(temp_dir, 1.8f);
        std::ostringstream typ;
        if (!page_pngs.empty()) {
            typ << "#set page(paper: \"a4\", margin: 0mm)\n"
                << "#set par(spacing: 0mm, leading: 0mm)\n";
            for (size_t i = 0; i < page_pngs.size(); ++i) {
                if (i > 0) typ << "#pagebreak()\n";
                // Используем относительное имя файла внутри temp_dir — работает одинаково на Linux и Windows
                // без проблем с буквами дисков и обратными слешами.
                const std::string rel_name = std::filesystem::u8path(page_pngs[i]).filename().u8string();
                typ << "#image(\"" << typst_escape_str(rel_name)
                    << "\", width: 210mm, height: 297mm, fit: \"stretch\")\n";
            }
        } else {
            typ << "#let f = (\n"
            << "  name: \"" << typst_escape_str(doc.meta.name) << "\",\n"
            << "  code: \"" << typst_escape_str(doc.meta.code) << "\",\n"
            << "  org: \"" << typst_escape_str(doc.meta.org) << "\",\n"
            << "  dev: \"" << typst_escape_str(doc.meta.dev) << "\",\n"
            << "  check: \"" << typst_escape_str(doc.meta.check) << "\",\n"
            << "  tcheck: \"" << typst_escape_str(doc.meta.tcheck) << "\",\n"
            << "  ncheck: \"" << typst_escape_str(doc.meta.ncheck) << "\",\n"
            << "  approve: \"" << typst_escape_str(doc.meta.approve) << "\",\n"
            << ")\n\n"
            << "#let s-thick = 0.7pt\n"
            << "#let s-thin = 0.25pt\n"
            << "#let lab(t) = text(size: 6pt, style: \"italic\", t)\n"
            << "#let val(t, size: 8pt) = text(size: size, t)\n"
            << "#let cell(x, y, w, h, body, stroke: s-thick, align-to: horizon + left, inset-x: 0.8mm) = place(\n"
            << "  top + left, dx: x, dy: y,\n"
            << "  box(width: w, height: h, stroke: stroke, inset: (x: inset-x, y: 0.3mm))[#align(align-to)[#body]],\n"
            << ")\n"
            << "#let side-stamp() = {\n"
            << "  let hs = (35mm, 25mm, 25mm, 35mm, 25mm)\n"
            << "  let labels = ([Подп. и дата], [Инв. № дубл.], [Взам. инв. №], [Подп. и дата], [Инв. № подл.])\n"
            << "  let y = 0mm\n"
            << "  for i in range(5) {\n"
            << "    let h = hs.at(i)\n"
            << "    cell(0mm, y, 5mm, h, rotate(-90deg, reflow: true)[#lab(labels.at(i))], align-to: horizon + center, inset-x: 0mm)\n"
            << "    cell(5mm, y, 7mm, h, [])\n"
            << "    y += h\n"
            << "  }\n"
            << "}\n"
            << "#let stamp-first(f, sheet, sheets) = {\n"
            << "  let cx = (0mm, 7mm, 17mm, 40mm, 55mm, 65mm)\n"
            << "  for r in range(2) {\n"
            << "    let y = 5mm * r\n"
            << "    for i in range(5) {\n"
            << "      cell(cx.at(i), y, cx.at(i + 1) - cx.at(i), 5mm, [], stroke: (left: s-thick, right: s-thick, top: if r == 0 { s-thick } else { s-thin }, bottom: s-thin))\n"
            << "    }\n"
            << "  }\n"
            << "  for (i, t) in ([Изм.], [Лист], [№ докум.], [Подп.], [Дата]).enumerate() {\n"
            << "    cell(cx.at(i), 10mm, cx.at(i + 1) - cx.at(i), 5mm, lab(t), stroke: (left: s-thick, right: s-thick, top: s-thin, bottom: s-thick), align-to: horizon + center, inset-x: 0.2mm)\n"
            << "  }\n"
            << "  let roles = ([Разраб.], [Пров.], [], [Н.контр.], [Утв.])\n"
            << "  let values = (f.dev, f.check, f.tcheck, f.ncheck, f.approve)\n"
            << "  for r in range(5) {\n"
            << "    let y = 15mm + 5mm * r\n"
            << "    let st = (left: s-thick, right: s-thick, top: if r == 0 { s-thick } else { s-thin }, bottom: if r == 4 { s-thick } else { s-thin })\n"
            << "    let role-txt = if r == 2 and f.tcheck != \"\" { [Т.контр.] } else { roles.at(r) }\n"
            << "    cell(0mm, y, 17mm, 5mm, lab(role-txt), stroke: st)\n"
            << "    cell(17mm, y, 23mm, 5mm, val(values.at(r), size: 7pt), stroke: st)\n"
            << "    cell(40mm, y, 15mm, 5mm, [], stroke: st)\n"
            << "    cell(55mm, y, 10mm, 5mm, [], stroke: st)\n"
            << "  }\n"
            << "  cell(65mm, 0mm, 120mm, 15mm, val(f.code, size: 11pt), align-to: horizon + center)\n"
            << "  cell(65mm, 15mm, 67mm, 25mm, val(f.name, size: 10pt), align-to: horizon + center)\n"
            << "  cell(132mm, 15mm, 15mm, 5mm, lab[Лит.], align-to: horizon + center)\n"
            << "  cell(147mm, 15mm, 15mm, 5mm, lab[Лист], align-to: horizon + center)\n"
            << "  cell(162mm, 15mm, 23mm, 5mm, lab[Листов], align-to: horizon + center)\n"
            << "  for i in range(3) {\n"
            << "    cell(132mm + 5mm * i, 20mm, 5mm, 5mm, [], stroke: (top: s-thick, bottom: s-thick, left: if i == 0 { s-thick } else { s-thin }, right: if i == 2 { s-thick } else { s-thin }))\n"
            << "  }\n"
            << "  cell(147mm, 20mm, 15mm, 5mm, val(size: 8pt)[#sheet], align-to: horizon + center)\n"
            << "  cell(162mm, 20mm, 23mm, 5mm, val(size: 8pt)[#sheets], align-to: horizon + center)\n"
            << "  cell(132mm, 25mm, 53mm, 15mm, val(f.org, size: 9pt), align-to: horizon + center)\n"
            << "}\n"
            << "#let stamp-next(f, sheet) = {\n"
            << "  let cx = (0mm, 7mm, 17mm, 40mm, 55mm, 65mm)\n"
            << "  for r in range(2) {\n"
            << "    let y = 5mm * r\n"
            << "    for i in range(5) {\n"
            << "      cell(cx.at(i), y, cx.at(i + 1) - cx.at(i), 5mm, [], stroke: (left: s-thick, right: s-thick, top: if r == 0 { s-thick } else { s-thin }, bottom: s-thin))\n"
            << "    }\n"
            << "  }\n"
            << "  for (i, t) in ([Изм.], [Лист], [№ докум.], [Подп.], [Дата]).enumerate() {\n"
            << "    cell(cx.at(i), 10mm, cx.at(i + 1) - cx.at(i), 5mm, lab(t), stroke: (left: s-thick, right: s-thick, top: s-thin, bottom: s-thick), align-to: horizon + center, inset-x: 0.2mm)\n"
            << "  }\n"
            << "  cell(65mm, 0mm, 110mm, 15mm, val(f.code, size: 10pt), align-to: horizon + center)\n"
            << "  cell(175mm, 0mm, 10mm, 7mm, lab[Лист], align-to: horizon + center, inset-x: 0.2mm)\n"
            << "  cell(175mm, 7mm, 10mm, 8mm, val(size: 8pt)[#sheet], align-to: horizon + center, inset-x: 0.2mm)\n"
            << "}\n";
        const int title_pages = md::gost_title_pages(doc.meta);
        const int first_text_p = title_pages + 1;
        typ << "#let frame-bg = context {\n"
            << "  let p = counter(page).get().first()\n"
            << "  let total = counter(page).final().first()\n"
            << "  place(top + left, dx: 20mm, dy: 5mm, rect(width: 185mm, height: 287mm, stroke: s-thick))\n"
            << "  place(top + left, dx: 8mm, dy: 147mm)[#side-stamp()]\n"
            << "  if p == " << first_text_p << " {\n"
            << "    place(top + left, dx: 20mm, dy: 252mm)[#stamp-first(f, p, total)]\n"
            << "  } else if p > " << first_text_p << " {\n"
            << "    place(top + left, dx: 20mm, dy: 277mm)[#stamp-next(f, p)]\n"
            << "  }\n"
            << "  place(top + left, dx: 20mm, dy: 292mm)[\n"
            << "    #box(width: 185mm, height: 5mm)[\n"
            << "      #place(top + left, dx: 66mm, dy: 0.8mm)[#lab[Копировал]]\n"
            << "      #place(top + left, dx: 135mm, dy: 0.8mm)[#lab[Формат А4]]\n"
            << "    ]\n"
            << "  ]\n"
            << "}\n"
            << "#set page(paper: \"a4\", margin: (left: 25mm, right: 10mm, top: 15mm, bottom: 30mm), background: frame-bg)\n"
            << "#set text(font: \"PT Serif\", size: " << std::clamp(doc.meta.font_pt, 11, 14) << "pt, lang: \"ru\", hyphenate: true)\n"
            << "#set par(justify: true, leading: " << (0.65f * md::gost_line_spacing(doc.meta.line_spacing_idx)) << "em, first-line-indent: 12.5mm)\n"
            << "#show heading: set block(above: 14mm, below: 10mm)\n"
            << "#show heading: set par(first-line-indent: 12.5mm)\n"
            << "#show heading: set text(hyphenate: false)\n\n";

        auto emit_sign_entry = [&](const md::TitleSign& s) {
            typ << "      " << typst_escape_content(s.role) << "\\\n"
                << "      #v(4mm)\n"
                << "      \\________________________ " << typst_escape_content(s.name) << "\\\n";
            if (!s.date.empty()) {
                typ << "      #text(size: 10pt)[" << typst_escape_content(s.date) << "]\\\n";
            }
            typ << "      #v(4mm)\n";
        };

        if (doc.meta.has_title_page) {
            if (doc.meta.title_by_lu) {
                typ << "// Титульный лист при наличии ЛУ (п. 8.11, Приложение Г ГОСТ Р 2.105-2019)\n"
                    << "#block(width: 100%, height: 100%)[\n"
                    << "  #set par(first-line-indent: 0mm)\n"
                    << "  #v(12mm)\n"
                    << "  #strong[Утвержден]\\\n"
                    << "  #strong[" << typst_escape_content(doc.meta.code) << "-ЛУ]\n"
                    << "  #v(75mm)\n"
                    << "  #align(center)[\n"
                    << "    #text(size: 16pt, weight: \"bold\")[" << typst_escape_content(doc.meta.title_product) << "]\\\n"
                    << "    #v(4mm)\n"
                    << "    #text(size: 14pt)[" << typst_escape_content(doc.meta.title_doc_kind) << "]\\\n"
                    << "    #v(6mm)\n"
                    << "    #text(size: 14pt, weight: \"bold\")[" << typst_escape_content(doc.meta.code) << "]\n"
                    << "  ]\n"
                    << "  #v(1fr)\n"
                    << "  #align(center)[#text(size: 12pt)[" << typst_escape_content(doc.meta.title_year) << "]]\n"
                    << "]\n"
                    << "#pagebreak()\n\n";
            } else {
                typ << "// Титульный лист по разделу 8 ГОСТ Р 2.105-2019\n"
                    << "#block(width: 100%, height: 100%)[\n"
                    << "  #set par(first-line-indent: 0mm)\n"
                    << "  #align(center)[#text(size: 12pt)[" << typst_escape_content(doc.meta.title_org) << "]]\n"
                    << "  #v(6mm)\n";
                if (!doc.meta.title_okpd.empty()) {
                    typ << "  #text(size: 10pt)[Код ОКПД 2: " << typst_escape_content(doc.meta.title_okpd) << "]\n";
                }
                typ << "  #v(10mm)\n"
                    << "  #grid(columns: (1fr, 1fr), gutter: 12mm,\n"
                    << "    [\n";
                if (!doc.meta.title_agrees.empty()) {
                    typ << "      #strong[СОГЛАСОВАНО]\\\n";
                    emit_sign_entry(doc.meta.title_agrees[0]);
                }
                typ << "    ],\n"
                    << "    [\n";
                if (!doc.meta.title_approves.empty()) {
                    typ << "      #strong[УТВЕРЖДАЮ]\\\n";
                    for (size_t ai = 0; ai < std::min<size_t>(2, doc.meta.title_approves.size()); ++ai) {
                        emit_sign_entry(doc.meta.title_approves[ai]);
                    }
                }
                typ << "    ]\n"
                    << "  )\n"
                    << "  #v(24mm)\n"
                    << "  #align(center)[\n"
                    << "    #text(size: 16pt, weight: \"bold\")[" << typst_escape_content(doc.meta.title_product) << "]\\\n"
                    << "    #v(4mm)\n"
                    << "    #text(size: 14pt)[" << typst_escape_content(doc.meta.title_doc_kind) << "]\\\n"
                    << "    #v(6mm)\n"
                    << "    #text(size: 14pt, weight: \"bold\")[" << typst_escape_content(doc.meta.code) << "]\n"
                    << "  ]\n"
                    << "  #v(1fr)\n"
                    << "  #grid(columns: (1fr, 1fr), gutter: 12mm,\n"
                    << "    [\n";
                if (doc.meta.title_agrees.size() > 1) {
                    typ << "      #strong[СОГЛАСОВАНО]\\\n";
                    for (size_t gi = 1; gi < std::min<size_t>(3, doc.meta.title_agrees.size()); ++gi) {
                        emit_sign_entry(doc.meta.title_agrees[gi]);
                    }
                }
                typ << "    ],\n"
                    << "    [\n";
                for (size_t di = 0; di < std::min<size_t>(2, doc.meta.title_devs.size()); ++di) {
                    emit_sign_entry(doc.meta.title_devs[di]);
                }
                typ << "    ]\n"
                    << "  )\n";
                if (title_pages > 1) {
                    typ << "  #align(right)[#text(size: 12pt)[Продолжение на следующем листе]]\n"
                        << "]\n"
                        << "#pagebreak()\n"
                        << "#block(width: 100%, height: 100%)[\n"
                        << "  #set par(first-line-indent: 0mm)\n"
                        << "  #align(right)[#text(size: 12pt)[Продолжение титульного листа]]\n"
                        << "  #v(6mm)\n"
                        << "  #align(center)[#strong[" << typst_escape_content(doc.meta.title_product) << "]\\\n"
                        << "  " << typst_escape_content(doc.meta.code) << "]\n"
                        << "  #v(12mm)\n"
                        << "  #grid(columns: (1fr, 1fr), gutter: 12mm,\n"
                        << "    [\n";
                    if (doc.meta.title_agrees.size() > 3) {
                        typ << "      #strong[СОГЛАСОВАНО]\\\n";
                        for (size_t gi = 3; gi < doc.meta.title_agrees.size(); ++gi) {
                            emit_sign_entry(doc.meta.title_agrees[gi]);
                        }
                    }
                    typ << "    ],\n"
                        << "    [\n";
                    if (doc.meta.title_approves.size() > 2) {
                        typ << "      #strong[УТВЕРЖДАЮ]\\\n";
                        for (size_t ai = 2; ai < doc.meta.title_approves.size(); ++ai) {
                            emit_sign_entry(doc.meta.title_approves[ai]);
                        }
                    }
                    for (size_t di = 2; di < doc.meta.title_devs.size(); ++di) {
                        emit_sign_entry(doc.meta.title_devs[di]);
                    }
                    typ << "    ]\n"
                        << "  )\n"
                        << "  #v(1fr)\n"
                        << "  #align(center)[#text(size: 12pt)[" << typst_escape_content(doc.meta.title_year) << "]]\n"
                        << "]\n"
                        << "#pagebreak()\n\n";
                } else {
                    typ << "  #align(center)[#text(size: 12pt)[" << typst_escape_content(doc.meta.title_year) << "]]\n"
                        << "]\n"
                        << "#pagebreak()\n\n";
                }
            }
        }

        typ << "#place(bottom, float: true, clearance: 0mm, block(width: 100%, height: 25mm))\n\n";

        if (doc.meta.has_toc) {
            typ << "// Содержание по п. 6.2 ГОСТ Р 2.105-2019\n"
                << "#align(center)[#par(first-line-indent: 0mm)[#text(weight: \"bold\", size: 15pt)[Содержание]]]\n"
                << "#v(4mm)\n"
                << "#outline(title: none, indent: 8mm, depth: 2)\n"
                << "#v(8mm)\n\n";
        }

        const int sub_pt = std::max(9, std::clamp(doc.meta.font_pt, 11, 14) - 2);
        const std::vector<md::GostBlockNum> bnums = md::compute_gost_numbering(doc);
        for (size_t bi = 0; bi < doc.blocks.size(); ++bi) {
            const md::Block& b = doc.blocks[bi];
            const md::GostBlockNum gn = (bi < bnums.size()) ? bnums[bi] : md::GostBlockNum{};
            if (gn.starts_new_page) {
                typ << "#pagebreak()\n\n";
            }
            switch (b.type) {
                case md::BlockType::Heading: {
                    const int lvl = std::clamp(b.level, 1, 6);
                    std::string eq(static_cast<size_t>(lvl), '=');
                    std::string htxt = typst_inlines(b.inlines);
                    while (!htxt.empty() && htxt.back() == '.') htxt.pop_back(); // ГОСТ 6.6.2: без точки в конце
                    const std::string full_h = typst_escape_content(gn.heading_prefix) + htxt;
                    if (gn.centered_heading) {
                        typ << "#align(center)[#par(first-line-indent: 0mm)[" << eq << " " << full_h << "]]\n\n";
                    } else {
                        typ << eq << " " << full_h << "\n\n";
                    }
                    break;
                }
                case md::BlockType::Paragraph: {
                    if (!b.inlines.empty() && b.inlines[0].text.rfind("где ", 0) == 0) {
                        typ << "#par(first-line-indent: 0mm)[" << typst_inlines(b.inlines) << "]\n\n";
                    } else {
                        typ << "#par[" << typst_inlines(b.inlines) << "]\n\n";
                    }
                    break;
                }
                case md::BlockType::Quote: {
                    // ГОСТ 6.12: примечание уменьшенным размером шрифта с абзацного отступа
                    if (gn.note_group_header) {
                        typ << "#par[#text(size: " << sub_pt << "pt)[Примечания]]\n\n";
                    }
                    typ << "#par[#text(size: " << sub_pt << "pt)["
                        << typst_escape_content(gn.note_prefix) << typst_inlines(b.inlines) << "]]\n\n";
                    break;
                }
                case md::BlockType::ListItem: {
                    const std::string ind(static_cast<size_t>(b.depth) * 2, ' ');
                    typ << ind << (b.ordered ? "+ " : "- ") << typst_inlines(b.inlines) << "\n";
                    break;
                }
                case md::BlockType::Rule: {
                    typ << "#pagebreak()\n\n";
                    break;
                }
                case md::BlockType::Table: {
                    const int cols = md::table_cols(b);
                    const int rows = md::table_rows(b);
                    if (cols > 0 && rows > 0) {
                        const bool is_full_form = (doc.meta.form_type > 0 && bi == 0);
                        if (is_full_form) {
                            const char* col_spec = (doc.meta.form_type == 1 && cols == 7)
                                ? "(6mm, 6mm, 8mm, 70mm, 63mm, 10mm, 22mm)"
                                : (doc.meta.form_type == 2 && cols == 3)
                                ? "(7mm, 70mm, 108mm)"
                                : (doc.meta.form_type == 3 && cols == 7)
                                ? "(7mm, 8mm, 70mm, 64mm, 8mm, 8mm, 20mm)"
                                : nullptr;
                            typ << "#pad(x: -5mm, top: -10mm)[\n"
                                << "  #set text(size: " << sub_pt << "pt)\n"
                                << "  #table(\n"
                                << "    columns: " << (col_spec ? std::string(col_spec) : ("(" + std::to_string(cols) + " * (1fr,))")) << ",\n"
                                << "    stroke: 0.5pt,\n"
                                << "    inset: (x: 1.2mm, y: 2mm),\n";
                            for (int r = 0; r < rows; ++r) {
                                for (int c = 0; c < cols; ++c) {
                                    typ << "    [" << typst_inlines(b.cells[static_cast<size_t>(r * cols + c)]) << "],\n";
                                }
                                if (r == 0 && rows > 1) {
                                    typ << "    table.hline(stroke: 1.2pt),\n";
                                }
                            }
                            const int pad_rows = std::max(0, 24 - rows);
                            for (int pr = 0; pr < pad_rows; ++pr) {
                                for (int c = 0; c < cols; ++c) {
                                    typ << "    [#v(4mm)],\n";
                                }
                            }
                            typ << "  )\n]\n\n";
                        } else {
                            const std::string tcap = gn.table_caption.empty() ? "Таблица" : gn.table_caption;
                            typ << "#block(breakable: true, width: 100%)[\n"
                                << "  #par(first-line-indent: 0mm)[#text(size: " << sub_pt << "pt)[" << typst_escape_content(tcap) << "]]\n"
                                << "  #v(2mm)\n"
                                << "  #set text(size: " << sub_pt << "pt)\n"
                                << "  #table(\n"
                                << "    columns: (" << cols << " * (1fr,)),\n"
                                << "    rows: (auto,).at(0),\n"
                                << "    stroke: 0.5pt,\n"
                                << "    inset: (x: 2mm, y: 2.2mm),\n";
                            for (int r = 0; r < rows; ++r) {
                                for (int c = 0; c < cols; ++c) {
                                    typ << "    [" << typst_inlines(b.cells[static_cast<size_t>(r * cols + c)]) << "],\n";
                                }
                                if (r == 0 && rows > 1) {
                                    typ << "    table.hline(stroke: 1.2pt),\n";
                                }
                            }
                            typ << "  )\n]\n\n";
                        }
                    }
                    break;
                }
                case md::BlockType::CodeBlock: {
                    std::string raw;
                    for (const md::Inline& inl : b.inlines) raw += inl.text;
                    const std::string& l = b.lang;
                    if (l == "math" || l == "latex" || l == "tex") {
                        // ГОСТ 6.10: формула по центру строки и номер справа в круглых скобках
                        std::string png = render_visual_block_to_png(b, temp_dir, bi);
                        typ << "#grid(columns: (1fr, auto, 1fr), align: (left, center + horizon, right + horizon),\n"
                            << "  [],\n";
                        if (!png.empty()) {
                            typ << "  [#image(\"" << typst_escape_str(png) << "\", height: 14mm)],\n";
                        } else {
                            typ << "  [" << typst_escape_content(raw) << "],\n";
                        }
                        typ << "  [" << typst_escape_content(gn.formula_number) << "],\n"
                            << ")\n\n";
                    } else if (md::is_diagram_lang(l) || md::is_chart_text(raw)) {
                        // ГОСТ 6.9: графический материал — рендерим в изображение для совпадения с редактором.
                        std::string png = render_visual_block_to_png(b, temp_dir, bi);
                        if (!png.empty()) {
                            typ << "#block(width: 100%)[\n"
                                << "  #align(center)[#image(\"" << typst_escape_str(png)
                                << "\", width: 80%)]\n"
                                << "  #v(2mm)\n"
                                << "  #align(center)[#text(size: 10pt)["
                                << typst_escape_content(gn.figure_caption) << "]]\n"
                                << "]\n\n";
                        } else {
                            // Fallback: show code if rendering failed.
                            typ << "#block(width: 100%)[\n"
                                << "  #align(center)[```\n" << raw << "\n```]\n"
                                << "  #v(2mm)\n"
                                << "  #align(center)[#text(size: 10pt)["
                                << typst_escape_content(gn.figure_caption) << "]]\n"
                                << "]\n\n";
                        }
                    } else {
                        typ << "```\n" << raw << "\n```\n\n";
                    }
                    break;
                }
            }
        }

        if (doc.meta.has_lri) {
            typ << "#pagebreak()\n"
                << "#pad(x: -5mm, top: -10mm)[\n"
                << "  #set text(size: 8pt)\n"
                << "  #table(\n"
                << "    columns: (10mm, 18mm, 18mm, 18mm, 20mm, 22mm, 25mm, 28mm, 14mm, 12mm),\n"
                << "    align: center + horizon,\n"
                << "    stroke: 0.5pt,\n"
                << "    inset: 1mm,\n"
                << "    table.cell(colspan: 10)[#text(size: 10pt)[Лист регистрации изменений]],\n"
                << "    table.cell(rowspan: 2)[Изм.],\n"
                << "    table.cell(colspan: 4)[Номера листов (страниц)],\n"
                << "    table.cell(rowspan: 2)[Всего листов (страниц) в докум.],\n"
                << "    table.cell(rowspan: 2)[№ докум.],\n"
                << "    table.cell(rowspan: 2)[Входящий № сопроводит. докум. и дата],\n"
                << "    table.cell(rowspan: 2)[Подп.],\n"
                << "    table.cell(rowspan: 2)[Дата],\n"
                << "    [изменен- ных], [заменен- ных], [новых], [аннулиро- ванных],\n"
                << "    table.hline(stroke: 1.2pt),\n";
            for (int r = 0; r < 27; ++r) {
                for (int c = 0; c < 10; ++c) {
                    typ << "    [#v(4.2mm)],\n";
                }
            }
            typ << "  )\n]\n";
        }
        } // end else (fallback typst markup)

        std::string typ_out = typ.str();
        for (const auto& f : doc.footnotes) {
            const std::string key = "#footnote[__FOOTNOTE_" + std::to_string(f.id) + "__]";
            const std::string repl = "#footnote[" + typst_escape_content(f.text) + "]";
            size_t pos = 0;
            while ((pos = typ_out.find(key, pos)) != std::string::npos) {
                typ_out.replace(pos, key.size(), repl);
                pos += repl.size();
            }
        }
        if (!md::write_text_file(tmp_typ, typ_out)) return false;

        std::string root_dir = temp_dir;
        while (root_dir.size() > 1 && (root_dir.back() == '/' || root_dir.back() == '\\')) {
            root_dir.pop_back();
        }
        std::string cmd = "\"" + typst_path + "\" compile";
        if (!root_dir.empty()) {
            cmd += " --root \"" + root_dir + "\"";
        }
        if (!fonts_dir.empty()) {
            cmd += " --font-path \"" + fonts_dir + "\"";
        }
        cmd += " \"" + tmp_typ + "\" \"" + out_path + "\"";
        int rc = run_utf8_cmd(cmd);
        std::error_code ec;
        std::filesystem::remove(std::filesystem::u8path(tmp_typ), ec);
        for (const auto& p : page_pngs) {
            std::filesystem::remove(std::filesystem::u8path(p), ec);
        }
        return rc == 0 && std::filesystem::exists(std::filesystem::u8path(out_path), ec);
    }

    bool export_with_pandoc(const std::string& to_fmt, const std::string& out_path) {
        if (to_fmt == "pdf" && !typst_path.empty()) {
            return export_gost_pdf(out_path);
        }
        if (!has_pandoc || pandoc_path.empty()) return false;
        std::string temp_dir;
        if (char* pref = SDL_GetPrefPath("MarkedTextDown", "Editor")) {
            temp_dir = pref;
            SDL_free(pref);
        }
        const std::string tmp_in = temp_dir + "export_input_tmp.md";
        std::vector<std::string> temp_pngs;

        const std::string ref_docx = (to_fmt == "docx") ? find_reference_docx() : std::string();
        std::string md_text;
        if (to_fmt == "docx" || to_fmt == "odt" || to_fmt == "html5") {
            // Рендерим каждый лист А4 со штампом, рамкой, титульным листом, содержанием,
            // таблицами, диаграммами, графиками, формулами и сносками ровно как в редакторе.
            temp_pngs = render_all_pages_to_png(temp_dir, 1.8f);
            if (!temp_pngs.empty()) {
                std::ostringstream oss;
                const char* w_attr = (to_fmt == "docx" && !ref_docx.empty()) ? "210mm" : "165mm";
                for (size_t i = 0; i < temp_pngs.size(); ++i) {
                    if (i > 0) oss << "\n\n";
                    const std::string rel_name = std::filesystem::u8path(temp_pngs[i]).filename().u8string();
                    oss << "![](" << rel_name << "){width=" << w_attr << "}";
                }
                oss << "\n";
                md_text = oss.str();
            }
        }
        if (md_text.empty()) {
            // Готовим копию документа, где визуальные блоки (диаграммы/графики) заменены на PNG-рисунки с подписями по ГОСТ.
            md::Document exp_doc = doc;
            const std::vector<md::GostBlockNum> bnums = md::compute_gost_numbering(doc);
            std::vector<md::Block> new_blocks;
            for (size_t bi = 0; bi < exp_doc.blocks.size(); ++bi) {
                md::Block b = exp_doc.blocks[bi];
                const md::GostBlockNum gn = (bi < bnums.size()) ? bnums[bi] : md::GostBlockNum{};
                if (b.type == md::BlockType::Heading && !gn.heading_prefix.empty() && !b.inlines.empty()) {
                    b.inlines[0].text = gn.heading_prefix + b.inlines[0].text;
                } else if (b.type == md::BlockType::CodeBlock) {
                    std::string raw;
                    for (const md::Inline& inl : b.inlines) raw += inl.text;
                    if (md::is_diagram_lang(b.lang) || md::is_chart_text(raw)) {
                        std::string png = render_visual_block_to_png(b, temp_dir, bi);
                        if (!png.empty()) {
                            temp_pngs.push_back(png);
                            const std::string rel_name = std::filesystem::u8path(png).filename().u8string();
                            md::Block img_p;
                            img_p.type = md::BlockType::Paragraph;
                            md::Inline img_inl;
                            img_inl.text = gn.figure_caption.empty() ? "Рисунок" : gn.figure_caption;
                            img_inl.style = md::StyleLink | md::StyleImage;
                            img_inl.href = rel_name;
                            img_p.inlines.push_back(std::move(img_inl));
                            new_blocks.push_back(std::move(img_p));
                            if (!gn.figure_caption.empty()) {
                                md::Block cap_p;
                                cap_p.type = md::BlockType::Paragraph;
                                cap_p.inlines.push_back(md::Inline{gn.figure_caption, 0, ""});
                                new_blocks.push_back(std::move(cap_p));
                            }
                            continue;
                        }
                    }
                }
                new_blocks.push_back(std::move(b));
            }
            exp_doc.blocks = std::move(new_blocks);
            md_text = md::export_markdown(exp_doc, true);
        }
        if (!md::write_text_file(tmp_in, md_text)) {
            return false;
        }

        std::string title = document_name();
        if (title.empty()) title = "Document";
        // Strip quotes from title to keep CLI clean
        title.erase(std::remove(title.begin(), title.end(), '"'), title.end());

        std::string res_dir = temp_dir;
        while (res_dir.size() > 1 && (res_dir.back() == '/' || res_dir.back() == '\\')) {
            res_dir.pop_back();
        }
        std::string cmd = "\"" + pandoc_path + "\" \"" + tmp_in + "\" -f markdown -t " + to_fmt
                        + " --standalone --dpi=96 --metadata title=\"" + title + "\"";
        if (!res_dir.empty()) {
            cmd += " --resource-path=\"" + res_dir + "\"";
        }
        if (to_fmt == "docx" && !ref_docx.empty()) {
            cmd += " --reference-doc=\"" + ref_docx + "\"";
        }
        if (to_fmt == "pdf") {
            if (!typst_path.empty()) {
                cmd += " --pdf-engine=\"" + typst_path + "\""
                     + " -V mainfont=\"Fira Sans\" -V monofont=\"Fira Mono\"";
                if (!fonts_dir.empty()) {
                    cmd += " --pdf-engine-opt=\"--font-path=" + fonts_dir + "\"";
                }
            }
        }
        cmd += " -o \"" + out_path + "\"";
        int rc = run_utf8_cmd(cmd);
        std::error_code ec;
        std::filesystem::remove(std::filesystem::u8path(tmp_in), ec);
        for (const auto& p : temp_pngs) {
            std::filesystem::remove(std::filesystem::u8path(p), ec);
        }
        return rc == 0 && std::filesystem::exists(std::filesystem::u8path(out_path), ec);
    }

    bool init() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetAppMetadata("GOST-Writer", "1.0.0", "com.radioglitch.gostwriter");
        if (!TTF_Init()) {
            std::fprintf(stderr, "TTF_Init: %s\n", SDL_GetError());
            return false;
        }
        load_settings();
        window = SDL_CreateWindow("GOST-Writer", 1100, 720,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window) {
            std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
            return false;
        }
        {
            std::vector<std::string> icon_paths;
            if (const char* base = SDL_GetBasePath()) {
                std::string b(base);
                icon_paths.push_back(b + "packaging/icons/gw-logo-64.bmp");
                icon_paths.push_back(b + "../packaging/icons/gw-logo-64.bmp");
                icon_paths.push_back(b + "../share/marked-text-down/gw-logo-64.bmp");
            }
            icon_paths.push_back("packaging/icons/gw-logo-64.bmp");
            icon_paths.push_back("../packaging/icons/gw-logo-64.bmp");
            icon_paths.push_back(std::string(PROJECT_SOURCE_DIR) + "/packaging/icons/gw-logo-64.bmp");
            for (const auto& ip : icon_paths) {
                if (SDL_Surface* icon = SDL_LoadBMP(ip.c_str())) {
                    SDL_SetWindowIcon(window, icon);
                    SDL_DestroySurface(icon);
                    break;
                }
            }
        }
        // The window draws its own frame and title bar; see draw_titlebar().
        SDL_SetWindowBordered(window, false);
        SDL_SetWindowMinimumSize(window, 560, 400);
        SDL_SetWindowHitTest(window, hit_test, this);
        // Last: SDL's own setup above would otherwise move the window back to its remembered rectangle.
        if (geometry_known && !selftest_mode && winstate::usable(geometry)) {
            winstate::apply(window, geometry);
        }
        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
            return false;
        }
        hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
        SDL_SetRenderVSync(renderer, 1);
        {
            SDL_DisplayID did = SDL_GetDisplayForWindow(window);
            if (did != 0) {
                float sc = SDL_GetDisplayContentScale(did);
                if (sc > 0.0f && sc != 1.0f) {
                    // Some backends need texture filter set; also try render scale carefully
                }
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_StartTextInput(window);
        update_title();

        fonts_dir = find_fonts_dir();
        if (!fontbook.init(fonts_dir)) {
            std::fprintf(stderr, "Font load failed: %s\n", SDL_GetError());
            return false;
        }
        if (!TTF_FontHasGlyph(fontbook.get(md::Family::Sans, false, false, 17.0f), 0x042F)
            || !TTF_FontHasGlyph(fontbook.get(md::Family::Mono, false, false, 15.0f), 0x042F)) {
            std::fprintf(stderr, "Font has no Cyrillic glyphs\n");
            return false;
        }
        fonts.regular = fontbook.get(md::Family::Sans, false, false, 15.0f);
        fonts.bold = fontbook.get(md::Family::Sans, true, false, 15.0f);
        fonts.italic = fontbook.get(md::Family::Sans, false, true, 15.0f);
        fonts.bold_italic = fontbook.get(md::Family::Sans, true, true, 15.0f);
        fonts.strike = fontbook.get(md::Family::Sans, false, false, 15.0f, true);
        fonts.mono = fontbook.get(md::Family::Mono, false, false, 14.0f);
        // Heading buttons get three sizes so H1, H2 and H3 are easy to tell apart.
        heading_fonts[0] = fontbook.get(md::Family::Sans, true, false, 19.0f);
        heading_fonts[1] = fontbook.get(md::Family::Sans, true, false, 16.0f);
        heading_fonts[2] = fontbook.get(md::Family::Sans, true, false, 13.0f);
        doc = md::make_demo_document();
        pandoc_path = find_pandoc_path();
        if (!pandoc_path.empty()) {
            has_pandoc = true;
        }
        typst_path = find_typst_path();
        history.reset(doc, editor.caret(), editor.anchor());
        editor.blink_tick = SDL_GetTicks();

        ui.renderer = renderer;
        ui.fonts = &fonts;
        ui.palette = light_theme ? ui::light_palette() : ui::dark_palette();
        welcome_open = show_welcome_on_startup;
        status = tr("Готово", "Ready");
        update_title();
        return true;
    }

    void shutdown() {
        save_settings();
        fontbook.clear();
        if (hand_cursor) SDL_DestroyCursor(hand_cursor);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }

    // Lets SDL move and resize the borderless window. Controls in the title row
    // stay clickable; the rest of the row drags the window.
    static SDL_HitTestResult hit_test(SDL_Window* win, const SDL_Point* point, void* data) {
        const App& app = *static_cast<const App*>(data);
        const float x = static_cast<float>(point->x);
        const float y = static_cast<float>(point->y);
        const float w = static_cast<float>(app.width);
        const float h = static_cast<float>(app.height);

        if (y < kTitleH) {
            const ui::Rect controls[] = {app.file_rect, app.dialect_rect, app.theme_rect,
                                         app.lang_rect, app.help_rect,
                                         app.caption_rects[0], app.caption_rects[1],
                                         app.caption_rects[2]};
            for (const ui::Rect& r : controls) {
                if (ui::contains(r, x, y)) return SDL_HITTEST_NORMAL;
            }
        }

        if (!(SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED)) {
            const float grip = 6.0f;
            const bool left = x < grip;
            const bool right = x >= w - grip;
            const bool top = y < grip;
            const bool bottom = y >= h - grip;
            if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
            if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
            if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
            if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
            if (left) return SDL_HITTEST_RESIZE_LEFT;
            if (right) return SDL_HITTEST_RESIZE_RIGHT;
            if (top) return SDL_HITTEST_RESIZE_TOP;
            if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
        }

        return y < kTitleH ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
    }

    void request_quit() {
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
    }

    // Tab strip under the title row. The table tab is locked unless the dialect
    // supports tables.
    // Title row on top, then one centred formatting row. Inside a table the row
    // shows the table tools instead. Narrow windows wrap the row onto more lines.
    void draw_toolbar() {
        using namespace ui;
        const float pad = 14.0f;
        const float btn_h = 30.0f;
        const float row_step = btn_h + 8.0f;
        const float right = static_cast<float>(width) - pad;
        const float top = kTitleH + 8.0f;
        const bool tables_ok = gfm();

        std::vector<float> row_ends; // right edge of each row, measured in the first pass
        bool measuring = true;
        int row = 0;
        float row_start = pad;
        float x = pad;
        float y = top;

        auto row_origin = [&](int r) {
            float offset = 0.0f;
            if (!measuring && static_cast<size_t>(r) < row_ends.size()) {
                offset = std::max(0.0f, (static_cast<float>(width) - 2.0f * pad
                                         - (row_ends[static_cast<size_t>(r)] - pad)) / 2.0f);
            }
            return pad + offset;
        };
        auto place = [&](float w) {
            if (x > row_start && x + w > right) {
                if (measuring) row_ends.push_back(x - 2.0f);
                ++row;
                row_start = row_origin(row);
                x = row_start;
                y = top + row * row_step;
            }
            const Rect r{x, y, w, btn_h};
            x += w + 2.0f;
            return r;
        };
        auto place_label = [&](const char* label, TTF_Font* font = nullptr) {
            TTF_Font* f = font ? font : fonts.regular;
            return place(std::max(30.0f, text_width(f, label) + 12.0f));
        };
        auto sep = [&]() {
            if (!measuring) ui::separator(ui, {x + 4.0f, y + 8.0f, 1.0f, btn_h - 16.0f});
            x += 10.0f;
        };

        auto insert_template = [&](const PTemplate& t) {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = t.lang;
            b.inlines.clear();
            b.inlines.push_back(md::Inline{std::string(en() ? t.text_en : t.text_ru), 0, ""});
            doc.blocks.insert(doc.blocks.begin() + editor.caret().block + 1, std::move(b));
            doc_dirty = true;
            mark_edited();
            editor.set_caret(md::Pos{editor.caret().block + 1, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
        };

        auto body = [&]() {
            row = 0;
            row_start = row_origin(0);
            x = row_start;
            y = top;

            auto widget_button = [&](const char* id, const char* label, bool enabled = true) {
                const Rect r = place_label(label);
                return !measuring && ui::button(ui, id, r, label, enabled);
            };
            auto widget_toggle = [&](const char* id, const char* label, bool active,
                                     bool enabled = true, TTF_Font* font = nullptr) {
                const Rect r = place_label(label, font);
                return !measuring && ui::toggle_button(ui, id, r, label, active, enabled, font);
            };

            // Formatting is always available, also inside tables.
            const unsigned style = editor.active_style();
            if (widget_toggle("bold", tr("Ж", "B"), style & md::StyleBold, true, fonts.bold)) {
                editor.toggle_style(md::StyleBold);
                mark_edited();
            }
            if (widget_toggle("italic", tr("К", "I"), style & md::StyleItalic, true, fonts.italic)) {
                editor.toggle_style(md::StyleItalic);
                mark_edited();
            }
            if (widget_toggle("strike", tr("Зач.", "S"), style & md::StyleStrike, true, fonts.strike)) {
                editor.toggle_style(md::StyleStrike);
                mark_edited();
            }
            if (widget_toggle("code", "</>", style & md::StyleCode, true, fonts.mono)) {
                editor.toggle_style(md::StyleCode);
                mark_edited();
            }
            if (widget_button("link", tr("Ссылка", "Link"))) {
                link_open_dialog();
            }
            sep();

            const md::Block& cb = doc.blocks[editor.caret().block];
            const int cb_level = cb.level;
            auto block_button = [&](const char* id, const char* label, bool active,
                                    md::BlockType t, int level, bool ordered,
                                    TTF_Font* font = nullptr) {
                if (widget_toggle(id, label, active, true, font)) {
                    if (active) {
                        editor.set_block(md::BlockType::Paragraph, 0, false);
                    } else {
                        editor.set_block(t, level, ordered);
                    }
                    mark_edited();
                    ensure_caret_visible();
                }
            };
            block_button("h1", "H₁", cb.type == md::BlockType::Heading && cb_level == 1,
                         md::BlockType::Heading, 1, false, heading_fonts[0]);
            block_button("h2", "H₂", cb.type == md::BlockType::Heading && cb_level == 2,
                         md::BlockType::Heading, 2, false, heading_fonts[1]);
            block_button("h3", "H₃", cb.type == md::BlockType::Heading && cb_level == 3,
                         md::BlockType::Heading, 3, false, heading_fonts[2]);
            sep();

            {
                const std::vector<std::string> items = {
                    tr("— Дефис (ГОСТ 6.7)", "— Dash (GOST 6.7)"),
                    tr("1. Нумерованный", "1. Numbered"),
                    tr("а) Буквенный (ГОСТ 6.7)", "a) Lettered (GOST 6.7)"),
                };
                const char* lbl = tr("Список ↓", "List ↓");
                const Rect r = place_label(lbl);
                const int ch = !measuring ? ui::menu(ui, "list_menu", r, lbl, items, 240.0f) : -1;
                if (ch >= 0 && ch <= 2) {
                    const bool ordered = (ch == 1 || ch == 2);
                    const int lvl = (ch == 2) ? 2 : 0;
                    if (cb.type == md::BlockType::ListItem && cb.ordered == ordered && cb.level == lvl) {
                        editor.set_block(md::BlockType::Paragraph, 0, false);
                    } else {
                        editor.set_block(md::BlockType::ListItem, lvl, ordered);
                    }
                    mark_edited();
                    ensure_caret_visible();
                }
            }
            block_button("quote", tr("Примечание", "Note"), cb.type == md::BlockType::Quote,
                         md::BlockType::Quote, 0, false);
            if (widget_button("footnote_btn", tr("Сноска", "Footnote"))) {
                md::Pos p = editor.caret();
                if (p.block < 0 || p.block >= static_cast<int>(doc.blocks.size())) return;
                int fid = doc.next_footnote_id++;
                doc.footnotes.push_back(md::Footnote{fid, tr("текст сноски", "footnote text")});
                md::Inline marker;
                marker.footnote_id = fid;
                marker.text.clear();
                md::Block& b = doc.blocks[static_cast<size_t>(p.block)];
                auto& inls = md::inlines_of(b, p.cell);
                if (p.inl < 0) p.inl = 0;
                if (p.inl > static_cast<int>(inls.size())) p.inl = static_cast<int>(inls.size());
                if (inls.empty()) {
                    inls.push_back(md::Inline{});
                    inls.insert(inls.begin(), marker);
                    editor.set_caret(md::Pos{p.block, 0, 0, p.cell});
                } else {
                    const int cur_inl = p.inl;
                    const int cur_off = p.offset;
                    const int cur_text_len = static_cast<int>(inls[static_cast<size_t>(cur_inl)].text.size());
                    int insert_at = cur_inl;
                    if (cur_off > 0 && cur_off < cur_text_len) {
                        // Курсор внутри текста инлайна: разбиваем его и вставляем маркер между частями.
                        const md::Inline old = inls[static_cast<size_t>(cur_inl)];
                        md::Inline left = old;
                        left.text = old.text.substr(0, static_cast<size_t>(cur_off));
                        md::Inline right = old;
                        right.text = old.text.substr(static_cast<size_t>(cur_off));
                        inls.erase(inls.begin() + cur_inl);
                        inls.insert(inls.begin() + cur_inl, left);
                        inls.insert(inls.begin() + cur_inl + 1, marker);
                        inls.insert(inls.begin() + cur_inl + 2, right);
                        insert_at = cur_inl + 1;
                    } else if (cur_off >= cur_text_len) {
                        // Курсор в конце инлайна: маркер после этого инлайна.
                        insert_at = cur_inl + 1;
                        if (insert_at > static_cast<int>(inls.size())) insert_at = static_cast<int>(inls.size());
                        inls.insert(inls.begin() + insert_at, marker);
                    } else {
                        // cur_off == 0: маркер перед инлайном.
                        inls.insert(inls.begin() + insert_at, marker);
                    }
                    // Курсор сразу после маркера.
                    md::Pos np{p.block, insert_at, 0, p.cell};
                    editor.set_caret(np);
                }
                active_footnote_id = fid;
                footnote_editor_open = true;
                doc_dirty = true;
                mark_edited();
                ensure_caret_visible();
                history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            }
            block_button("codeblock", tr("Блок кода", "Code block"), cb.type == md::BlockType::CodeBlock,
                         md::BlockType::CodeBlock, 0, false);
            sep();

            if (editor.in_table() && tables_ok) {
                const std::vector<std::string> items = {
                    tr("+ строка", "+ row"), tr("- строка", "- row"),
                    tr("+ столбец", "+ column"), tr("- столбец", "- column"),
                    tr("Выровнять слева", "Align left"), tr("Выровнять по центру", "Align center"),
                    tr("Выровнять справа", "Align right"), tr("Удалить таблицу", "Delete table"),
                };
                const char* lbl = tr("Таблица ↓", "Table ↓");
                const Rect r = place_label(lbl);
                const int ch = !measuring ? ui::menu(ui, "table_menu", r, lbl, items, 240.0f) : -1;
                if (ch == 0) editor.table_add_row();
                if (ch == 1) editor.table_remove_row();
                if (ch == 2) editor.table_add_column();
                if (ch == 3) editor.table_remove_column();
                if (ch == 4) editor.table_set_align(md::TableAlign::Left);
                if (ch == 5) editor.table_set_align(md::TableAlign::Center);
                if (ch == 6) editor.table_set_align(md::TableAlign::Right);
                if (ch == 7) editor.table_remove();
                if (ch >= 0) mark_edited();
            } else if (widget_button("tbl_insert", tr("Таблица", "Table"), tables_ok)) {
                editor.insert_table(3, 2);
                mark_edited();
            }
            if (widget_button("vis_diag", tr("Диаграмма", "Diagram"))) diag_open_new();
            if (widget_button("vis_formula", tr("Формула", "Formula"))) formula_open_new();
            if (widget_button("vis_gde", tr("где …", "where …"))) {
                md::Block b;
                b.type = md::BlockType::Paragraph;
                b.inlines.push_back(md::Inline{
                    en() ? "where x — parameter name, unit;" : "где x — наименование величины, ед. изм.;",
                    0, ""});
                const int ins = std::clamp(editor.caret().block + 1, 0, static_cast<int>(doc.blocks.size()));
                doc.blocks.insert(doc.blocks.begin() + ins, std::move(b));
                editor.set_caret(md::Pos{ins, 0, 0, -1});
                mark_edited();
                ensure_caret_visible();
                history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            }
            if (widget_button("vis_chart", tr("График", "Chart"))) chart_open_new();
            sep();
            if (widget_button("rule", tr("Разрыв листа", "Page break"))) {
                editor.insert_rule();
                mark_edited();
                ensure_caret_visible();
            }
            sep();

            // Инструменты ГОСТ Р 2.105 (п. 5.1.1 — кегль 11..14 пт; п. 5.1.2 — интервал 1..2).
            {
                const std::vector<std::string> pt_items = {
                    tr("11 пт (ГОСТ 5.1.1)", "11 pt (GOST 5.1.1)"),
                    tr("12 пт (ГОСТ 5.1.1)", "12 pt (GOST 5.1.1)"),
                    tr("13 пт (ГОСТ 5.1.1)", "13 pt (GOST 5.1.1)"),
                    tr("14 пт (основной)", "14 pt (default)"),
                };
                const std::string pt_lbl = std::to_string(std::clamp(doc.meta.font_pt, 11, 14))
                                         + (en() ? " pt ↓" : " пт ↓");
                const Rect r = place_label(pt_lbl.c_str());
                const int ch = !measuring ? ui::menu(ui, "gost_pt_menu", r, pt_lbl.c_str(), pt_items, 200.0f) : -1;
                if (ch >= 0 && ch <= 3) {
                    doc.meta.font_pt = 11 + ch;
                    doc.meta.modified_meta = true;
                    doc_dirty = true;
                    mark_edited();
                }
            }
            {
                static const char* kLsLabels[6] = {"1,0", "1,15", "1,25", "1,5", "1,75", "2,0"};
                const std::vector<std::string> ls_items = {
                    tr("1,0 (одинарный)", "1.0 (single)"),
                    "1,15",
                    "1,25",
                    tr("1,5 (полуторный, ГОСТ)", "1.5 (GOST default)"),
                    "1,75",
                    tr("2,0 (двойной)", "2.0 (double)"),
                };
                const int lsi = std::clamp(doc.meta.line_spacing_idx, 0, 5);
                const std::string ls_lbl = std::string(tr("Инт. ", "Line ")) + kLsLabels[lsi] + " ↓";
                const Rect r = place_label(ls_lbl.c_str());
                const int ch = !measuring ? ui::menu(ui, "gost_ls_menu", r, ls_lbl.c_str(), ls_items, 220.0f) : -1;
                if (ch >= 0 && ch <= 5) {
                    doc.meta.line_spacing_idx = ch;
                    doc.meta.modified_meta = true;
                    doc_dirty = true;
                    mark_edited();
                }
            }
            if (widget_toggle("gost_title_btn", tr("Титул", "Title page"), doc.meta.has_title_page)) {
                doc.meta.has_title_page = !doc.meta.has_title_page;
                doc.meta.modified_meta = true;
                doc_dirty = true;
                mark_edited();
                if (doc.meta.has_title_page) scroll_y = 0.0f;
            }
            if (widget_toggle("gost_toc_btn", tr("Содержание", "Contents"), doc.meta.has_toc)) {
                doc.meta.has_toc = !doc.meta.has_toc;
                doc.meta.modified_meta = true;
                doc_dirty = true;
                mark_edited();
            }
            if (widget_toggle("gost_lri_btn", tr("ЛРИ", "Change sheet"), doc.meta.has_lri)) {
                doc.meta.has_lri = !doc.meta.has_lri;
                doc.meta.modified_meta = true;
                doc_dirty = true;
                mark_edited();
            }
            {
                const std::vector<std::string> num_items = {
                    doc.meta.auto_num_headings
                        ? tr("✓ Автонумерация разделов (1, 1.1)", "✓ Auto-number sections (1, 1.1)")
                        : tr("  Автонумерация разделов (1, 1.1)", "  Auto-number sections (1, 1.1)"),
                    !doc.meta.section_num_media
                        ? tr("• Сквозная: Табл. 1, Рис. 1, (1)", "• Global: Table 1, Fig. 1, (1)")
                        : tr("  Сквозная: Табл. 1, Рис. 1, (1)", "  Global: Table 1, Fig. 1, (1)"),
                    doc.meta.section_num_media
                        ? tr("• По разделам: Табл. 1.1, Рис. 1.1, (1.1)", "• By section: Table 1.1, Fig. 1.1, (1.1)")
                        : tr("  По разделам: Табл. 1.1, Рис. 1.1, (1.1)", "  By section: Table 1.1, Fig. 1.1, (1.1)"),
                    tr("+ Приложение (А, Б, В…)", "+ Appendix (A, B, C…)"),
                };
                const char* num_lbl = tr("Нумерация ↓", "Numbering ↓");
                const Rect r = place_label(num_lbl);
                const int ch = !measuring ? ui::menu(ui, "gost_num_menu", r, num_lbl, num_items, 290.0f) : -1;
                if (ch == 0) {
                    doc.meta.auto_num_headings = !doc.meta.auto_num_headings;
                    doc.meta.modified_meta = true;
                    doc_dirty = true;
                    mark_edited();
                } else if (ch == 1) {
                    doc.meta.section_num_media = false;
                    doc.meta.modified_meta = true;
                    doc_dirty = true;
                    mark_edited();
                } else if (ch == 2) {
                    doc.meta.section_num_media = true;
                    doc.meta.modified_meta = true;
                    doc_dirty = true;
                    mark_edited();
                } else if (ch == 3) {
                    // Определяем очередную букву приложения по ГОСТ Р 2.105 п. 6.3.5
                    static const char* kAppLetters[] = {
                        "А", "Б", "В", "Г", "Д", "Е", "Ж", "И", "К", "Л", "М", "Н",
                        "П", "Р", "С", "Т", "У", "Ф", "Х", "Ц", "Ш", "Щ", "Э", "Ю", "Я"
                    };
                    int app_count = 0;
                    for (const md::Block& blk : doc.blocks) {
                        if (blk.type == md::BlockType::Heading && !blk.inlines.empty()
                            && blk.inlines[0].text.rfind("Приложение", 0) == 0) {
                            ++app_count;
                        }
                    }
                    const char* letter = kAppLetters[app_count % 25];
                    md::Block h_app;
                    h_app.type = md::BlockType::Heading;
                    h_app.level = 1;
                    h_app.inlines.push_back(md::Inline{std::string("Приложение ") + letter + " (обязательное)", 0, ""});
                    const int ins = std::clamp(editor.caret().block + 1, 0, static_cast<int>(doc.blocks.size()));
                    doc.blocks.insert(doc.blocks.begin() + ins, std::move(h_app));
                    editor.set_caret(md::Pos{ins, 0, 0, -1});
                    doc_dirty = true;
                    mark_edited();
                    ensure_caret_visible();
                    history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
                }
            }
            sep();

            {
                std::vector<std::string> items = {
                    tr("Документ: Пояснительная записка (ПЗ, ГОСТ Р 2.106)", "Doc: Explanatory note (PZ, GOST R 2.106)"),
                    tr("Документ: Программа и методика испытаний (ПМ)", "Doc: Test program & procedure (PM)"),
                    tr("Документ: Расчёт (РР, ГОСТ Р 2.106)", "Doc: Calculation (RR, GOST R 2.106)"),
                    tr("Форма 1: Спецификация (во весь лист А4)", "Form 1: Specification (full-page A4)"),
                    tr("Форма 4: Ведомость ссылочных документов (ВД)", "Form 4: Referenced documents list (VD)"),
                    tr("Форма 8: Ведомость техпроекта (ТП / ЭП / ПТ)", "Form 8: Technical project list (TP)"),
                };
                const size_t doc_tpl_count = items.size();
                const char* lbl = tr("Шаблоны ↓", "Templates ↓");
                const Rect r = place_label(lbl);
                const int ch = !measuring ? ui::menu(ui, "templates_menu", r, lbl, items, 380.0f) : -1;
                if (ch >= 0 && static_cast<size_t>(ch) < doc_tpl_count) {
                    const md::GostMeta prev_meta = doc.meta;
                    if (ch == 0) {
                        md::import_markdown(
                            "# Введение\n\n"
                            "Настоящая пояснительная записка разработана на основании технического задания.\n\n"
                            "# Наименование и область применения\n\n"
                            "Изделие предназначено для применения в составе автоматизированных систем.\n\n"
                            "# Техническая характеристика\n\n"
                            "| Наименование показателя | Значение |\n"
                            "| --- | --- |\n"
                            "| 1 Напряжение питания, В | 24 |\n"
                            "| 2 Потребляемая мощность, Вт, не более | 15 |\n\n"
                            "# Описание и обоснование выбранной конструкции\n\n"
                            "Конструкция изделия выполнена в модульном исполнении.\n\n"
                            "# Расчеты, подтверждающие работоспособность и надежность конструкции\n\n"
                            "$$\nP = U \\cdot I\n$$\n\n"
                            "где U — напряжение питания, В;\n\n"
                            "# Описание организации работ с применением разрабатываемого изделия\n\n"
                            "Эксплуатация изделия осуществляется обслуживающим персоналом.\n\n"
                            "# Ожидаемые технико-экономические показатели\n\n"
                            "Применение изделия снижает трудоемкость обслуживания.\n",
                            true, doc);
                        doc.meta = prev_meta;
                        doc.meta.form_type = 0;
                        doc.meta.has_title_page = true;
                        doc.meta.has_toc = true;
                        doc.meta.has_lri = true;
                        doc.meta.code = "АБВГ.123456.001 ПЗ";
                        doc.meta.name = "Пояснительная записка";
                        doc.meta.title_doc_kind = "Пояснительная записка";
                    } else if (ch == 1) {
                        md::import_markdown(
                            "# Общие положения\n\n"
                            "Настоящая программа и методика испытаний устанавливает порядок проведения испытаний изделия.\n\n"
                            "# Общие требования к условиям, обеспечению и проведению испытаний\n\n"
                            "Испытания проводят в нормальных климатических условиях.\n\n"
                            "# Требования безопасности\n\n"
                            "К проведению испытаний допускается персонал, прошедший инструктаж.\n\n"
                            "# Определяемые показатели (характеристики) и точность их измерений\n\n"
                            "| Определяемый показатель | Номинальное значение | Допускаемая погрешность |\n"
                            "| --- | --- | --- |\n"
                            "| 1 Напряжение питания, В | 24,0 | ±0,5 |\n\n"
                            "# Режимы испытаний изделия\n\n"
                            "Испытания проводят в непрерывном режиме работы.\n\n"
                            "# Методы испытаний и измерений\n\n"
                            "Проверка параметров выполняется универсальными средствами измерений.\n\n"
                            "# Отчетность\n\n"
                            "Результаты испытаний оформляют протоколом испытаний.\n",
                            true, doc);
                        doc.meta = prev_meta;
                        doc.meta.form_type = 0;
                        doc.meta.has_title_page = true;
                        doc.meta.has_toc = true;
                        doc.meta.has_lri = true;
                        doc.meta.code = "АБВГ.123456.001 ПМ";
                        doc.meta.name = "Программа и методика испытаний";
                        doc.meta.title_doc_kind = "Программа и методика испытаний";
                    } else if (ch == 2) {
                        md::import_markdown(
                            "# Задача расчета\n\n"
                            "Определение расчетных параметров надежности и теплового режима изделия.\n\n"
                            "# Данные для расчета\n\n"
                            "Исходные данные приведены в техническом задании.\n\n"
                            "# Условия расчета\n\n"
                            "Расчет выполняется для стационарного режима работы.\n\n"
                            "# Расчет\n\n"
                            "$$\n\\eta = \\frac{P_2}{P_1}\n$$\n\n"
                            "где P_2 — полезная мощность, Вт;\n\n"
                            "# Заключение\n\n"
                            "Расчетные значения соответствуют установленным требованиям.\n",
                            true, doc);
                        doc.meta = prev_meta;
                        doc.meta.form_type = 0;
                        doc.meta.has_title_page = true;
                        doc.meta.has_toc = false;
                        doc.meta.has_lri = true;
                        doc.meta.code = "АБВГ.123456.001 РР";
                        doc.meta.name = "Расчет";
                        doc.meta.title_doc_kind = "Расчет";
                    } else if (ch == 3) {
                        md::import_markdown(
                            "| Формат | Зона | Поз. | Обозначение | Наименование | Кол. | Приме- чание |\n"
                            "| :---: | :---: | :---: | :--- | :--- | :---: | :--- |\n"
                            "|  |  |  |  | Документация |  |  |\n"
                            "| А4 |  |  | АБВГ.123456.001 СБ | Сборочный чертеж |  |  |\n"
                            "| А4 |  |  | АБВГ.123456.001 ПЗ | Пояснительная записка |  |  |\n"
                            "|  |  |  |  | Сборочные единицы |  |  |\n"
                            "| А4 |  | 1 | АБВГ.123456.010 | Корпус | 1 |  |\n"
                            "|  |  |  |  | Детали |  |  |\n"
                            "| А4 |  | 2 | АБВГ.711111.001 | Крышка | 1 |  |\n"
                            "| А4 |  | 3 | АБВГ.711111.002 | Втулка | 2 |  |\n"
                            "|  |  |  |  | Стандартные изделия |  |  |\n"
                            "|  |  | 4 |  | Винт М4-6g×12.58 ГОСТ 17473-80 | 4 |  |\n"
                            "|  |  | 5 |  | Шайба 4.65Г ГОСТ 6402-70 | 4 |  |\n",
                            true, doc);
                        doc.meta = prev_meta;
                        doc.meta.form_type = 1;
                        doc.meta.has_title_page = false;
                        doc.meta.has_toc = false;
                        doc.meta.code = "АБВГ.123456.001";
                        doc.meta.name = "Изделие";
                    } else if (ch == 4) {
                        md::import_markdown(
                            "| № строки | Обозначение | Наименование |\n"
                            "| :---: | :--- | :--- |\n"
                            "| 1 |  | Документы государственные (национальные) |\n"
                            "| 2 | ГОСТ Р 2.105-2019 | ЕСКД. Общие требования к текстовым документам |\n"
                            "| 3 | ГОСТ Р 2.106-2019 | ЕСКД. Текстовые документы |\n"
                            "| 4 | ГОСТ 2.104-2006 | ЕСКД. Основные надписи |\n",
                            true, doc);
                        doc.meta = prev_meta;
                        doc.meta.form_type = 2;
                        doc.meta.has_title_page = false;
                        doc.meta.has_toc = false;
                        doc.meta.code = "АБВГ.123456.001 ВД";
                        doc.meta.name = "Ведомость ссылочных документов";
                    } else if (ch == 5) {
                        md::import_markdown(
                            "| № строки | Формат | Обозначение | Наименование | Кол. листов | № экз. | Приме- чание |\n"
                            "| :---: | :---: | :--- | :--- | :---: | :---: | :--- |\n"
                            "| 1 |  |  | Документация общая |  |  |  |\n"
                            "| 2 |  |  | Вновь разработанная |  |  |  |\n"
                            "| 3 | А4 | АБВГ.123456.001 ПЗ | Пояснительная записка | 12 | 1 |  |\n"
                            "| 4 | А1 | АБВГ.123456.001 ВО | Чертеж общего вида | 1 | 1 |  |\n",
                            true, doc);
                        doc.meta = prev_meta;
                        doc.meta.form_type = 3;
                        doc.meta.has_title_page = false;
                        doc.meta.has_toc = false;
                        doc.meta.code = "АБВГ.123456.001 ТП";
                        doc.meta.name = "Ведомость технического проекта";
                    }
                    doc.meta.modified_meta = true;
                    editor.set_caret(md::Pos{0, 0, 0});
                    scroll_y = 0.0f;
                    doc_dirty = true;
                    mark_edited();
                    history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
                }
            }
        };

        body();
        row_ends.push_back(x - 2.0f);
        measuring = false;
        const int rows = row + 1;
        toolbar_h = top + (rows - 1) * row_step + btn_h + 8.0f;

        ui::fill_rect(ui, {0, 0, static_cast<float>(width), toolbar_h}, ui.palette.window_bg);
        ui::fill_rect(ui, {0, toolbar_h - 1.0f, static_cast<float>(width), 1.0f}, ui.palette.panel_border);
        draw_titlebar();
        body();
    }


    void draw_titlebar() {
        using namespace ui;
        const float pad = 8.0f;
        const float h = 28.0f;
        const float y = (kTitleH - h) / 2.0f;
        const float w_px = static_cast<float>(width);
        const float cap_w = 46.0f;

        caption_rects[0] = Rect{w_px - 3.0f * cap_w, 0.0f, cap_w, kTitleH};
        caption_rects[1] = Rect{w_px - 2.0f * cap_w, 0.0f, cap_w, kTitleH};
        caption_rects[2] = Rect{w_px - cap_w, 0.0f, cap_w, kTitleH};

        const float help_w = 34.0f;
        help_rect = Rect{caption_rects[0].x - pad - help_w, y, help_w, h};

        const char* lang_label = en() ? "EN" : "RU";
        const float lang_w = 42.0f;
        lang_rect = Rect{help_rect.x - 6.0f - lang_w, y, lang_w, h};

        const char* theme_label = light_theme ? tr("Светлая", "Light") : tr("Тёмная", "Dark");
        const float theme_w = std::max(text_width(fonts.regular, tr("Светлая", "Light")),
                                       text_width(fonts.regular, tr("Тёмная", "Dark"))) + 26.0f;
        theme_rect = Rect{lang_rect.x - 6.0f - theme_w, y, theme_w, h};

        dialect_rect = Rect{};
        const char* file_lbl = tr("Файл", "File");
        file_rect = Rect{pad, y, std::max(34.0f, text_width(fonts.regular, file_lbl) + 28.0f), h};

        std::vector<std::string> file_items;
        if (file_export_mode) {
            file_items = {tr("← Назад", "< Back"), "DOCX", "PDF", "HTML", "ODT", "EPUB"};
        } else {
            file_items = en() ? kFileItemsEn : kFileItemsRu;
            for (const std::string& path : recent_files) {
                file_items.push_back("   " + std::filesystem::u8path(path).filename().u8string());
            }
        }
        const int choice = menu(ui, "file", file_rect, file_lbl, file_items, 270.0f);
        if (file_export_mode) {
            if (choice == 0) {
                file_export_mode = false;
                ui.open_popup = "file";
            } else if (choice >= 1) {
                file_export_mode = false;
                export_format(static_cast<size_t>(choice - 1));
            }
        } else {
            if (choice == 0) new_document();
            if (choice == 1) open();
            if (choice == 2) save();
            if (choice == 3) save_as();
            if (choice == 4) welcome_open = true;
            if (choice == 5) toggle_outline();
            if (choice == 6) {
                file_export_mode = true;
                ui.open_popup = "file";
            }
            if (choice >= 7 && choice - 7 < static_cast<int>(recent_files.size())) {
                const std::string path = recent_files[static_cast<size_t>(choice - 7)];
                if (open_from_path(path, false, true) == OpenResult::Error) {
                    recent_files.erase(std::remove(recent_files.begin(), recent_files.end(), path),
                                       recent_files.end());
                    save_settings();
                }
            }
        }
        if (file_export_mode && ui.open_popup != "file") file_export_mode = false;

        if (button(ui, "theme_btn", theme_rect, theme_label)) {
            toggle_theme();
        }
        if (button(ui, "lang_btn", lang_rect, lang_label)) {
            toggle_lang();
        }
        if (button(ui, "welcome_btn", help_rect, "?")) {
            welcome_open = !welcome_open;
        }

        const float left_end = file_rect.x + file_rect.w + 12.0f;
        const float right_start = theme_rect.x - 12.0f;
        const float avail = right_start - left_end;
        if (avail > 40.0f) {
            const std::string title = fit_text(fonts.regular,
                                               (modified ? "* " : "") + document_name(), avail);
            ui::draw_text(ui, fonts.regular, title,
                      left_end + (avail - text_width(fonts.regular, title)) / 2.0f,
                      y + (h - text_height(fonts.regular)) / 2.0f, ui.palette.text_dim);
        }

        if (button(ui, "win_min", caption_rects[0], "")) SDL_MinimizeWindow(window);
        const bool maximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
        if (button(ui, "win_max", caption_rects[1], "")) {
            if (maximized) {
                SDL_RestoreWindow(window);
            } else {
                SDL_MaximizeWindow(window);
            }
        }
        if (button(ui, "win_close", caption_rects[2], "")) request_quit();

        // Icons are drawn, not typeset, so they look the same with any font.
        const float cy = kTitleH / 2.0f;
        const float min_cx = caption_rects[0].x + cap_w / 2.0f;
        const float max_cx = caption_rects[1].x + cap_w / 2.0f;
        const float close_cx = caption_rects[2].x + cap_w / 2.0f;
        ui::fill_rect(ui, {min_cx - 5.0f, cy, 10.0f, 1.0f}, ui.palette.text);
        ui::stroke_rect(ui, {max_cx - 5.0f, cy - 5.0f, 10.0f, 10.0f}, ui.palette.text);
        const float x_w = text_width(fonts.regular, "×");
        ui::draw_text(ui, fonts.regular, "×", close_cx - x_w / 2.0f,
                  cy - text_height(fonts.regular) / 2.0f, ui.palette.text);
    }

    // Items flow left to right and wrap onto new rows when the window is too
    

    bool dispatch(const SDL_Event& e) {
        md::EditKind kind = md::EditKind::Other;
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            kind = md::EditKind::Typing;
        } else if (e.type == SDL_EVENT_KEY_DOWN
                   && (e.key.key == SDLK_BACKSPACE || e.key.key == SDLK_DELETE)) {
            kind = md::EditKind::Deleting;
        }
        const bool running = handle_event(e);
        // Any change to the text marks the document as modified, including ones
        // that have no dedicated handler, such as adding a table row with Tab.
        if (history.commit(doc, editor.caret(), editor.anchor(), kind)) {
            mark_edited();
        }
        return running;
    }
void diag_select(int index, bool edge) {
        diag_sel = index;
        diag_sel_edge = edge;
        std::string* t = diag_sel_text();
        diag_off = t != nullptr ? static_cast<int>(t->size()) : 0;
    }

    void diag_open_new() {
        formula_editor_open = false;
        chart_editor_open = false;
        diag_editor_open = true;
        diag_block = -1;
        diag_edit = md::diag_new();
        diag_select(0, false);
        status = tr("Редактор диаграмм: выберите элемент и вводите текст",
                    "Diagram Editor: select an element and type text");
    }

    // Open the diagram editor for an existing block. Returns false when the
    // code has no visual form: that block is left untouched, so nothing the
    // user cannot see is ever lost.
    bool diag_open_block(int block_index) {
        if (block_index < 0 || block_index >= static_cast<int>(doc.blocks.size())) {
            return false;
        }
        const md::Block& b = doc.blocks[block_index];
        std::string code;
        for (const md::Inline& inl : b.inlines) code += inl.text;
        // Charts have their own editor.
        if (md::is_chart_text(code)) return false;
        bool ok = true;
        md::DiagModel model = md::diag_from_text(code, &ok);
        if (!ok) {
            status = tr("Диаграмма не поддерживается визуальным редактором",
                        "Diagram is not supported by visual editor");
            return false;
        }
        formula_editor_open = false;
        chart_editor_open = false;
        diag_editor_open = true;
        diag_block = block_index;
        diag_edit = std::move(model);
        if (diag_edit.nodes.empty() && diag_edit.edges.empty()) {
            diag_edit = md::diag_new();
        }
        diag_select(0, false);
        status = tr("Редактор диаграмм: клик по элементу — выбор, ввод — текст",
                    "Diagram Editor: click element to select, type to edit label");
        return true;
    }

    void diag_add_node() {
        md::DiagNode n;
        n.id = md::diag_new_id(diag_edit);
        diag_edit.nodes.push_back(std::move(n));
        diag_select(static_cast<int>(diag_edit.nodes.size()) - 1, false);
    }

    // A new node connected to the selected (or last) one: one click builds
    // the next step of the diagram.
    void diag_add_edge() {
        if (diag_edit.nodes.empty()) diag_add_node();
        int from = -1;
        if (!diag_sel_edge && diag_sel >= 0
            && diag_sel < static_cast<int>(diag_edit.nodes.size())) {
            from = diag_sel;
        } else {
            from = static_cast<int>(diag_edit.nodes.size()) - 1;
        }
        md::DiagNode n;
        n.id = md::diag_new_id(diag_edit);
        diag_edit.nodes.push_back(std::move(n));
        const int to = static_cast<int>(diag_edit.nodes.size()) - 1;
        md::DiagEdge e;
        e.from = diag_edit.nodes[static_cast<size_t>(from)].id;
        e.to = diag_edit.nodes[static_cast<size_t>(to)].id;
        diag_edit.edges.push_back(std::move(e));
        diag_select(to, false);
    }

    // Removes the selected node (with its edges) or the selected edge.
    void diag_delete_element() {
        if (diag_sel_edge) {
            if (diag_sel >= 0 && diag_sel < static_cast<int>(diag_edit.edges.size())) {
                diag_edit.edges.erase(diag_edit.edges.begin() + diag_sel);
            }
            if (diag_sel < static_cast<int>(diag_edit.edges.size())) {
                diag_select(diag_sel, true); // the next edge slid into place
            } else if (!diag_edit.nodes.empty()) {
                diag_select(static_cast<int>(diag_edit.nodes.size()) - 1, false);
            } else {
                diag_select(-1, false);
            }
        } else {
            if (diag_sel >= 0 && diag_sel < static_cast<int>(diag_edit.nodes.size())) {
                const std::string id = diag_edit.nodes[static_cast<size_t>(diag_sel)].id;
                diag_edit.nodes.erase(diag_edit.nodes.begin() + diag_sel);
                diag_edit.edges.erase(
                    std::remove_if(diag_edit.edges.begin(), diag_edit.edges.end(),
                                   [&](const md::DiagEdge& e2) {
                                       return e2.from == id || e2.to == id;
                                   }),
                    diag_edit.edges.end());
            }
            if (diag_edit.nodes.empty() && diag_edit.edges.empty()) {
                diag_select(-1, false);
            } else if (!diag_edit.nodes.empty()) {
                diag_select(std::min(diag_sel, static_cast<int>(diag_edit.nodes.size()) - 1),
                            false);
            } else {
                diag_select(0, true);
            }
        }
        status = tr("Элемент удалён", "Element deleted");
    }

    void diag_set_shape(md::DiagShape shape) {
        if (diag_sel_edge || diag_sel < 0
            || diag_sel >= static_cast<int>(diag_edit.nodes.size())) {
            return;
        }
        diag_edit.nodes[static_cast<size_t>(diag_sel)].shape = shape;
    }

    // Typing goes straight into the selected element's label.
    void diag_insert_text(const char* sym) {
        std::string* t = diag_sel_text();
        if (t == nullptr) {
            diag_add_node();
            t = diag_sel_text();
            if (t == nullptr) return;
        }
        t->insert(static_cast<size_t>(diag_off), sym);
        diag_off += static_cast<int>(std::string(sym).size());
    }

    // Backspace deletes one utf-8 code point of the selected label.
    void diag_backspace() {
        std::string* t = diag_sel_text();
        if (t == nullptr || diag_off <= 0) return;
        size_t p = static_cast<size_t>(diag_off);
        --p;
        while (p > 0 && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) --p;
        t->erase(p, static_cast<size_t>(diag_off) - p);
        diag_off = static_cast<int>(p);
    }

    // Tab walks the elements in drawing order.
    void diag_move_slot(int dir) {
        if (diag_slots.empty()) return;
        int cur = 0;
        for (size_t i = 0; i < diag_slots.size(); ++i) {
            if (diag_slots[i].index == diag_sel && diag_slots[i].edge == diag_sel_edge) {
                cur = static_cast<int>(i);
                break;
            }
        }
        int next = cur + dir;
        if (next < 0) next = static_cast<int>(diag_slots.size()) - 1;
        if (next >= static_cast<int>(diag_slots.size())) next = 0;
        const md::diagdraw::Slot& s = diag_slots[static_cast<size_t>(next)];
        diag_select(s.index, s.edge);
    }

    // Writes the picture back to the block as mermaid code — the only place
    // the code exists besides the file itself.
    void diag_apply() {
        const std::string code = md::diag_to_mermaid(diag_edit);
        if (diag_block >= 0 && diag_block < static_cast<int>(doc.blocks.size())) {
            md::Block& b = doc.blocks[static_cast<size_t>(diag_block)];
            b.lang = "mermaid";
            b.inlines.clear();
            b.inlines.push_back(md::Inline{code, 0, ""});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Диаграмма обновлена", "Diagram updated");
        } else {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = "mermaid";
            b.inlines.push_back(md::Inline{code, 0, ""});
            const int at = std::max(0, editor.caret().block + 1);
            const int idx = std::min(at, static_cast<int>(doc.blocks.size()));
            doc.blocks.insert(doc.blocks.begin() + idx, std::move(b));
            editor.set_caret(md::Pos{idx, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Диаграмма вставлена", "Diagram inserted");
        }
        doc_dirty = true;
        modified = true;
        diag_editor_open = false;
    }

    void draw_diag_editor() {
        if (!diag_editor_open) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float ow = w - 40.0f;
        const float oh = std::min(430.0f, h - toolbar_h - 40.0f);
        const float ox = 20.0f;
        const float oy = toolbar_h + 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold,
                  diag_block >= 0 ? tr("Редактор диаграммы", "Edit Diagram")
                                  : tr("Новая диаграмма", "New Diagram"),
                  ox + 16.0f, oy + 10.0f, ui.palette.text);

        const bool flow = diag_edit.kind == md::DiagKind::Flowchart;
        const bool node_sel = !diag_sel_edge && diag_sel >= 0
            && diag_sel < static_cast<int>(diag_edit.nodes.size());

        // --- element buttons (wrapping rows like the formula editor) ---
        float tx = ox + 18.0f;
        float ty = oy + 40.0f;
        auto row_button = [&](const char* id, const char* label, bool enabled,
                              const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (button(ui, id, {tx, ty, bw, 30.0f}, label, enabled)) act();
            tx += bw + 6.0f;
        };
        auto row_toggle = [&](const char* id, const char* label, bool active,
                              bool enabled, const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (toggle_button(ui, id, {tx, ty, bw, 30.0f}, label, active, enabled)) act();
            tx += bw + 6.0f;
        };

        row_button("d_node", tr("Узел", "Node"), true, [&]() { diag_add_node(); });
        row_button("d_edge", tr("Связь", "Edge"), true, [&]() { diag_add_edge(); });
        const bool shapes_on = node_sel && flow;
        row_button("d_shape_box", tr("Прямоугольник", "Rectangle"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Box); });
        row_button("d_shape_round", tr("Скруглённый", "Rounded"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Rounded); });
        row_button("d_shape_diamond", tr("Ромб", "Diamond"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Diamond); });
        row_button("d_shape_ellipse", tr("Эллипс", "Ellipse"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Ellipse); });
        row_button("d_shape_stadium", tr("Стадион", "Stadium"), shapes_on,
                   [&]() { diag_set_shape(md::DiagShape::Stadium); });
        ty += 34.0f;

        // --- kind and direction ---
        tx = ox + 18.0f;
        row_toggle("d_flow", tr("Схема потока", "Flowchart"), flow, true,
                   [&]() { diag_edit.kind = md::DiagKind::Flowchart; });
        row_toggle("d_seq", tr("Последовательность", "Sequence"), !flow, true,
                   [&]() { diag_edit.kind = md::DiagKind::Sequence; });
        row_toggle("d_dir",
                   flow ? (diag_edit.lr ? tr("Направление: вправо →", "Direction: Right →")
                                        : tr("Направление: вниз ↓", "Direction: Down ↓"))
                        : tr("Направление: вправо →", "Direction: Right →"),
                   diag_edit.lr, flow, [&]() { diag_edit.lr = !diag_edit.lr; });
        ty += 34.0f;

        // --- diagram canvas ---
        const float canvas_h = std::max(64.0f, oh - (ty - oy) - 56.0f);
        const Rect canvas{ox + 18.0f, ty, ow - 36.0f, canvas_h};
        ui::fill_rect(ui, canvas, ui.palette.window_bg);
        ui::stroke_rect(ui, canvas, ui.palette.panel_border);

        // Click on an element (checked against the previous frame's rects).
        if (ui.input.released && contains(canvas, ui.input.mouse_x, ui.input.mouse_y)) {
            for (const auto& slot : diag_slots) {
                if (contains(slot.rect, ui.input.mouse_x, ui.input.mouse_y)) {
                    diag_select(slot.index, slot.edge);
                    break;
                }
            }
        }

        // Render the picture; collect clickable elements.
        diag_slots.clear();
        md::diagdraw::Selection sel;
        sel.index = diag_sel;
        sel.edge = diag_sel_edge;
        md::diagdraw::render(&ui, fontbook, diag_edit,
                             canvas.x + 14.0f, canvas.y + 10.0f, canvas.w - 28.0f,
                             diag_sel >= 0 ? &sel : nullptr, &diag_slots);
        ui::draw_text(ui, fonts.regular,
                  tr("Клик по элементу — выбор, ввод — текст, Tab — следующий, Delete — удалить элемент, Esc — закрыть",
                     "Click element to select, type to edit, Tab — next, Delete — remove element, Esc — close"),
                  canvas.x + 4.0f, canvas.y + canvas_h - 17.0f, ui.palette.text_dim);

        // --- apply / delete / cancel ---
        const float by = oy + oh - 40.0f;
        if (button(ui, "d_apply", {ox + 18.0f, by, 130.0f, 30.0f},
                   diag_block >= 0 ? tr("Сохранить", "Save") : tr("Вставить", "Insert"))) {
            diag_apply();
        }
        if (button(ui, "d_delete", {ox + 156.0f, by, 150.0f, 30.0f},
                   tr("Удалить элемент", "Delete Element"), diag_sel >= 0)) {
            diag_delete_element();
        }
        if (button(ui, "d_cancel", {ox + 314.0f, by, 110.0f, 30.0f}, tr("Отмена", "Cancel"))) {
            diag_editor_open = false;
            status = tr("Редактор диаграмм закрыт", "Diagram editor closed");
        }
    }
void handle_key(const SDL_KeyboardEvent& key) {
        const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
        const bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
        using Move = md::Editor::Move;
        switch (key.key) {
            case SDLK_LEFT: editor.move(layout, fontbook, Move::Left, shift); ensure_caret_visible(); break;
            case SDLK_RIGHT: editor.move(layout, fontbook, Move::Right, shift); ensure_caret_visible(); break;
            case SDLK_UP: editor.move(layout, fontbook, Move::Up, shift); ensure_caret_visible(); break;
            case SDLK_DOWN: editor.move(layout, fontbook, Move::Down, shift); ensure_caret_visible(); break;
            case SDLK_HOME:
                editor.move(layout, fontbook, ctrl ? Move::DocStart : Move::LineStart, shift);
                ensure_caret_visible();
                break;
            case SDLK_END:
                editor.move(layout, fontbook, ctrl ? Move::DocEnd : Move::LineEnd, shift);
                ensure_caret_visible();
                break;
            case SDLK_Z:
                if (ctrl) apply_history(shift);
                break;
            case SDLK_Y:
                if (ctrl) apply_history(true);
                break;
            case SDLK_N:
                if (ctrl) new_document();
                break;
            case SDLK_S:
                if (ctrl && shift) save_as();
                else if (ctrl) save();
                break;
            case SDLK_BACKSPACE: editor.backspace(); mark_edited(); ensure_caret_visible(); break;
            case SDLK_DELETE: editor.delete_forward(); mark_edited(); ensure_caret_visible(); break;
            case SDLK_RETURN: { editor.newline(); mark_edited(); ensure_caret_visible(); break; }
            case SDLK_KP_ENTER: { editor.newline(); mark_edited(); ensure_caret_visible(); break; }
            case SDLK_A: if (ctrl) editor.select_all(); break;
            case SDLK_C: if (ctrl) editor.copy_selection(); break;
            case SDLK_X: if (ctrl) { editor.cut_selection(); mark_edited(); } break;
            case SDLK_V: if (ctrl) { editor.paste(); mark_edited(); ensure_caret_visible(); } break;
            case SDLK_B: if (ctrl) { editor.toggle_style(md::StyleBold); mark_edited(); } break;
            case SDLK_I: if (ctrl) { editor.toggle_style(md::StyleItalic); mark_edited(); } break;
            case SDLK_E:
                if (ctrl && shift) {
                    editor.set_block(md::BlockType::CodeBlock);
                    mark_edited();
                    ensure_caret_visible();
                } else if (ctrl) {
                    editor.toggle_style(md::StyleCode);
                    mark_edited();
                }
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
                if (ctrl) {
                    editor.set_block(md::BlockType::Heading,
                                     static_cast<int>(key.key - SDLK_0));
                    mark_edited();
                }
                break;
            case SDLK_K:
                if (ctrl && !shift) link_open_dialog();
                break;
            case SDLK_L:
                if (ctrl && shift) { editor.set_block(md::BlockType::ListItem); mark_edited(); }
                break;
            case SDLK_O:
                if (ctrl && shift) {
                    editor.set_block(md::BlockType::ListItem, 0, true);
                    mark_edited();
                } else if (ctrl) {
                    open();
                }
                break;
            case SDLK_Q:
                if (ctrl && shift) { editor.set_block(md::BlockType::Quote); mark_edited(); }
                break;
            case SDLK_F:
                if (ctrl) open_find(false);
                break;
            case SDLK_H:
                if (ctrl) open_find(true);
                break;
            case SDLK_T:
                if (ctrl && shift) toggle_outline();
                break;
            case SDLK_R:
                if (ctrl && shift) { editor.insert_rule(); mark_edited(); }
                break;
            case SDLK_TAB:
                if (editor.in_table()) {
                    editor.table_step(!shift);
                } else if (editor.code_tab(shift)) {
                    mark_edited();
                    ensure_caret_visible();
                } else if (editor.indent(shift ? -1 : 1)) {
                    mark_edited();
                }
                break;
            case SDLK_M:
                if (ctrl && shift) {
                    if (!editor.in_table()) {
                        editor.insert_text("$...$");
                        md::Pos p = editor.caret();
                        if (p.offset >= 4) p.offset -= 4;
                        editor.set_caret(p);
                        history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
                    }
                }
                break;
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
                if (ctrl) zoom_to(zoom + 0.1f, doc_viewport.y + doc_viewport.h * 0.5f);
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                if (ctrl) zoom_to(zoom - 0.1f, doc_viewport.y + doc_viewport.h * 0.5f);
                break;
            case SDLK_0:
            case SDLK_KP_0:
                if (ctrl) zoom_to(1.0f, doc_viewport.y + doc_viewport.h * 0.5f);
                break;
            default: break;
        }
    }

    // Changes the document zoom, arming an anchor so the document point under
    // the cursor stays put after the relayout.
    void zoom_to(float new_zoom, float cursor_y) {
        new_zoom = std::clamp(new_zoom, 0.3f, 2.5f);
        if (new_zoom == zoom) return;
        if (doc_viewport.h > 0.0f
            && cursor_y >= doc_viewport.y && cursor_y <= doc_viewport.y + doc_viewport.h) {
            zoom_anchor_doc_y = scroll_y + (cursor_y - doc_viewport.y);
            zoom_anchor_view_y = cursor_y - doc_viewport.y;
            zoom_anchor_old_zoom = text_zoom();
        } else {
            zoom_anchor_doc_y = -1.0f;
        }
        zoom = new_zoom;
        save_settings();
        fontbook.set_zoom(text_zoom());
        doc_dirty = true; // force a relayout with the new zoom
    }

    void editor_pointer(float x, float y, bool extend) {
        find_focus = false;
        editor.click(layout, fontbook, x - doc_viewport.x,
                     y - doc_viewport.y + scroll_y, extend);
        ensure_caret_visible();
    }

    // ---------- links ----------

    static bool is_image_path(const std::string& path) {
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos) return false;
        std::string ext = path.substr(dot + 1);
        for (char& c : ext) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif"
            || ext == "webp" || ext == "bmp" || ext == "svg";
    }

    void link_open_dialog() {
        if (editor.caret_block().type == md::BlockType::CodeBlock) {
            status = tr("В блоке кода ссылку не вставить", "Links do not go into code blocks");
            return;
        }
        link_text_value = editor.has_selection() ? editor.selected_text() : std::string();
        if (link_text_value.find('\n') != std::string::npos) link_text_value.clear();
        link_href_value.clear();
        link_field = link_text_value.empty() ? 0 : 1;
        link_open = true;
        find_focus = false;
    }

    // A picked file is stored relative to the document when it lies in the same folder or below.
    std::string link_path_for(const std::string& picked) const {
        if (file_path.empty()) return picked;
        namespace fs = std::filesystem;
        const fs::path base = fs::u8path(file_path).parent_path();
        const fs::path rel = fs::u8path(picked).lexically_relative(base);
        const std::string text = rel.generic_u8string();
        if (rel.empty() || text.rfind("..", 0) == 0) return picked;
        return text;
    }

    void link_pick_file() {
        const std::string picked = dialogs::open_any_file();
        if (picked.empty()) return;
        link_href_value = link_path_for(picked);
        if (link_text_value.empty()) {
            link_text_value = std::filesystem::u8path(picked).filename().u8string();
        }
    }

    void link_apply() {
        std::string href = link_href_value;
        while (!href.empty() && href.front() == ' ') href.erase(0, 1);
        while (!href.empty() && href.back() == ' ') href.pop_back();
        if (href.empty()) {
            status = tr("Укажите адрес ссылки", "Enter a link address");
            return;
        }
        std::string text = link_text_value;
        if (text.empty()) {
            text = md::is_web_link(href) ? href : std::filesystem::u8path(href).filename().u8string();
        }
        if (text.empty()) text = href;
        editor.insert_link(text, href, is_image_path(href));
        link_open = false;
        mark_edited();
        ensure_caret_visible();
        history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
        status = tr("Ссылка вставлена", "Link inserted");
    }

    void link_key(SDL_Keycode key, SDL_Keymod mod) {
        std::string& field = link_field == 0 ? link_text_value : link_href_value;
        switch (key) {
            case SDLK_ESCAPE:
                link_open = false;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                link_apply();
                break;
            case SDLK_TAB:
                link_field = 1 - link_field;
                break;
            case SDLK_BACKSPACE:
                if (!field.empty()) {
                    size_t start = field.size() - 1;
                    while (start > 0 && (static_cast<unsigned char>(field[start]) & 0xC0) == 0x80) --start;
                    field.erase(start);
                }
                break;
            case SDLK_V:
                if (mod & SDL_KMOD_CTRL) {
                    char* clip = SDL_GetClipboardText();
                    std::string t = clip ? clip : "";
                    SDL_free(clip);
                    for (char& c : t) {
                        if (c == '\n' || c == '\r') c = ' ';
                    }
                    field += t;
                }
                break;
            default:
                break;
        }
    }

    // Local files go to the system through a file:// address; Windows takes the path itself.
    static std::string file_url(const std::filesystem::path& p) {
        const std::string utf8 = std::filesystem::absolute(p).u8string();
#ifdef _WIN32
        return utf8;
#else
        std::string out = "file://";
        for (const unsigned char c : utf8) {
            const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '.' || c == '_' || c == '~';
            if (plain) {
                out += static_cast<char>(c);
            } else {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned>(c));
                out += buf;
            }
        }
        return out;
#endif
    }

    void open_link(const std::string& href) {
        if (md::is_web_link(href)) {
            SDL_OpenURL(href.c_str());
            return;
        }
        namespace fs = std::filesystem;
        fs::path target = fs::u8path(href);
        if (target.is_relative() && !file_path.empty()) {
            target = fs::u8path(file_path).parent_path() / target;
        }
        std::error_code ec;
        if (!fs::exists(target, ec)) {
            status = tr("Файл не найден", "File not found");
            return;
        }
        SDL_OpenURL(file_url(target).c_str());
    }

    // The link under a window point, if any.
    bool link_at(float x, float y, std::string& href) {
        if (!ui::contains(doc_viewport, x, y)) return false;
        const md::Pos p = md::hit_test(doc, layout, fontbook, x - doc_viewport.x,
                                       y - doc_viewport.y + scroll_y);
        if (p.block < 0 || p.block >= static_cast<int>(doc.blocks.size())) return false;
        const std::vector<md::Inline>& inls = md::inlines_of(doc.blocks[p.block], p.cell);
        if (p.inl < 0 || p.inl >= static_cast<int>(inls.size())) return false;
        const md::Inline& inl = inls[static_cast<size_t>(p.inl)];
        if (!(inl.style & md::StyleLink) || inl.href.empty()) return false;
        href = inl.href;
        return true;
    }

    // Ctrl over a link shows a hand: Ctrl+click opens it.
    void update_link_cursor() {
        std::string href;
        const bool over = (SDL_GetModState() & SDL_KMOD_CTRL) && !link_open && !welcome_open
            && !pandoc_help_open && link_at(ui.input.mouse_x, ui.input.mouse_y, href);
        SDL_SetCursor(over && hand_cursor ? hand_cursor : SDL_GetDefaultCursor());
    }

    void draw_link_dialog() {
        if (!link_open) return;
        using namespace ui;
        const float w = std::min(520.0f, static_cast<float>(width) - 32.0f);
        const float h = 214.0f;
        const Rect bg{(static_cast<float>(width) - w) / 2.0f, (static_cast<float>(height) - h) / 2.0f, w, h};
        fill_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)}, SDL_Color{0, 0, 0, 145});
        fill_rounded_rect(ui, bg, 10.0f, ui.palette.panel_bg);
        stroke_rect(ui, bg, ui.palette.panel_border);
        draw_text(ui, fonts.bold, tr("Вставить ссылку", "Insert Link"),
                  bg.x + 20.0f, bg.y + 14.0f, ui.palette.text);

        const float fx = bg.x + 20.0f;
        const float fw = w - 40.0f;
        draw_text(ui, fonts.regular, tr("Текст", "Text"), fx, bg.y + 46.0f, ui.palette.text_dim);
        link_text_box = Rect{fx, bg.y + 66.0f, fw, 30.0f};
        draw_find_field(link_text_box, link_text_value, tr("Что показать", "What to show"), link_field == 0);
        draw_text(ui, fonts.regular, tr("Адрес или файл", "Address or file"),
                  fx, bg.y + 104.0f, ui.palette.text_dim);
        link_href_box = Rect{fx, bg.y + 124.0f, fw - 116.0f, 30.0f};
        draw_find_field(link_href_box, link_href_value, "https://", link_field == 1);

        if (button(ui, "lnk_file", Rect{fx + fw - 108.0f, bg.y + 124.0f, 108.0f, 30.0f},
                   tr("Файл…", "File…"))) {
            link_pick_file();
        }
        if (button(ui, "lnk_ok", Rect{bg.x + w - 150.0f, bg.y + h - 46.0f, 130.0f, 30.0f},
                   tr("Вставить", "Insert"))) {
            link_apply();
        }
        if (button(ui, "lnk_cancel", Rect{bg.x + w - 290.0f, bg.y + h - 46.0f, 130.0f, 30.0f},
                   tr("Отмена", "Cancel"))) {
            link_open = false;
        }
        link_card = bg;
    }

    // ---------- find and replace ----------

    std::string& find_text() { return find_field == 0 ? find_query : find_replacement; }

    void open_find(bool replace) {
        if (editor.has_selection()) {
            const std::string sel = editor.selected_text();
            if (!sel.empty() && sel.find('\n') == std::string::npos) find_query = sel;
        }
        find_open = true;
        find_focus = true;
        find_replace_mode = replace;
        find_field = 0;
        find_query_changed();
    }

    void close_find() {
        find_open = false;
        find_focus = false;
        find_matches.clear();
        find_current = -1;
    }

    // Recomputes the matches and selects the first one at or after the start
    // of the selection, so the search follows what is being typed.
    void find_query_changed() {
        find_matches = md::find_all(doc, find_query);
        find_current = -1;
        const md::Pos from = editor.has_selection() ? editor.selection().first : editor.caret();
        for (int i = 0; i < static_cast<int>(find_matches.size()); ++i) {
            if (!(find_matches[i].begin < from)) {
                find_current = i;
                break;
            }
        }
        if (find_current < 0 && !find_matches.empty()) find_current = 0;
        if (find_current >= 0) find_select(find_current);
    }

    // Keeps the match list valid after the text changed.
    void find_sync() {
        find_matches = md::find_all(doc, find_query);
        const int n = static_cast<int>(find_matches.size());
        if (find_current >= n) find_current = n - 1;
    }

    void find_select(int index) {
        find_current = index;
        const md::Match& m = find_matches[static_cast<size_t>(index)];
        editor.set_caret(m.begin);
        editor.set_caret(m.end, true);
        ensure_caret_visible();
    }

    void find_step(bool forward) {
        find_sync();
        const int n = static_cast<int>(find_matches.size());
        if (n == 0) {
            status = tr("Совпадений нет", "No matches");
            return;
        }
        const int cur = std::min(find_current, n - 1);
        int next = 0;
        if (cur < 0) {
            next = forward ? 0 : n - 1;
        } else {
            next = forward ? (cur + 1) % n : (cur - 1 + n) % n;
        }
        find_select(next);
    }

    // With the current match selected, replaces it and selects the next one.
    // Otherwise only selects the match.
    void find_replace_current() {
        find_sync();
        if (find_matches.empty()) {
            status = tr("Совпадений нет", "No matches");
            return;
        }
        bool on_match = false;
        if (editor.has_selection() && find_current >= 0) {
            const auto [s, e] = editor.selection();
            const md::Match& m = find_matches[static_cast<size_t>(find_current)];
            on_match = m.begin == s && m.end == e;
        }
        if (!on_match) {
            find_query_changed();
            return;
        }
        editor.insert_text(find_replacement);
        mark_edited();
        find_query_changed();
    }

    void find_replace_all() {
        const std::vector<md::Match> matches = md::find_all(doc, find_query);
        if (matches.empty()) {
            status = tr("Совпадений нет", "No matches");
            return;
        }
        // Back to front, so earlier positions stay valid.
        for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
            editor.set_caret(it->begin);
            editor.set_caret(it->end, true);
            editor.insert_text(find_replacement);
        }
        mark_edited();
        find_matches = md::find_all(doc, find_query);
        find_current = -1;
        status = std::string(tr("Заменено: ", "Replaced: ")) + std::to_string(matches.size());
    }

    void find_text_input(const std::string& text) {
        find_text() += text;
        if (find_field == 0) find_query_changed();
    }

    void find_backspace() {
        std::string& t = find_text();
        if (!t.empty()) {
            size_t start = t.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(t[start]) & 0xC0) == 0x80) --start;
            t.erase(start);
        }
        if (find_field == 0) find_query_changed();
    }

    void find_key(const SDL_KeyboardEvent& key) {
        const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
        const bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
        switch (key.key) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (find_replace_mode && find_field == 1) {
                    if (ctrl) find_replace_all();
                    else find_replace_current();
                } else {
                    find_step(!shift);
                }
                break;
            case SDLK_TAB:
                if (find_replace_mode) find_field = 1 - find_field;
                break;
            case SDLK_BACKSPACE: find_backspace(); break;
            case SDLK_V:
                if (ctrl) {
                    char* clip = SDL_GetClipboardText();
                    std::string t = clip ? clip : "";
                    SDL_free(clip);
                    for (char& c : t) {
                        if (c == '\n' || c == '\r') c = ' ';
                    }
                    find_text() += t;
                    if (find_field == 0) find_query_changed();
                }
                break;
            case SDLK_F: if (ctrl) open_find(false); break;
            case SDLK_H: if (ctrl) open_find(true); break;
            default: break;
        }
    }

    void find_click(float x, float y) {
        find_focus = true;
        for (int i = 0; i < 2; ++i) {
            if (ui::contains(find_field_rect[i], x, y)) find_field = i;
        }
    }

    void draw_find_field(ui::Rect r, const std::string& text, const char* placeholder,
                         bool focused) {
        using namespace ui;
        fill_rounded_rect(ui, r, 4.0f, ui.palette.window_bg);
        stroke_rect(ui, r, focused ? ui.palette.accent : ui.palette.panel_border);
        const float ty = r.y + (r.h - text_height(fonts.regular)) / 2.0f;
        const float max_w = r.w - 16.0f;
        if (text.empty()) {
            draw_text(ui, fonts.regular, placeholder, r.x + 8.0f, ty, ui.palette.text_dim);
        } else {
            const std::string shown = fit_text(fonts.regular, text, max_w);
            draw_text(ui, fonts.regular, shown, r.x + 8.0f, ty, ui.palette.text);
        }
        if (focused && (SDL_GetTicks() / 530) % 2 == 0) {
            const float cx = r.x + 8.0f
                + std::min(text_width(fonts.regular, text), max_w);
            fill_rect(ui, {cx, ty, 1.5f, text_height(fonts.regular)}, ui.palette.text);
        }
    }

    void draw_find_bar(float x, float y, float w, bool replace) {
        using namespace ui;
        const float h = replace ? 76.0f : 36.0f;
        find_bar_rect = Rect{x, y, w, h};
        fill_rounded_rect(ui, find_bar_rect, 8.0f, ui.palette.panel_bg);
        stroke_rect(ui, find_bar_rect, ui.palette.panel_border);

        const float y1 = y + 4.0f;
        const float y2 = y + 40.0f;
        const Rect close_r{x + w - 6.0f - 28.0f, y1, 28.0f, 28.0f};
        const Rect next_r{close_r.x - 4.0f - 28.0f, y1, 28.0f, 28.0f};
        const Rect prev_r{next_r.x - 4.0f - 28.0f, y1, 28.0f, 28.0f};
        std::string count;
        if (!find_query.empty()) {
            if (find_matches.empty()) {
                count = tr("Нет совпадений", "No matches");
            } else {
                count = std::to_string(find_current < 0 ? 0 : find_current + 1) + " / "
                    + std::to_string(find_matches.size());
            }
        }
        const float count_w = count.empty() ? 0.0f : text_width(fonts.regular, count) + 12.0f;
        const float field_x = x + 6.0f;
        const float count_x = prev_r.x - 8.0f - count_w;
        find_field_rect[0] = Rect{field_x, y1, std::max(60.0f, count_x - 8.0f - field_x), 28.0f};

        draw_find_field(find_field_rect[0], find_query, tr("Поиск", "Find"),
                        find_focus && find_field == 0);
        if (!count.empty()) {
            draw_text(ui, fonts.regular, count, count_x + 12.0f,
                      y1 + (28.0f - text_height(fonts.regular)) / 2.0f,
                      find_matches.empty() ? ui.palette.text_dim : ui.palette.text);
        }

        if (button(ui, "find_prev", prev_r, "<")) find_step(false);
        if (button(ui, "find_next", next_r, ">")) find_step(true);
        if (button(ui, "find_close", close_r, "×")) close_find();

        if (replace) {
            const Rect all_r{x + w - 6.0f - 56.0f, y2, 56.0f, 28.0f};
            const Rect one_r{all_r.x - 4.0f - 96.0f, y2, 96.0f, 28.0f};
            find_field_rect[1] = Rect{field_x, y2, std::max(60.0f, one_r.x - 8.0f - field_x), 28.0f};
            draw_find_field(find_field_rect[1], find_replacement, tr("Заменить на", "Replace with"),
                            find_focus && find_field == 1);
            if (button(ui, "rep_one", one_r, tr("Заменить", "Replace"))) find_replace_current();
            if (button(ui, "rep_all", all_r, tr("Все", "All"))) find_replace_all();
        } else {
            find_field_rect[1] = Rect{};
        }
    }

    // ---------- outline ----------

    void toggle_outline() {
        outline_open = !outline_open;
        if (!outline_open) outline_rows.clear();
    }

    void draw_outline(ui::Rect side) {
        using namespace ui;
        fill_rect(ui, side, ui.palette.panel_bg);
        fill_rect(ui, {side.x + side.w - 1.0f, side.y, 1.0f, side.h}, ui.palette.panel_border);
        draw_text(ui, fonts.bold, tr("Оглавление", "Outline"), side.x + 12.0f, side.y + 10.0f,
                  ui.palette.text);

        outline_rows.clear();
        const float row_h = 22.0f;
        float y = side.y + 40.0f;
        for (size_t bi = 0; bi < doc.blocks.size(); ++bi) {
            const md::Block& block = doc.blocks[bi];
            if (block.type != md::BlockType::Heading) continue;
            if (y + row_h > side.y + side.h) break;
            const Rect row{side.x + 6.0f, y, side.w - 13.0f, row_h};
            std::string title;
            if (bi < layout.block_nums.size() && !layout.block_nums[bi].heading_prefix.empty()) {
                title += layout.block_nums[bi].heading_prefix;
            }
            for (const md::Inline& inl : block.inlines) title += inl.text;
            if (title.empty()) title = tr("(пустой заголовок)", "(empty heading)");
            const float indent = 8.0f + (std::clamp(block.level, 1, 6) - 1) * 12.0f;
            if (contains(row, ui.input.mouse_x, ui.input.mouse_y)) {
                fill_rounded_rect(ui, row, 4.0f, ui.palette.button_hover);
            }
            draw_text(ui, fonts.regular, fit_text(fonts.regular, title, row.w - indent - 6.0f),
                      row.x + indent, y + 3.0f, ui.palette.text);
            outline_rows.push_back({row, static_cast<int>(bi)});
            y += row_h;
        }
        if (outline_rows.empty()) {
            draw_text(ui, fonts.regular, tr("Заголовков нет", "No headings"), side.x + 12.0f,
                      side.y + 40.0f, ui.palette.text_dim);
        }
    }

    // Moves the caret to a clicked heading. False when the point is not on a row.
    bool outline_click(float x, float y) {
        for (const auto& [row, block] : outline_rows) {
            if (!ui::contains(row, x, y)) continue;
            editor.set_caret(md::Pos{block, 0, 0});
            for (const md::Line& line : layout.lines) {
                if (line.block != block) continue;
                scroll_y = std::clamp(line.y - 12.0f, 0.0f, max_scroll);
                break;
            }
            return true;
        }
        return false;
    }

    void apply_history(bool redo) {
        md::Pos caret, anchor;
        const bool done = redo ? history.redo(doc, caret, anchor)
                               : history.undo(doc, caret, anchor);
        if (!done) {
            status = redo ? tr("Нечего повторять", "Nothing to redo")
                          : tr("Нечего отменять", "Nothing to undo");
            return;
        }
        editor.set_caret(anchor);
        editor.set_caret(caret, true);
        mark_edited();
        status = redo ? tr("Повторено", "Redone") : tr("Отменено", "Undone");
    }

    // Runs one event, then records what it changed for undo.


    std::vector<std::string> code_lang_items() const {
        std::vector<std::string> items;
        for (const CodeLang& l : kCodeLangs) {
            items.push_back(l.value[0] == '\0' ? std::string(tr("Без языка", "No language")) : std::string(l.label));
        }
        return items;
    }

    void draw_workspace() {
        using namespace ui;
        // Зарезервировать место снизу, если открыта панель редактирования сноски,
        // чтобы текст документа не заходил под панель.
        float fn_reserve = footnote_editor_open ? 200.0f : 0.0f;
        Rect area{0, toolbar_h, static_cast<float>(width),
                  static_cast<float>(height) - toolbar_h - 28.0f - fn_reserve};
        ui::fill_rect(ui, area, ui.palette.window_bg);
        if (outline_open) {
            area.x += kOutlineW;
            area.w -= kOutlineW;
        }

        const float doc_zoom = text_zoom();
        const float content_w = 700.0f * doc_zoom;
        const float mm = content_w / 175.0f;
        const float content_x = area.x + (area.w - content_w) / 2.0f + 7.5f * mm;
        // The find bar sits above the document and pushes the text down.
        const float top = find_open ? 8.0f + (find_replace_mode ? 76.0f : 36.0f) + 8.0f : 24.0f;
        const Rect viewport{content_x, area.y + top, content_w, area.h - top - 24.0f};

        // The formula the caret is in is laid out as its source; leaving it re-renders it.
        const md::MathReveal reveal = md::math_reveal_at(doc, editor.caret());
        if (!(reveal == math_reveal_shown)) doc_dirty = true;
        if (content_w != laid_out_width || doc_zoom != laid_out_zoom || doc_dirty) {
            fontbook.set_base_pt(doc.meta.font_pt);
            fontbook.set_zoom(doc_zoom);
            md::layout_document(doc, fontbook, content_w, layout, reveal);
            math_reveal_shown = reveal;
            laid_out_width = content_w;
            laid_out_zoom = doc_zoom;
            doc_dirty = false;
            word_count = md::count_words(doc);
            if (find_open) find_sync();
        }
        max_scroll = std::max(0.0f, layout.content_height - viewport.h + 24.0f);
        // Re-anchor the view: document geometry scales proportionally with the
        // zoom, so the held point moves by the zoom ratio. Keep it under the
        // cursor by shifting the scroll by the same amount.
        if (zoom_anchor_doc_y >= 0.0f) {
            const float ratio = doc_zoom / zoom_anchor_old_zoom;
            scroll_y = zoom_anchor_doc_y * ratio - zoom_anchor_view_y;
            zoom_anchor_doc_y = -1.0f;
        }
        scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);
        doc_viewport = viewport;

        md::EditView view;
        if (editor.has_selection()) {
            const auto [s, e] = editor.selection();
            view.selecting = true;
            view.sel_start = s;
            view.sel_end = e;
        }
        view.caret_visible = (SDL_GetTicks() - editor.blink_tick) % 1060 < 530;
        view.caret = editor.caret();
        view.active_stamp_field = active_stamp_field;
        view.active_target = active_stamp_target;
        view.stamp_slots = &stamp_slots;
        md::render_document(ui, doc, layout, fontbook, viewport, scroll_y, &view);
        if (outline_open) {
            draw_outline(Rect{0.0f, area.y, kOutlineW, area.h});
        }
        if (find_open) draw_find_bar(content_x, area.y + 8.0f, content_w, find_replace_mode);

        // Code blocks carry a language menu in their header strip.
        for (size_t bi = 0; bi < doc.blocks.size() && bi < layout.boxes.size(); ++bi) {
            md::Block& block = doc.blocks[bi];
            if (block.type != md::BlockType::CodeBlock || is_visual_lang(block.lang)) continue;
            const float by = viewport.y + layout.boxes[bi].y - scroll_y;
            const float top = std::max(by + 8.0f, viewport.y + 4.0f); // a block scrolled under the top edge
            if (by + 40.0f < viewport.y || top + 24.0f > viewport.y + viewport.h) continue;
            std::string current = block.lang.empty() ? std::string(tr("язык ↓", "language ↓")) : block.lang + " ↓";
            for (const CodeLang& l : kCodeLangs) {
                if (!block.lang.empty() && block.lang == l.value) current = std::string(l.label) + " ↓";
            }
            const Rect r{viewport.x + layout.boxes[bi].x + 14.0f, top, 170.0f, 24.0f};
            const int pick = menu(ui, ("code_lang_" + std::to_string(bi)).c_str(), r, current.c_str(),
                                  code_lang_items(), 200.0f);
            if (pick >= 0) {
                block.lang = kCodeLangs[pick].value;
                mark_edited();
                history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            }
        }


        if (max_scroll > 0.0f) {
            const float thumb_h = std::max(30.0f,
                viewport.h * viewport.h / (layout.content_height + 24.0f));
            const float thumb_y = viewport.y
                + (viewport.h - thumb_h) * (scroll_y / max_scroll);
            scrollbar_track = Rect{area.x + area.w - 16.0f, viewport.y, 14.0f, viewport.h};
            scrollbar_thumb = Rect{area.x + area.w - 11.0f, thumb_y, 6.0f, thumb_h};
            const bool sb_hover = scrollbar_dragging
                || contains(scrollbar_track, ui.input.mouse_x, ui.input.mouse_y);
            ui::fill_rounded_rect(ui, scrollbar_thumb, 3.0f,
                                  sb_hover ? ui.palette.accent : ui.palette.panel_border);
        } else {
            scrollbar_track = Rect{};
            scrollbar_thumb = Rect{};
        }
    }

    void draw_statusbar() {
        using namespace ui;
        const float y = static_cast<float>(height) - 28.0f;
        ui::fill_rect(ui, {0, y, static_cast<float>(width), 28.0f}, ui.palette.panel_bg);
        ui::fill_rect(ui, {0, y, static_cast<float>(width), 1.0f}, ui.palette.panel_border);
        std::string right = std::string(tr("ГОСТ Р 2.105–2019 · А4", "GOST R 2.105–2019 · A4")) + "  ·  UTF-8";
        right = std::string(tr("Слов: ", "Words: ")) + std::to_string(word_count) + "  ·  " + right;
        if (zoom != 1.0f) {
            char zbuf[16];
            std::snprintf(zbuf, sizeof(zbuf), "%.0f%%", zoom * 100.0f);
            right = zbuf + ("  ·  " + right);
        }
        const float right_w = text_width(fonts.regular, right);
        const float right_x = static_cast<float>(width) - 12.0f - right_w;

        const std::string help = tr("Справка о дополнениях", "Extensions help");
        const float help_w = text_width(fonts.regular, help) + 24.0f;
        if (button(ui, "status_help", Rect{12.0f, y + 3.0f, help_w, 22.0f}, help.c_str())) {
            pandoc_help_open = true;
        }

        const float left_x = 12.0f + help_w + 18.0f;
        std::string left = std::string("• ") + (modified ? tr("изменено", "modified") : tr("сохранено", "saved"));
        if (!status.empty()) left += std::string("  ·  ") + status;
        ui::draw_text(ui, fonts.regular, fit_text(fonts.regular, left, right_x - left_x - 24.0f),
                      left_x, y + 6.0f, ui.palette.text_dim);

        ui::draw_text(ui, fonts.regular, right, right_x, y + 6.0f, ui.palette.text_dim);
    }

    // ---------- formula editor (tree-based) ----------

    // Open the formula editor for a new formula at the caret.
    void formula_open_new() {
        diag_editor_open = false;
        chart_editor_open = false;
        formula_editor_open = true;
        formula_block = -1;
        formula_edit = md::math_seq({});
        formula_path.clear();
        md::first_text_path(formula_edit, {}, formula_path);
        formula_off = 0;
        status = tr("Редактор формул: выберите структуру и заполняйте поля",
                    "Formula Editor: choose a structure and fill the slots");
    }

    // Open the formula editor for an existing math block.
    void formula_open_block(int block_index) {
        if (block_index < 0 || block_index >= static_cast<int>(doc.blocks.size())) return;
        const md::Block& b = doc.blocks[block_index];
        std::string tex;
        for (const md::Inline& inl : b.inlines) tex += inl.text;
        diag_editor_open = false;
        chart_editor_open = false;
        formula_editor_open = true;
        formula_block = block_index;
        formula_edit = md::math_from_latex(tex);
        formula_path.clear();
        md::first_text_path(formula_edit, {}, formula_path);
        const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        formula_off = leaf && leaf->type == md::MathNode::Text
                          ? static_cast<int>(leaf->text.size()) : 0;
        status = tr("Редактор формул: выберите поле и вводите символы",
                    "Formula Editor: click a slot and type symbols");
    }

    static md::MathNode formula_new_structure(const std::string& key) {
        using N = md::MathNode;
        auto slot = []() { return md::math_seq({}); };
        if (key == "frac")   return md::math_frac(slot(), slot());
        if (key == "sqrt")   return md::math_sqrt(slot());
        if (key == "root")   return md::math_root(slot(), slot());
        if (key == "sup")    return md::math_sup(slot(), slot());
        if (key == "sub")    return md::math_sub(slot(), slot());
        if (key == "sum")    return md::math_sum();
        if (key == "prod")   return N{N::Prod, "", {slot(), slot(), slot()}};
        if (key == "int")    return md::math_int();
        if (key == "parens") return md::math_parens(slot());
        if (key == "brackets") return N{N::Brackets, "", {slot()}};
        if (key == "braces") return N{N::Braces, "", {slot()}};
        if (key == "abs")    return N{N::Abs, "", {slot()}};
        if (key == "matrix") return md::math_matrix(2, 2);
        if (key == "cases")  return N{N::Piecewise, "", {md::math_seq({md::math_text("")})}};
        return md::math_seq({});
    }

    // Insert a structure at the caret, splitting the current text if needed.
    void formula_insert_node(const std::string& key) {
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        md::MathNode node = formula_new_structure(key);
        if (!leaf || leaf->type != md::MathNode::Text) {
            formula_edit.slots.push_back(std::move(node));
            const int idx = static_cast<int>(formula_edit.slots.size()) - 1;
            formula_path.clear();
            md::first_text_path(formula_edit.slots[static_cast<size_t>(idx)],
                                {idx}, formula_path);
            if (formula_path.empty()) formula_path = {idx};
            formula_off = 0;
            return;
        }
        const std::string before = leaf->text.substr(0, static_cast<size_t>(formula_off));
        const std::string after = leaf->text.substr(static_cast<size_t>(formula_off));

        std::vector<int> parent_path(formula_path.begin(), formula_path.end() - 1);
        const int slot_index = formula_path.back();
        md::MathNode* parent = md::node_at_path(formula_edit, parent_path);
        if (!parent) return;
        if (parent->type == md::MathNode::Seq) {
            // splice: [before-text] [structure] [after-text] in place of the leaf
            std::vector<md::MathNode> repl;
            if (!before.empty()) repl.push_back(md::math_text(before));
            repl.push_back(std::move(node));
            if (!after.empty()) repl.push_back(md::math_text(after));
            std::vector<md::MathNode> out;
            int target_idx = slot_index;
            for (size_t i = 0; i < parent->slots.size(); ++i) {
                if (static_cast<int>(i) == slot_index) {
                    target_idx = static_cast<int>(out.size());
                    for (auto& r : repl) out.push_back(std::move(r));
                } else {
                    out.push_back(std::move(parent->slots[i]));
                }
            }
            parent->slots = std::move(out);
            std::vector<int> base = parent_path;
            base.push_back(target_idx);
            formula_path.clear();
            md::first_text_path(parent->slots[static_cast<size_t>(target_idx)], base,
                                formula_path);
            if (formula_path.empty()) formula_path = base;
        } else {
            // Current slot is a direct child of a non-Seq structure (e.g. Matrix cell or Root):
            // wrap the existing text and the new structure into a Seq so nothing is overwritten!
            std::vector<md::MathNode> repl;
            if (!before.empty()) repl.push_back(md::math_text(before));
            const int node_idx = static_cast<int>(repl.size());
            repl.push_back(std::move(node));
            if (!after.empty()) repl.push_back(md::math_text(after));
            parent->slots[static_cast<size_t>(slot_index)] = md::math_seq(std::move(repl));
            std::vector<int> base = parent_path;
            base.push_back(slot_index);
            base.push_back(node_idx);
            formula_path.clear();
            md::first_text_path(
                parent->slots[static_cast<size_t>(slot_index)].slots[static_cast<size_t>(node_idx)],
                base, formula_path);
            if (formula_path.empty()) formula_path = base;
        }
        formula_off = 0;
    }

    // Insert a binary/relational operator as its own separated element and open a fresh slot after it.
    void formula_insert_operator(const char* op) {
        std::string op_str = std::string(" ") + op + " ";
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (leaf && leaf->type == md::MathNode::Seq && leaf->slots.empty()) {
            leaf->slots.push_back(md::math_text(op_str));
            leaf->slots.push_back(md::math_text(""));
            formula_path.push_back(1);
            formula_off = 0;
            return;
        }
        if (!leaf || leaf->type != md::MathNode::Text || formula_path.empty()) {
            formula_edit.slots.push_back(md::math_text(op_str));
            formula_edit.slots.push_back(md::math_text(""));
            formula_path = {static_cast<int>(formula_edit.slots.size()) - 1};
            formula_off = 0;
            return;
        }
        const std::string before = leaf->text.substr(0, static_cast<size_t>(formula_off));
        const std::string after = leaf->text.substr(static_cast<size_t>(formula_off));
        std::vector<int> parent_path(formula_path.begin(), formula_path.end() - 1);
        const int slot_index = formula_path.back();
        md::MathNode* parent = md::node_at_path(formula_edit, parent_path);
        if (!parent) return;
        if (parent->type == md::MathNode::Seq) {
            std::vector<md::MathNode> repl;
            if (!before.empty()) repl.push_back(md::math_text(before));
            repl.push_back(md::math_text(op_str));
            const int after_idx = static_cast<int>(repl.size());
            repl.push_back(md::math_text(after));
            std::vector<md::MathNode> out;
            int target_idx = slot_index;
            for (size_t i = 0; i < parent->slots.size(); ++i) {
                if (static_cast<int>(i) == slot_index) {
                    target_idx = static_cast<int>(out.size()) + after_idx;
                    for (auto& r : repl) out.push_back(std::move(r));
                } else {
                    out.push_back(std::move(parent->slots[i]));
                }
            }
            parent->slots = std::move(out);
            formula_path = parent_path;
            formula_path.push_back(target_idx);
            formula_off = 0;
        } else {
            leaf->text.insert(static_cast<size_t>(formula_off), op_str);
            formula_off += static_cast<int>(op_str.size());
        }
    }

    // Add a new empty block at the end of the current sequence (or root) so the user can continue the formula.
    void formula_add_slot_after() {
        if (formula_edit.type != md::MathNode::Seq) {
            formula_edit = md::math_seq({std::move(formula_edit)});
        }
        formula_edit.slots.push_back(md::math_text(""));
        formula_path = {static_cast<int>(formula_edit.slots.size()) - 1};
        formula_off = 0;
    }

    // Insert a single unicode symbol at the caret.
    void formula_insert_symbol(const char* sym) {
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (leaf && leaf->type == md::MathNode::Seq && leaf->slots.empty()) {
            if (leaf == &formula_edit) {
                // The root stays a Seq (structures depend on it): add a text child.
                formula_edit.slots.push_back(md::math_text(sym));
                formula_path = {0};
                formula_off = static_cast<int>(std::string(sym).size());
                return;
            }
            // An empty slot row becomes a text run when the user types.
            leaf->type = md::MathNode::Text;
            leaf->text = sym;
            formula_off = static_cast<int>(std::string(sym).size());
            return;
        }
        if (!leaf || leaf->type != md::MathNode::Text) {
            formula_edit.slots.push_back(md::math_text(sym));
            const int idx = static_cast<int>(formula_edit.slots.size()) - 1;
            formula_path = {idx};
            formula_off = static_cast<int>(std::string(sym).size());
            return;
        }
        leaf->text.insert(static_cast<size_t>(formula_off), sym);
        formula_off += static_cast<int>(std::string(sym).size());
    }

    // Backspace: delete one utf-8 code point; empty structures delete themselves.
    void formula_backspace() {
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (!leaf || leaf->type != md::MathNode::Text) return;
        if (formula_off > 0) {
            size_t p = static_cast<size_t>(formula_off);
            --p;
            while (p > 0 && (static_cast<unsigned char>(leaf->text[p]) & 0xC0) == 0x80) --p;
            leaf->text.erase(p, static_cast<size_t>(formula_off) - p);
            formula_off = static_cast<int>(p);
            return;
        }
        if (formula_path.empty()) return;
        std::vector<int> parent_path(formula_path.begin(), formula_path.end() - 1);
        const int idx = formula_path.back();
        md::MathNode* parent = md::node_at_path(formula_edit, parent_path);
        if (!parent || idx <= 0) return;
        md::MathNode& prev = parent->slots[static_cast<size_t>(idx) - 1];
        if (prev.type != md::MathNode::Text && prev.type != md::MathNode::Seq
            && md::math_is_empty(prev)) {
            parent->slots.erase(parent->slots.begin() + (idx - 1));
            return;
        }
        std::vector<int> prev_path = parent_path;
        prev_path.push_back(idx - 1);
        std::vector<int> tp;
        md::first_text_path(prev, prev_path, tp);
        if (!tp.empty()) {
            const md::MathNode* t = md::node_at_path(formula_edit, tp);
            formula_path = tp;
            formula_off = t && t->type == md::MathNode::Text
                              ? static_cast<int>(t->text.size()) : 0;
        }
    }

    // Delete the immediate element (structure or symbol run) the caret sits in,
    // never wiping out the entire formula when editing inside a nested structure.
    void formula_delete_element() {
        if (formula_edit.type != md::MathNode::Seq) {
            formula_edit = md::math_seq({});
            formula_path = {};
            formula_off = 0;
            return;
        }
        if (formula_path.empty()) return;

        // Find the innermost enclosing Seq (or structure) to remove just one block.
        int seq_depth = -1;
        int struct_depth = -1;
        for (int d = static_cast<int>(formula_path.size()) - 1; d >= 0; --d) {
            std::vector<int> sub(formula_path.begin(), formula_path.begin() + d);
            const md::MathNode* n = md::node_at_path(formula_edit, sub);
            if (!n) continue;
            if (n->type == md::MathNode::Seq) {
                if (n->slots.size() > 1 || d == 0) {
                    seq_depth = d;
                    break;
                }
            } else if (struct_depth < 0) {
                struct_depth = d;
            }
        }

        // If the caret is inside an single-element slot of a structure (like numerator of a fraction),
        // and that slot has text, clear its text first; if already empty, remove the structure itself from its parent Seq.
        if (seq_depth > 0) {
            std::vector<int> seq_path(formula_path.begin(), formula_path.begin() + seq_depth);
            const int child_idx = formula_path[static_cast<size_t>(seq_depth)];
            md::MathNode* seq_node = md::node_at_path(formula_edit, seq_path);
            if (seq_node && child_idx >= 0 && static_cast<size_t>(child_idx) < seq_node->slots.size()) {
                seq_node->slots.erase(seq_node->slots.begin() + child_idx);
                if (seq_node->slots.empty()) {
                    formula_path = seq_path;
                    formula_off = 0;
                } else {
                    const int land = std::min(child_idx, static_cast<int>(seq_node->slots.size()) - 1);
                    std::vector<int> base = seq_path;
                    base.push_back(land);
                    formula_path.clear();
                    md::first_text_path(seq_node->slots[static_cast<size_t>(land)], base, formula_path);
                    if (formula_path.empty()) formula_path = base;
                    const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
                    formula_off = leaf && leaf->type == md::MathNode::Text
                                      ? static_cast<int>(leaf->text.size()) : 0;
                }
                return;
            }
        }

        // If we are inside a structure slot that has only 1 element:
        // if that leaf has text, clear it; if it's already empty, remove the enclosing structure from its parent Seq.
        md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        if (formula_path.size() > 1 && leaf && leaf->type == md::MathNode::Text && !leaf->text.empty()) {
            leaf->text.clear();
            formula_off = 0;
            return;
        }

        // Remove top-level or enclosing structure from its parent Seq.
        int target_seq_depth = 0;
        for (int d = static_cast<int>(formula_path.size()) - 2; d >= 0; --d) {
            std::vector<int> sub(formula_path.begin(), formula_path.begin() + d);
            const md::MathNode* n = md::node_at_path(formula_edit, sub);
            if (n && n->type == md::MathNode::Seq) {
                target_seq_depth = d;
                break;
            }
        }
        std::vector<int> ppath(formula_path.begin(), formula_path.begin() + target_seq_depth);
        const int idx = formula_path[static_cast<size_t>(target_seq_depth)];
        md::MathNode* pseq = md::node_at_path(formula_edit, ppath);
        if (!pseq || idx < 0 || static_cast<size_t>(idx) >= pseq->slots.size()) return;
        pseq->slots.erase(pseq->slots.begin() + idx);
        if (pseq->slots.empty()) {
            formula_path = ppath;
            formula_off = 0;
        } else {
            const int land = std::min(idx, static_cast<int>(pseq->slots.size()) - 1);
            std::vector<int> base = ppath;
            base.push_back(land);
            formula_path.clear();
            md::first_text_path(pseq->slots[static_cast<size_t>(land)], base, formula_path);
            if (formula_path.empty()) formula_path = base;
            const md::MathNode* l = md::node_at_path(formula_edit, formula_path);
            formula_off = l && l->type == md::MathNode::Text
                              ? static_cast<int>(l->text.size()) : 0;
        }
    }

    // Move caret to next/previous slot in drawing order.
    void formula_move_slot(int dir) {
        if (formula_slots.empty()) return;
        int cur = 0;
        for (size_t i = 0; i < formula_slots.size(); ++i) {
            if (formula_slots[i].path == formula_path) { cur = static_cast<int>(i); break; }
        }
        int next = cur + dir;
        if (next < 0) next = static_cast<int>(formula_slots.size()) - 1;
        if (next >= static_cast<int>(formula_slots.size())) next = 0;
        formula_path = formula_slots[static_cast<size_t>(next)].path;
        const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
        formula_off = leaf && leaf->type == md::MathNode::Text
                          ? static_cast<int>(leaf->text.size()) : 0;
    }

    // Puts the formula into the text at the caret as an inline "$...$" run. The
    // spaces keep it a separate word, which is what makes it count as a formula.
    void formula_insert_inline() {
        std::string inner = md::math_to_latex(formula_edit); // "$$...$$"
        if (inner.size() >= 4) inner = inner.substr(2, inner.size() - 4);
        for (char& c : inner) {
            if (c == '\n' || c == '\r') c = ' ';
        }
        if (inner.empty() || inner == "\\,") {
            status = tr("Формула пустая", "The formula is empty");
            return;
        }
        const md::Pos caret = editor.caret();
        const md::Block& cb = doc.blocks[caret.block];
        if (cb.type == md::BlockType::CodeBlock || cb.type == md::BlockType::Rule) {
            status = tr("Поставьте курсор в текстовый абзац", "Put the caret in a text paragraph");
            return;
        }
        const std::vector<md::Inline>& inls = md::inlines_of(cb, caret.cell);
        char before = 0;
        char after = 0;
        if (caret.inl >= 0 && caret.inl < static_cast<int>(inls.size())) {
            const std::string& text = inls[static_cast<size_t>(caret.inl)].text;
            const int off = caret.offset;
            if (off > 0 && off <= static_cast<int>(text.size())) {
                before = text[static_cast<size_t>(off - 1)];
            } else if (caret.inl > 0 && !inls[static_cast<size_t>(caret.inl - 1)].text.empty()) {
                before = inls[static_cast<size_t>(caret.inl - 1)].text.back();
            }
            if (off < static_cast<int>(text.size())) {
                after = text[static_cast<size_t>(off)];
            } else if (caret.inl + 1 < static_cast<int>(inls.size())
                       && !inls[static_cast<size_t>(caret.inl + 1)].text.empty()) {
                after = inls[static_cast<size_t>(caret.inl + 1)].text.front();
            }
        }
        std::string piece = "$" + inner + "$";
        if (before != 0 && before != ' ') piece = " " + piece;
        const bool closes_sentence = after == '.' || after == ',' || after == ';' || after == ':'
            || after == '!' || after == '?' || after == ')';
        if (after != 0 && after != ' ' && !closes_sentence) piece += " ";
        editor.insert_text(piece);
        mark_edited();
        history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
        ensure_caret_visible();
        formula_editor_open = false;
        status = tr("Формула вставлена в строку", "Formula inserted inline");
    }

    void formula_apply() {
        std::string inner = md::math_to_latex(formula_edit); // "$$...$$"
        if (inner.size() >= 4) inner = inner.substr(2, inner.size() - 4);
        if (inner.empty() || inner == "\\,") inner = " ";
        if (formula_block >= 0 && formula_block < static_cast<int>(doc.blocks.size())) {
            md::Block& b = doc.blocks[formula_block];
            b.inlines.clear();
            b.inlines.push_back(md::Inline{inner, 0, ""});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Формула обновлена", "Formula updated");
        } else {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = "math";
            b.inlines.push_back(md::Inline{inner, 0, ""});
            const int at = std::max(0, editor.caret().block + 1);
            const int idx = std::min(at, static_cast<int>(doc.blocks.size()));
            doc.blocks.insert(doc.blocks.begin() + idx, std::move(b));
            editor.set_caret(md::Pos{idx, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("Формула вставлена", "Formula inserted");
        }
        doc_dirty = true;
        modified = true;
        formula_editor_open = false;
    }

    void draw_formula_editor() {
        if (!formula_editor_open) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float ow = w - 40.0f;
        const float oh = std::min(460.0f, h - toolbar_h - 40.0f);
        const float ox = 20.0f;
        const float oy = toolbar_h + 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold,
                  formula_block >= 0 ? tr("Редактор формулы", "Edit Formula")
                                     : tr("Новая формула", "New Formula"),
                  ox + 16.0f, oy + 10.0f, ui.palette.text);

        // --- structure buttons ---
        struct SBtn { const char* id; const char* lbl_ru; const char* lbl_en; };
        static const SBtn structs[] = {
            {"frac", "Дробь", "Frac"}, {"sqrt", "√", "√"}, {"root", "ⁿ√", "ⁿ√"}, {"sup", "xʸ", "xʸ"},
            {"sub", "xᵢ", "xᵢ"}, {"sum", "∑", "∑"}, {"prod", "∏", "∏"}, {"int", "∫", "∫"},
            {"parens", "( )", "( )"}, {"brackets", "[ ]", "[ ]"}, {"braces", "{ }", "{ }"},
            {"abs", "| |", "| |"}, {"matrix", "Матрица", "Matrix"}, {"cases", "Случ. ф.", "Cases"},
        };
        float tx = ox + 18.0f;
        float ty = oy + 38.0f;
        for (const SBtn& s : structs) {
            const char* lbl = en() ? s.lbl_en : s.lbl_ru;
            const float bw = text_width(fonts.regular, lbl) + 18.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 32.0f; }
            if (button(ui, ("fbtn_" + std::string(s.id)).c_str(),
                       {tx, ty, bw, 28.0f}, lbl)) {
                formula_insert_node(s.id);
            }
            tx += bw + 5.0f;
        }
        {
            const char* add_lbl = tr("+ блок", "+ slot");
            const float bw = text_width(fonts.regular, add_lbl) + 18.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 32.0f; }
            if (button(ui, "fbtn_add_slot", {tx, ty, bw, 28.0f}, add_lbl)) {
                formula_add_slot_after();
            }
        }
        ty += 34.0f;

        // --- operator buttons (split/create new blocks around operator) ---
        static const char* ops[] = {
            "+", "-", "=", "±", "×", "÷", "≠", "≤", "≥", "≈", "→",
        };
        tx = ox + 18.0f;
        for (const char* op : ops) {
            const float bw = text_width(fonts.regular, op) + 18.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 30.0f; }
            if (button(ui, ("fop_" + std::string(op)).c_str(),
                       {tx, ty, bw, 26.0f}, op)) {
                formula_insert_operator(op);
            }
            tx += bw + 5.0f;
        }
        ui::separator(ui, {tx + 2.0f, ty + 3.0f, 1.0f, 20.0f});
        tx += 10.0f;

        // --- symbol buttons ---
        static const char* syms[] = {
            "α","β","γ","δ","θ","λ","μ","π","σ","φ","ω","∞","…",
        };
        for (const char* sym : syms) {
            const float bw = text_width(fonts.regular, sym) + 16.0f;
            if (tx + bw > ox + ow - 18.0f) { tx = ox + 18.0f; ty += 30.0f; }
            if (button(ui, ("fsym_" + std::string(sym)).c_str(),
                       {tx, ty, bw, 26.0f}, sym)) {
                formula_insert_symbol(sym);
            }
            tx += bw + 5.0f;
        }
        ty += 32.0f;

        // --- formula canvas ---
        const float canvas_h = std::max(64.0f, oh - (ty - oy) - 56.0f);
        const Rect canvas{ox + 18.0f, ty, ow - 36.0f, canvas_h};
        ui::fill_rect(ui, canvas, ui.palette.window_bg);
        ui::stroke_rect(ui, canvas, ui.palette.panel_border);

        // Click on a slot (checked against the previous frame's rects).
        if (ui.input.released &&
            canvas.x <= ui.input.mouse_x && ui.input.mouse_x <= canvas.x + canvas.w &&
            canvas.y <= ui.input.mouse_y && ui.input.mouse_y <= canvas.y + canvas.h) {
            for (const auto& slot : formula_slots) {
                if (slot.rect.x <= ui.input.mouse_x && ui.input.mouse_x <= slot.rect.x + slot.rect.w &&
                    slot.rect.y <= ui.input.mouse_y && ui.input.mouse_y <= slot.rect.y + slot.rect.h) {
                    formula_path = slot.path;
                    const md::MathNode* leaf = md::node_at_path(formula_edit, formula_path);
                    formula_off = leaf && leaf->type == md::MathNode::Text
                                      ? static_cast<int>(leaf->text.size()) : 0;
                    break;
                }
            }
        }

        // render the tree; collect clickable slots
        formula_slots.clear();
        md::mathdraw::render(&ui, fontbook, formula_edit,
                         canvas.x + 14.0f, canvas.y + 10.0f, 20.0f,
                         &formula_path, &formula_slots);
        ui::draw_text(ui, fonts.regular,
                  tr("Клик по полю — выбор, Tab — следующее, Delete — удалить элемент, Esc — закрыть",
                     "Click slot to select, Tab — next, Delete — remove element, Esc — close"),
                  canvas.x + 4.0f, canvas.y + canvas_h - 17.0f, ui.palette.text_dim);

        // --- apply / delete / cancel ---
        const float by = oy + oh - 40.0f;
        if (button(ui, "f_apply", {ox + 18.0f, by, 130.0f, 30.0f},
                   formula_block >= 0 ? tr("Сохранить", "Save") : tr("Вставить", "Insert"))) {
            formula_apply();
        }
        if (button(ui, "f_delete", {ox + 156.0f, by, 150.0f, 30.0f}, tr("Удалить элемент", "Delete Element"))) {
            formula_delete_element();
        }
        if (button(ui, "f_cancel", {ox + 314.0f, by, 110.0f, 30.0f}, tr("Отмена", "Cancel"))) {
            formula_editor_open = false;
            status = tr("Редактор формул закрыт", "Formula editor closed");
        }
        if (button(ui, "f_inline", {ox + 434.0f, by, 150.0f, 30.0f}, tr("В строку", "Inline"))) {
            formula_insert_inline();
        }
    }

    // ---------- chart editor ----------

    std::string* chart_sel_text() {
        if (chart_sel < 0) return nullptr;
        if (chart_sel_value) {
            return chart_sel < static_cast<int>(chart_edit.values.size())
                ? &chart_edit.values[static_cast<size_t>(chart_sel)]
                : nullptr;
        }
        return chart_sel < static_cast<int>(chart_edit.labels.size())
            ? &chart_edit.labels[static_cast<size_t>(chart_sel)]
            : nullptr;
    }

    void chart_select(int index, bool value) {
        chart_sel = index;
        chart_sel_value = value;
        std::string* t = chart_sel_text();
        chart_off = t != nullptr ? static_cast<int>(t->size()) : 0;
    }

    void chart_open_new() {
        formula_editor_open = false;
        diag_editor_open = false;
        chart_editor_open = true;
        chart_block = -1;
        chart_edit = md::chart_new();
        chart_select(0, false);
        status = tr("Редактор графика: выберите ячейку и вводите текст",
                    "Chart Editor: select a cell and type text");
    }

    bool chart_open_block(int block_index) {
        if (block_index < 0 || block_index >= static_cast<int>(doc.blocks.size())) {
            return false;
        }
        const md::Block& b = doc.blocks[block_index];
        std::string code;
        for (const md::Inline& inl : b.inlines) code += inl.text;
        if (!md::is_chart_text(code)) return false;
        bool ok = true;
        md::ChartModel model = md::chart_from_text(code, &ok);
        if (!ok) {
            status = tr("График не поддерживается визуальным редактором",
                        "Chart is not supported by visual editor");
            return false;
        }
        formula_editor_open = false;
        diag_editor_open = false;
        chart_editor_open = true;
        chart_block = block_index;
        chart_edit = std::move(model);
        if (chart_edit.labels.empty()) chart_edit = md::chart_new();
        chart_select(0, false);
        status = tr("Редактор графика: клик по элементу — выбор, ввод — текст",
                    "Chart Editor: click element to select, type to edit");
        return true;
    }

    static constexpr size_t kMaxChartRows = 12;

    void chart_add_row() {
        if (chart_edit.labels.size() >= kMaxChartRows) {
            status = tr("Достигнуто максимальное число рядов (12)",
                        "Maximum number of rows reached (12)");
            return;
        }
        chart_edit.labels.push_back("");
        chart_edit.values.push_back("");
        chart_select(static_cast<int>(chart_edit.labels.size()) - 1, false);
    }

    void chart_delete_row() {
        if (chart_edit.labels.empty()) return;
        if (chart_sel < 0 || chart_sel >= static_cast<int>(chart_edit.labels.size())) {
            chart_select(0, false);
        }
        if (chart_edit.labels.size() == 1) {
            chart_edit.labels[0].clear();
            chart_edit.values[0].clear();
            chart_select(0, false);
            status = tr("Ряд очищен", "Row cleared");
            return;
        }
        const int idx = std::clamp(chart_sel, 0, static_cast<int>(chart_edit.labels.size()) - 1);
        chart_edit.labels.erase(chart_edit.labels.begin() + idx);
        if (idx < static_cast<int>(chart_edit.values.size())) {
            chart_edit.values.erase(chart_edit.values.begin() + idx);
        }
        if (chart_edit.labels.empty()) {
            chart_edit.labels.push_back("");
            chart_edit.values.push_back("");
        }
        chart_select(std::min(idx, static_cast<int>(chart_edit.labels.size()) - 1), false);
        status = tr("Ряд удалён", "Row deleted");
    }

    void chart_insert_text(const char* sym) {
        std::string* t = chart_sel_text();
        if (t == nullptr) {
            if (chart_edit.labels.empty()) chart_add_row();
            chart_select(0, false);
            t = chart_sel_text();
            if (t == nullptr) return;
        }
        t->insert(static_cast<size_t>(chart_off), sym);
        chart_off += static_cast<int>(std::string(sym).size());
    }

    void chart_backspace() {
        std::string* t = chart_sel_text();
        if (t == nullptr) {
            chart_delete_row();
            return;
        }
        if (t->empty()) {
            chart_delete_row();
            return;
        }
        if (chart_off <= 0) {
            chart_delete_row();
            return;
        }
        size_t p = static_cast<size_t>(chart_off);
        --p;
        while (p > 0 && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) --p;
        t->erase(p, static_cast<size_t>(chart_off) - p);
        chart_off = static_cast<int>(p);
    }

    void chart_move_slot(int dir) {
        if (chart_slots.empty()) return;
        int cur = 0;
        for (size_t i = 0; i < chart_slots.size(); ++i) {
            if (chart_slots[i].index == chart_sel && chart_slots[i].value == chart_sel_value) {
                cur = static_cast<int>(i);
                break;
            }
        }
        int next = cur + dir;
        if (next < 0) next = static_cast<int>(chart_slots.size()) - 1;
        if (next >= static_cast<int>(chart_slots.size())) next = 0;
        const md::chartdraw::Slot& s = chart_slots[static_cast<size_t>(next)];
        chart_select(s.index, s.value);
    }

    void chart_apply() {
        const std::string code = md::chart_to_mermaid(chart_edit);
        if (chart_block >= 0 && chart_block < static_cast<int>(doc.blocks.size())) {
            md::Block& b = doc.blocks[static_cast<size_t>(chart_block)];
            b.lang = "mermaid";
            b.inlines.clear();
            b.inlines.push_back(md::Inline{code, 0, ""});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("График обновлён", "Chart updated");
        } else {
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = "mermaid";
            b.inlines.push_back(md::Inline{code, 0, ""});
            const int at = std::max(0, editor.caret().block + 1);
            const int idx = std::min(at, static_cast<int>(doc.blocks.size()));
            doc.blocks.insert(doc.blocks.begin() + idx, std::move(b));
            editor.set_caret(md::Pos{idx, 0, 0, -1});
            history.commit(doc, editor.caret(), editor.anchor(), md::EditKind::Other);
            status = tr("График вставлен", "Chart inserted");
        }
        doc_dirty = true;
        modified = true;
        chart_editor_open = false;
    }

    void draw_footnote_editor() {
        if (!footnote_editor_open || active_footnote_id <= 0) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float ow = std::min(680.0f, w - 40.0f);
        const float oh = 150.0f;
        const float ox = (w - ow) / 2.0f;
        const float oy = static_cast<float>(height) - oh - 22.0f - 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold, tr("Сноска (ГОСТ Р 2.105 п. 6.13)", "Footnote (GOST R 2.105 6.13)"),
                      ox + 16.0f, oy + 10.0f, ui.palette.text);

        std::string* ftxt = nullptr;
        for (auto& f : doc.footnotes) {
            if (f.id == active_footnote_id) { ftxt = &f.text; break; }
        }
        if (!ftxt) {
            footnote_editor_open = false;
            active_footnote_id = 0;
            return;
        }
        const Rect box{ox + 16.0f, oy + 40.0f, ow - 32.0f, 70.0f};
        ui::fill_rect(ui, box, ui.palette.window_bg);
        ui::stroke_rect(ui, box, ui.palette.panel_border);
        std::string shown = *ftxt;
        while (!shown.empty() && ui::text_width(fonts.regular, shown) > box.w - 24.0f) {
            shown.erase(shown.begin());
        }
        const Uint64 t = SDL_GetTicks();
        ui::draw_text(ui, fonts.regular, shown + ((t % 1000) < 500 ? "|" : ""),
                      box.x + 8.0f, box.y + 10.0f, ui.palette.text);
        if (ui::button(ui, "fn_close", {ox + ow - 110.0f, oy + oh - 36.0f, 94.0f, 28.0f}, tr("Готово", "Done"))) {
            footnote_editor_open = false;
            active_footnote_id = 0;
        }
        if (ui::button(ui, "fn_delete", {ox + ow - 212.0f, oy + oh - 36.0f, 94.0f, 28.0f}, tr("Удалить сноску", "Delete Footnote"))) {
            // Удаляем запись сноски и все маркеры в тексте с этим footnote_id.
            for (auto it = doc.footnotes.begin(); it != doc.footnotes.end(); ++it) {
                if (it->id == active_footnote_id) {
                    doc.footnotes.erase(it);
                    break;
                }
            }
            // Удаляем маркеры сноски из всех блоков (включая ячейки таблиц).
            for (auto& b : doc.blocks) {
                auto remove_fn = [&](std::vector<md::Inline>& inls) {
                    for (auto it2 = inls.begin(); it2 != inls.end(); ) {
                        if (it2->footnote_id == active_footnote_id && it2->text.empty()) {
                            it2 = inls.erase(it2);
                        } else {
                            ++it2;
                        }
                    }
                };
                remove_fn(b.inlines);
                if (b.type == md::BlockType::Table) {
                    for (auto& cell : b.cells) remove_fn(cell);
                }
            }
            // Нормализуем пустые инлайны (оставляем хотя бы один в блоке).
            for (auto& b : doc.blocks) {
                auto& inls = b.inlines;
                if (inls.empty()) inls.push_back(md::Inline{});
                if (b.type == md::BlockType::Table) {
                    for (auto& cell : b.cells) {
                        if (cell.empty()) cell.push_back(md::Inline{});
                    }
                }
            }
            footnote_editor_open = false;
            active_footnote_id = 0;
            doc_dirty = true;
            mark_edited();
            ensure_caret_visible();
        }
        ui::draw_text(ui, fonts.regular, tr("Текст сноски редактируется здесь, маркер N) остаётся только в основном тексте", "Edit footnote text here; only marker N) shown in main text"),
                      ox + 16.0f, oy + oh - 28.0f, ui.palette.text_dim);
    }

    void draw_chart_editor() {
        if (!chart_editor_open) return;
        using namespace ui;
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        const float ow = w - 40.0f;
        const float oh = std::min(560.0f, h - toolbar_h - 40.0f);
        const float ox = 20.0f;
        const float oy = toolbar_h + 20.0f;
        ui::fill_rounded_rect(ui, {ox, oy, ow, oh}, 10.0f, ui.palette.panel_bg);
        ui::stroke_rect(ui, {ox, oy, ow, oh}, ui.palette.panel_border);
        ui::draw_text(ui, fonts.bold,
                  chart_block >= 0 ? tr("Редактор графика", "Edit Chart")
                                   : tr("Новый график", "New Chart"),
                  ox + 16.0f, oy + 10.0f, ui.palette.text);

        const bool pie = chart_edit.kind == md::ChartKind::Pie;

        float tx = ox + 18.0f;
        float ty = oy + 40.0f;
        auto row_button = [&](const char* id, const char* label, bool enabled,
                              const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (button(ui, id, {tx, ty, bw, 30.0f}, label, enabled)) act();
            tx += bw + 6.0f;
        };
        auto row_toggle = [&](const char* id, const char* label, bool active,
                              bool enabled, const std::function<void()>& act) {
            const float bw = text_width(fonts.regular, label) + 20.0f;
            if (tx + bw > ox + ow - 18.0f) {
                tx = ox + 18.0f;
                ty += 34.0f;
            }
            if (toggle_button(ui, id, {tx, ty, bw, 30.0f}, label, active, enabled)) act();
            tx += bw + 6.0f;
        };

        row_toggle("ch_bar", tr("Столбчатый", "Bar"), chart_edit.kind == md::ChartKind::Bar, true,
                   [&]() { chart_edit.kind = md::ChartKind::Bar; });
        row_toggle("ch_line", tr("Линейный", "Line"), chart_edit.kind == md::ChartKind::Line, true,
                   [&]() { chart_edit.kind = md::ChartKind::Line; });
        row_toggle("ch_pie", tr("Круговой", "Pie"), pie, true,
                   [&]() { chart_edit.kind = md::ChartKind::Pie; });
        ty += 34.0f;

        tx = ox + 18.0f;
        row_button("ch_add", tr("Добавить ряд", "Add Row"), true, [&]() { chart_add_row(); });
        row_button("ch_del", tr("Удалить ряд", "Delete Row"), true, [&]() { chart_delete_row(); });
        ty += 34.0f;

        // --- chart canvas (clipped so long labels/legends never bleed over buttons) ---
        const float canvas_h = std::max(80.0f, oh - (ty - oy) - 56.0f);
        const Rect canvas{ox + 18.0f, ty, ow - 36.0f, canvas_h};
        ui::fill_rect(ui, canvas, ui.palette.window_bg);
        ui::stroke_rect(ui, canvas, ui.palette.panel_border);

        if (ui.input.released && contains(canvas, ui.input.mouse_x, ui.input.mouse_y)) {
            for (const auto& slot : chart_slots) {
                if (contains(slot.rect, ui.input.mouse_x, ui.input.mouse_y)) {
                    chart_select(slot.index, slot.value);
                    break;
                }
            }
        }

        const SDL_Rect clip_rect{static_cast<int>(canvas.x + 1), static_cast<int>(canvas.y + 1),
                                 static_cast<int>(canvas.w - 2), static_cast<int>(canvas.h - 20)};
        SDL_SetRenderClipRect(ui.renderer, &clip_rect);
        chart_slots.clear();
        md::chartdraw::Selection sel;
        sel.index = chart_sel;
        sel.value = chart_sel_value;
        // The chart must fit between the top of the canvas and the hint line,
        // otherwise the category/value slots end up outside the clipped area.
        const float chart_max_h = std::max(60.0f, canvas.h - 26.0f);
        md::chartdraw::render(&ui, fontbook, chart_edit,
                              canvas.x + 14.0f, canvas.y + 6.0f, canvas.w - 28.0f,
                              chart_sel >= 0 ? &sel : nullptr, &chart_slots,
                              chart_max_h);
        SDL_SetRenderClipRect(ui.renderer, nullptr);
        ui::draw_text(ui, fonts.regular,
                  tr("Клик по категории/значению — выбор, ввод — текст, Tab — следующий, Delete — удалить/очистить ряд, Esc — закрыть",
                     "Click category/value to select, type to edit, Tab — next, Delete — remove/clear row, Esc — close"),
                  canvas.x + 4.0f, canvas.y + canvas_h - 17.0f, ui.palette.text_dim);

        const float by = oy + oh - 40.0f;
        if (button(ui, "ch_apply", {ox + 18.0f, by, 130.0f, 30.0f},
                   chart_block >= 0 ? tr("Сохранить", "Save") : tr("Вставить", "Insert"))) {
            chart_apply();
        }
        if (button(ui, "ch_cancel", {ox + 156.0f, by, 110.0f, 30.0f}, tr("Отмена", "Cancel"))) {
            chart_editor_open = false;
            status = tr("Редактор графика закрыт", "Chart editor closed");
        }
    }

    // ---------- diagram editor ----------

    // Label of the selected element; nullptr when nothing is selected.
    std::string* diag_sel_text() {
        if (diag_sel < 0) return nullptr;
        if (diag_sel_edge) {
            return diag_sel < static_cast<int>(diag_edit.edges.size())
                ? &diag_edit.edges[static_cast<size_t>(diag_sel)].label
                : nullptr;
        }
        return diag_sel < static_cast<int>(diag_edit.nodes.size())
            ? &diag_edit.nodes[static_cast<size_t>(diag_sel)].label
            : nullptr;
    }



    void frame() {
        update_link_cursor();
        if (geometry_dirty && SDL_GetTicks() - geometry_tick > 500) {
            geometry_dirty = false;
            save_settings();
        }
        SDL_GetRenderOutputSize(renderer, &width, &height);
        ui.screen_h = static_cast<float>(height);
        SDL_SetRenderDrawColor(renderer, ui.palette.window_bg.r, ui.palette.window_bg.g,
                               ui.palette.window_bg.b, ui.palette.window_bg.a);
        SDL_RenderClear(renderer);

        ui::begin_frame(ui);
        draw_toolbar();
        draw_workspace();
        draw_statusbar();
        draw_formula_editor();
        draw_link_dialog();
        draw_diag_editor();
        draw_footnote_editor();
        draw_chart_editor();
        auto wrap_lines = [&](TTF_Font* fnt, const std::string& s, float maxw) {
            std::vector<std::string> out;
            std::string cur;
            std::string word;
            for (char c : s) {
                if (c == ' ' || c == '\t') {
                    if (!word.empty()) {
                        std::string cand = cur.empty() ? word : cur + " " + word;
                        if (!cur.empty() && ui::text_width(fnt, cand) > maxw) {
                            out.push_back(cur);
                            cur = word;
                        } else {
                            cur = cand;
                        }
                        word.clear();
                    }
                    continue;
                }
                if (c == '\n') {
                    if (!word.empty()) {
                        std::string cand = cur.empty() ? word : cur + " " + word;
                        if (!cur.empty() && ui::text_width(fnt, cand) > maxw) {
                            out.push_back(cur);
                            cur = word;
                        } else {
                            cur = cand;
                        }
                        word.clear();
                    }
                    if (!cur.empty()) out.push_back(cur);
                    cur.clear();
                    continue;
                }
                word += c;
            }
            if (!word.empty()) {
                std::string cand = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && ui::text_width(fnt, cand) > maxw) {
                    out.push_back(cur);
                    cur = word;
                } else {
                    cur = cand;
                }
            }
            if (!cur.empty()) out.push_back(cur);
            return out;
        };
        if (pandoc_help_open) {
            using namespace ui;
            const float w = 580.0f;
            const float h = 380.0f;
            const Rect bg{(static_cast<float>(width)-w)/2.0f, (static_cast<float>(height)-h)/2.0f, w, h};
            ui::fill_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)},
                          SDL_Color{0, 0, 0, 140});
            ui::fill_rounded_rect(ui, bg, 8.0f, ui.palette.panel_bg);
            ui::stroke_rect(ui, bg, ui.palette.panel_border);
            ui::draw_text(ui, fonts.bold,
                          tr("Справка по ГОСТ и экспорту", "GOST & Export Help"),
                          bg.x+16.0f, bg.y+10.0f, ui.palette.text);
            if (ui::button(ui, "ph_close", Rect{bg.x+w-96.0f, bg.y+8.0f, 80.0f, 26.0f},
                           tr("Закрыть", "Close"))) {
                pandoc_help_open = false;
            }
            float y = bg.y + 46.0f;
            {
                std::string txt = tr(
                    "GOST-Writer оформляет текстовые конструкторские документы по ГОСТ Р 2.105–2019 и ГОСТ Р 2.106–2019 (ЕСКД): рамка А4 185×287 мм, основная надпись Форма 2 и Форма 2а (ГОСТ 2.104), титульный лист, автосодержание, полностраничные формы (Спецификация, ВД, ТП/ЭП/ПТ) и Лист регистрации изменений (ГОСТ 2.503). Экспорт в PDF (Typst) и DOCX/ODT/HTML (Pandoc) переносит свёрстанные листы 1:1 с экраном.",
                    "GOST-Writer formats technical text documents per GOST R 2.105–2019 and GOST R 2.106–2019 (ESKD): A4 185×287 mm frame, Form 2 & Form 2a stamps (GOST 2.104), title page, auto-TOC, full-page tabular forms (Specification, VD, TP/EP/PT) and Change Sheet (GOST 2.503). Export to PDF (Typst) and DOCX/ODT/HTML (Pandoc) preserves the A4 sheets 1:1.");
                auto lines = wrap_lines(fonts.regular, txt, w - 32.0f);
                for (auto& l : lines) {
                    ui::draw_text(ui, fonts.regular, l, bg.x+16.0f, y, ui.palette.text);
                    y += 18.0f;
                }
                y += 6.0f;
            }
            const char* head[] = {tr("Дополнение", "Extension"), "GitHub", "GitLab", "Notion", "Obsidian", "Reddit"};
            float xh = bg.x+16.0f;
            for (int i=0;i<6;++i) {
                ui::draw_text(ui, fonts.regular, head[i], xh, y, ui.palette.text_dim);
                xh += (i==0?200.0f:72.0f);
            }
            y += 20.0f;
            const char* rows[][6] = {
                {"LaTeX/Math", "+", "+", "+", "+", "-"},
                {tr("Mermaid (диаграммы)", "Mermaid (diagrams)"), "+", "+", "+", "+", "-"},
                {tr("XYChart/Pie (графики)", "XYChart/Pie (charts)"), "-", "-", "-", "+/-", "-"},
                {tr("GFM-таблицы", "GFM tables"), "+", "+", "+", "+", "+"},
            };
            for (int r=0;r<4;++r) {
                float xx = bg.x+16.0f;
                for (int c=0;c<6;++c) {
                    ui::draw_text(ui, fonts.regular, rows[r][c], xx, y, ui.palette.text);
                    xx += (c==0?200.0f:72.0f);
                }
                y += 18.0f;
            }
            y += 10.0f;
            {
                std::string note = tr(
                    "+ — обычно поддерживается, +/- — зависит от сайта/приложения, - — не поддерживается. Поддержка в браузерных MD-рендерах отличается от экспорта через Pandoc.",
                    "+ — widely supported, +/- — depends on site/plugins, - — unsupported. Browser Markdown renderers differ from Pandoc export.");
                auto lines = wrap_lines(fonts.regular, note, w - 32.0f);
                for (auto& l : lines) {
                    ui::draw_text(ui, fonts.regular, l, bg.x+16.0f, y, ui.palette.text_dim);
                    y += 16.0f;
                }
            }
        }
        if (welcome_open) {
            using namespace ui;
            const float w = std::min(680.0f, static_cast<float>(width) - 32.0f);
            const float h = std::min(600.0f, static_cast<float>(height) - 32.0f);
            const Rect bg{(static_cast<float>(width) - w) / 2.0f,
                          (static_cast<float>(height) - h) / 2.0f, w, h};
            ui::fill_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)},
                          SDL_Color{0, 0, 0, 145});
            ui::fill_rounded_rect(ui, bg, 10.0f, ui.palette.panel_bg);
            ui::stroke_rect(ui, bg, ui.palette.panel_border);

            ui::draw_text(ui, fonts.bold,
                          tr("Добро пожаловать в GOST-Writer",
                             "Welcome to GOST-Writer"),
                          bg.x + 20.0f, bg.y + 14.0f, ui.palette.text);
            if (ui::button(ui, "wel_lang", Rect{bg.x + w - 150.0f, bg.y + 10.0f, 48.0f, 28.0f},
                           en() ? "RU" : "EN")) {
                toggle_lang();
            }
            if (ui::button(ui, "wel_close_top", Rect{bg.x + w - 94.0f, bg.y + 10.0f, 76.0f, 28.0f},
                           tr("Закрыть", "Close"))) {
                welcome_open = false;
            }

            float y = bg.y + 50.0f;
            {
                std::string intro = tr(
                    "Редактор текстовых документов на базе Markdown с оформлением по ГОСТ Р 2.105–2019 и ГОСТ Р 2.106–2019 (ЕСКД). Позволяет готовить пояснительные записки, спецификации, ведомости и отчёты на листах А4 с рамкой и основной надписью.",
                    "A Markdown-based document editor compliant with GOST R 2.105–2019 and GOST R 2.106–2019 (ESKD). Prepare explanatory notes, specifications, schedules and reports on A4 sheets with frames and stamps.");
                for (const auto& l : wrap_lines(fonts.regular, intro, w - 40.0f)) {
                    ui::draw_text(ui, fonts.regular, l, bg.x + 20.0f, y, ui.palette.text);
                    y += 19.0f;
                }
                y += 8.0f;
            }

            struct Feature {
                const char* title_ru;
                const char* title_en;
                const char* desc_ru;
                const char* desc_en;
            };
            static const Feature feats[] = {
                {"Лист А4, рамка и штамп ГОСТ",
                 "A4 Sheet, Frame & GOST Stamp",
                 "Рамка 185×287 мм, боковой штамп, Форма 2 на первом текстовом листе и Форма 2а на последующих. Поля штампа и титульного листа редактируются кликом.",
                 "185×287 mm frame, side stamp, Form 2 on the first text sheet and Form 2a on subsequent sheets. Click any stamp or title field to edit."},
                {"Титульный лист, содержание и ЛРИ",
                 "Title Page, TOC & Change Sheet",
                 "Кнопки «Титул», «Содержание» и «ЛРИ» включают титульный лист (в т.ч. по ЛУ), автосодержание с отточием и номерами листов и Лист регистрации изменений.",
                 "The Title, TOC and LRI buttons toggle the title page (including by LU), auto-TOC with dot leaders and sheet numbers, and the Change Registration Sheet."},
                {"Шаблоны документов ЕСКД",
                 "ESKD Document Templates",
                 "Меню «Шаблоны ↓»: Пояснительная записка (ПЗ), Спецификация, Ведомость держателей подлинников (ВД), ТП / ЭП / ПТ, Технические условия (ТУ) и РЭ.",
                 "Templates ↓ menu: Explanatory Note (PZ), Specification, Original Holders Schedule (VD), TP / EP / PT tables, Technical Conditions (TU) and Manual (RE)."},
                {"Формулы, диаграммы, графики и сноски",
                 "Formulas, Diagrams, Charts & Footnotes",
                 "Визуальные конструкторы формул (с автонумерацией), чёрно-белых схем и графиков («Рисунок N»), таблиц («Таблица N») и постраничных сносок («Сноска»).",
                 "Visual editors for numbered formulas, B&W diagrams and charts ('Figure N'), tables ('Table N') and per-page footnotes ('Footnote')."},
                {"Прямой экспорт в ГОСТ PDF и DOCX",
                 "Direct GOST PDF & DOCX Export",
                 "Меню «Файл» → «Экспорт ›»: печать в PDF (через встроенный Typst) и экспорт в DOCX/ODT/HTML/EPUB (через встроенный Pandoc) один в один с экраном.",
                 "File → Export ›: print-ready PDF (via bundled Typst) and DOCX/ODT/HTML/EPUB export (via bundled Pandoc) matching the on-screen A4 sheet."},
                {"Удобство работы",
                 "Convenient Workflow",
                 "Поиск и замена (Ctrl+F / Ctrl+H), оглавление (Ctrl+Shift+T), физический масштаб листа А4 (Ctrl+колесо), автосохранение, светлая и тёмная темы, RU/EN.",
                 "Find & replace (Ctrl+F / Ctrl+H), outline (Ctrl+Shift+T), physical A4 zoom (Ctrl+wheel), autosave recovery, light and dark themes, RU/EN."},
            };

            for (const auto& f : feats) {
                if (y + 52.0f > bg.y + h - 54.0f) break;
                const Rect card{bg.x + 18.0f, y, w - 36.0f, 66.0f};
                ui::fill_rounded_rect(ui, card, 6.0f, ui.palette.window_bg);
                ui::stroke_rect(ui, card, ui.palette.panel_border);
                ui::draw_text(ui, fonts.bold, en() ? f.title_en : f.title_ru,
                              card.x + 12.0f, card.y + 6.0f, ui.palette.accent);
                float fy = card.y + 26.0f;
                for (const auto& l : wrap_lines(fonts.regular, en() ? f.desc_en : f.desc_ru, card.w - 24.0f)) {
                    if (fy + 16.0f > card.y + card.h - 2.0f) break;
                    ui::draw_text(ui, fonts.regular, l, card.x + 12.0f, fy, ui.palette.text);
                    fy += 17.0f;
                }
                y += 72.0f;
            }

            const float by = bg.y + h - 42.0f;
            const std::string chk_label = std::string(show_welcome_on_startup ? "[ ] " : "[✓] ")
                + tr("Не показывать при следующем запуске", "Don't show again on startup");
            const float chk_w = text_width(fonts.regular, chk_label) + 24.0f;
            if (ui::toggle_button(ui, "wel_dont_show", {bg.x + 18.0f, by, chk_w, 30.0f},
                                  chk_label.c_str(), !show_welcome_on_startup)) {
                show_welcome_on_startup = !show_welcome_on_startup;
                save_settings();
            }

            const char* start_lbl = tr("Начать работу", "Start Writing");
            const float start_w = std::max(140.0f, text_width(fonts.regular, start_lbl) + 32.0f);
            if (ui::button(ui, "wel_start", {bg.x + w - 18.0f - start_w, by, start_w, 30.0f}, start_lbl)) {
                welcome_open = false;
            }
        }

        ui::stroke_rect(ui, {0, 0, static_cast<float>(width), static_cast<float>(height)},
                        ui.palette.panel_border);
        ui::end_frame(ui);
        SDL_RenderPresent(renderer);

        ui.input.pressed = false;
        ui.input.released = false;
    }

    


    bool handle_event(const SDL_Event& e) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                return !confirm_discard();
            case SDL_EVENT_MOUSE_MOTION:
                ui.input.mouse_x = e.motion.x;
                ui.input.mouse_y = e.motion.y;
                if (scrollbar_dragging && max_scroll > 0.0f) {
                    const float avail_h = scrollbar_track.h - scrollbar_thumb.h;
                    if (avail_h > 1.0f) {
                        const float ratio = (e.motion.y - scrollbar_track.y - scrollbar_drag_offset) / avail_h;
                        scroll_y = std::clamp(ratio * max_scroll, 0.0f, max_scroll);
                    }
                } else if (dragging) {
                    editor_pointer(e.motion.x, e.motion.y, true);
                }
                break;
            case SDL_EVENT_WINDOW_MOVED:
            case SDL_EVENT_WINDOW_RESIZED:
                geometry_dirty = true;
                geometry_tick = SDL_GetTicks();
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ui.input.mouse_x = e.button.x;
                    ui.input.mouse_y = e.button.y;
                    ui.input.down = true;
                    ui.input.pressed = true;
                    if (link_open) {
                        if (ui::contains(link_text_box, e.button.x, e.button.y)) link_field = 0;
                        else if (ui::contains(link_href_box, e.button.x, e.button.y)) link_field = 1;
                        else if (!ui::contains(link_card, e.button.x, e.button.y)) link_open = false;
                        break;
                    }
                    if ((SDL_GetModState() & SDL_KMOD_CTRL) && !welcome_open && !pandoc_help_open) {
                        std::string href;
                        if (link_at(e.button.x, e.button.y, href)) {
                            open_link(href);
                            break;
                        }
                    }
                    if (find_open && ui::contains(find_bar_rect, e.button.x, e.button.y)) {
                        find_click(e.button.x, e.button.y);
                    }
                    if (outline_open && !welcome_open && !pandoc_help_open
                        && outline_click(e.button.x, e.button.y)) {
                        break;
                    }
                    if (welcome_open) {
                        using namespace ui;
                        const float w = std::min(680.0f, static_cast<float>(width) - 32.0f);
                        const float h = std::min(600.0f, static_cast<float>(height) - 32.0f);
                        const Rect bg{(static_cast<float>(width) - w) / 2.0f,
                                      (static_cast<float>(height) - h) / 2.0f, w, h};
                        if (!contains(bg, e.button.x, e.button.y)) {
                            welcome_open = false;
                            break;
                        }
                    }
                    if (pandoc_help_open) {
                        using namespace ui;
                        const float w = 580.0f;
                        const float h = 380.0f;
                        const Rect bg{(static_cast<float>(width)-w)/2.0f, (static_cast<float>(height)-h)/2.0f, w, h};
                        if (contains(bg, e.button.x, e.button.y)) {
                            // click inside modal - let ui handle
                        } else {
                            pandoc_help_open = false;
                            break;
                        }
                    }
                    if (max_scroll > 0.0f && scrollbar_track.w > 0.0f
                        && ui::contains(scrollbar_track, e.button.x, e.button.y)
                        && !ui::pointer_over_popup(ui) && !formula_editor_open
                        && !diag_editor_open && !chart_editor_open
                        && !welcome_open && !pandoc_help_open) {
                        scrollbar_dragging = true;
                        if (ui::contains(scrollbar_thumb, e.button.x, e.button.y)) {
                            scrollbar_drag_offset = e.button.y - scrollbar_thumb.y;
                        } else {
                            scrollbar_drag_offset = scrollbar_thumb.h / 2.0f;
                            const float avail_h = scrollbar_track.h - scrollbar_thumb.h;
                            if (avail_h > 1.0f) {
                                const float ratio = (e.button.y - scrollbar_track.y - scrollbar_drag_offset) / avail_h;
                                scroll_y = std::clamp(ratio * max_scroll, 0.0f, max_scroll);
                            }
                        }
                        break;
                    }
                    if (!ui::pointer_over_popup(ui) && !formula_editor_open
                        && !diag_editor_open && !chart_editor_open
                        && !welcome_open && !pandoc_help_open
                        && e.button.y >= doc_viewport.y && e.button.y <= doc_viewport.y + doc_viewport.h) {
                        bool hit_stamp = false;
                        for (const md::StampSlot& s : stamp_slots) {
                            if (ui::contains(s.rect, e.button.x, e.button.y)) {
                                if (s.action > 0) {
                                    if (s.action == 1) {
                                        doc.meta.title_agrees.push_back(md::TitleSign{"Должность", "И.О. Фамилия", "01.09.2026"});
                                    } else if (s.action == 2 && !doc.meta.title_agrees.empty()) {
                                        doc.meta.title_agrees.pop_back();
                                    } else if (s.action == 3) {
                                        doc.meta.title_approves.push_back(md::TitleSign{"Должность", "И.О. Фамилия", "01.09.2026"});
                                    } else if (s.action == 4 && !doc.meta.title_approves.empty()) {
                                        doc.meta.title_approves.pop_back();
                                    } else if (s.action == 5) {
                                        doc.meta.title_devs.push_back(md::TitleSign{"Должность", "И.О. Фамилия", "01.09.2026"});
                                    } else if (s.action == 6 && !doc.meta.title_devs.empty()) {
                                        doc.meta.title_devs.pop_back();
                                    } else if (s.action == 7) {
                                        doc.meta.title_by_lu = !doc.meta.title_by_lu;
                                    }
                                    clear_stamp_focus();
                                    doc.meta.modified_meta = true;
                                    doc_dirty = true;
                                    mark_edited();
                                    dragging = false;
                                    hit_stamp = true;
                                    break;
                                }
                                active_stamp_field = s.field;
                                active_stamp_target = s.target;
                                find_focus = false;
                                editor.blink_tick = SDL_GetTicks();
                                dragging = false;
                                hit_stamp = true;
                                break;
                            }
                        }
                        if (hit_stamp) break;
                    }
                    if (ui::contains(doc_viewport, e.button.x, e.button.y)
                        && !ui::pointer_over_popup(ui) && !formula_editor_open
                        && !diag_editor_open && !chart_editor_open
                        && !welcome_open && !pandoc_help_open) {
                        clear_stamp_focus();
                        const float cy = e.button.y - doc_viewport.y + scroll_y;
                        // Клик по строке автоматического «Содержания» переходит к выбранному разделу.
                        bool hit_toc = false;
                        for (const md::TocEntry& te : layout.toc_entries) {
                            if (cy >= te.y && cy <= te.y + te.height
                                && te.block >= 0 && static_cast<size_t>(te.block) < doc.blocks.size()) {
                                editor.set_caret(md::Pos{te.block, 0, 0, -1});
                                ensure_caret_visible();
                                dragging = false;
                                hit_toc = true;
                                break;
                            }
                        }
                        if (hit_toc) break;

                        // Клик по маркеру сноски «N)» в основном тексте — открыть
                        // панель редактирования текста этой сноски (п. 6.13 ГОСТ Р 2.105).
                        {
                            const float cy = e.button.y - doc_viewport.y + scroll_y;
                            for (const md::Line& line : layout.lines) {
                                if (cy < line.y - 8.0f || cy > line.y + line.height + 8.0f)
                                    continue;
                                for (const md::Span& span : line.spans) {
                                    if (span.footnote_id <= 0) continue;
                                    const float sx = doc_viewport.x + span.x;
                                    if (e.button.x >= sx - 2.0f
                                        && e.button.x <= sx + span.width + 2.0f
                                        && e.button.y >= doc_viewport.y + line.y - 8.0f
                                        && e.button.y <= doc_viewport.y + line.y + line.height + 8.0f) {
                                        for (const auto& f : doc.footnotes) {
                                            if (f.id == span.footnote_id) {
                                                active_footnote_id = f.id;
                                                footnote_editor_open = true;
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (footnote_editor_open) break;
                            }
                        }

                        // A click on a formula/diagram/chart block opens the visual editor.
                        int hit = -1;
                        for (size_t i = 0; i < doc.blocks.size() && i < layout.boxes.size(); ++i) {
                            const auto& box = layout.boxes[i];
                            if (cy >= box.y && cy <= box.y + box.h) {
                                hit = static_cast<int>(i);
                                break;
                            }
                        }
                        // Double-click below the last block (or at the bottom when the last block
                        // is a code/formula/diagram/chart/table/rule block) appends a new paragraph
                        // and moves the caret into it so the user is never trapped.
                        if (e.button.clicks >= 2 && !doc.blocks.empty()) {
                            const float doc_bottom = layout.boxes.empty()
                                ? 0.0f
                                : (layout.boxes.back().y + layout.boxes.back().h);
                            const md::Block& last_blk = doc.blocks.back();
                            const bool last_is_non_para = last_blk.type != md::BlockType::Paragraph;
                            if (cy > doc_bottom - 4.0f || (hit == -1 && cy >= doc_bottom - 20.0f)
                                || (last_is_non_para && cy >= doc_bottom - 12.0f)) {
                                md::Block para;
                                para.type = md::BlockType::Paragraph;
                                para.inlines.push_back(md::Inline{"", 0, ""});
                                doc.blocks.push_back(std::move(para));
                                const int new_idx = static_cast<int>(doc.blocks.size()) - 1;
                                editor.set_caret(md::Pos{new_idx, 0, 0, -1});
                                mark_edited();
                                md::layout_document(doc, fontbook, laid_out_width > 0.0f ? laid_out_width : 760.0f, layout, math_reveal_shown);
                                max_scroll = std::max(0.0f, layout.content_height - doc_viewport.h + 24.0f);
                                ensure_caret_visible();
                                dragging = false;
                                break;
                            }
                        }
                        bool opened = false;
                        if (hit >= 0 && hit < static_cast<int>(doc.blocks.size())) {
                            const md::Block& blk = doc.blocks[hit];
                            if (blk.type == md::BlockType::CodeBlock
                                && (blk.lang == "math" || blk.lang == "latex" || blk.lang == "tex")) {
                                formula_open_block(hit);
                                opened = true;
                                // The panel appears under the cursor mid-click;
                                // swallow this click so it can't hit a panel button.
                                formula_just_opened = true;
                                ui.input.pressed = false;
                                ui.active_id.clear();
                            } else if (blk.type == md::BlockType::CodeBlock
                                       && md::is_diagram_lang(blk.lang)) {
                                std::string code;
                                for (const md::Inline& inl : blk.inlines) code += inl.text;
                                if (md::is_chart_text(code)) {
                                    if (chart_open_block(hit)) {
                                        opened = true;
                                        chart_just_opened = true;
                                        ui.input.pressed = false;
                                        ui.active_id.clear();
                                    }
                                } else if (diag_open_block(hit)) {
                                    // A click on a diagram block opens its editor,
                                    // swallowed the same way.
                                    opened = true;
                                    diag_just_opened = true;
                                    ui.input.pressed = false;
                                    ui.active_id.clear();
                                }
                            }
                        }
                        if (!opened) {
                            const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                            editor_pointer(e.button.x, e.button.y, shift);
                            dragging = true;
                        }
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    if (ui::contains(doc_viewport, e.button.x, e.button.y)
                        && !ui::pointer_over_popup(ui)) {
                        const float dx = e.button.x;
                        const float dy = e.button.y;
                        // find block under cursor
                        const float cy = dy - doc_viewport.y + scroll_y;
                        int hit_block = -1;
                        for (size_t i = 0; i < doc.blocks.size() && i < layout.boxes.size(); ++i) {
                            const auto& box = layout.boxes[i];
                            if (cy >= box.y && cy <= box.y + box.h) {
                                hit_block = (int)i;
                                break;
                            }
                        }
                        if (hit_block >= 0 && hit_block < (int)doc.blocks.size()) {
                            const auto& blk = doc.blocks[hit_block];
                            if (blk.type == md::BlockType::CodeBlock) {
                                const std::string& l = blk.lang;
                                if (l == "math" || l == "latex" || l == "tex" || l == "mermaid"
                                    || l == "plantuml" || l == "puml" || l == "dot" || l == "graphviz") {
                                    std::vector<std::string> items;
                                    if (l == "mermaid" || l == "plantuml" || l == "puml" || l == "dot" || l == "graphviz") {
                                        for (const auto& t : kDiagramTemplates) items.push_back(std::string("Шаблон: ") + t.label_ru);
                                    }
                                    if (l == "math" || l == "latex" || l == "tex") {
                                        for (const auto& t : kMathTemplates) items.push_back(std::string("Шаблон: ") + t.label_ru);
                                    }
                                    items.push_back("Очистить блок");
                                    items.push_back("Вставить $...$ inline");
                                    // store context
                                    struct Ctx { int block; std::string lang; };
                                    static Ctx ctxb = {-1, ""};
                                    ctxb.block = hit_block; ctxb.lang = l;
                                    // use a simple approach: show via dropdown? easier: just handle by pushing action
                                    // but we don't have ad-hoc menu; trigger by setting a flag - quick hack
                                    // open a small popup by reusing dropdown logic not directly possible; instead just apply first action? no
                                    // better: remember and handle in frame - store in app
                                    // (we'll add members)
                                    extern App* g_app;
                                    if (g_app) {
                                        g_app->ctx_menu_block = hit_block;
                                        g_app->ctx_menu_lang = l;
                                        g_app->ctx_menu_x = dx; g_app->ctx_menu_y = dy;
                                        g_app->show_ctx_menu = true;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    ui.input.mouse_x = e.button.x;
                    ui.input.mouse_y = e.button.y;
                    ui.input.down = false;
                    // Swallow the release of the click that opened an editor.
                    if (formula_just_opened || diag_just_opened || chart_just_opened) {
                        formula_just_opened = false;
                        diag_just_opened = false;
                        chart_just_opened = false;
                        ui.input.released = false;
                    } else {
                        ui.input.released = true;
                    }
                    dragging = false;
                    scrollbar_dragging = false;
                }
                break;
            case SDL_EVENT_DROP_BEGIN:
                pending_drops.clear();
                drop_gesture_active = true;
                break;
            case SDL_EVENT_DROP_FILE:
                if (e.drop.data != nullptr) {
                    if (!drop_gesture_active) {
                        // In case a backend emits DROP_FILE without DROP_BEGIN, process immediately
                        std::string dropped(e.drop.data);
                        if (!has_supported_markdown_ext(dropped)) {
                            dialogs::show_error(tr("Поддерживаются только файлы .md, .markdown и .txt",
                                                   "Only .md, .markdown, and .txt files are supported"));
                        } else {
                            open_from_path(dropped, true, true);
                        }
                    } else {
                        pending_drops.emplace_back(e.drop.data);
                    }
                }
                break;
            case SDL_EVENT_DROP_COMPLETE:
                if (drop_gesture_active) {
                    drop_gesture_active = false;
                    std::vector<std::string> files = std::move(pending_drops);
                    pending_drops.clear();
                    if (files.size() > 1) {
                        dialogs::show_error(tr("Можно перетащить только один файл за раз",
                                               "Please drop only one file at a time"));
                        status = tr("Отклонено: несколько файлов", "Rejected: multiple files");
                    } else if (files.size() == 1) {
                        std::error_code ec;
                        const std::filesystem::path fsp = std::filesystem::u8path(files[0]);
                        if (std::filesystem::is_directory(fsp, ec) || !has_supported_markdown_ext(files[0])) {
                            dialogs::show_error(tr("Поддерживаются только файлы .md, .markdown и .txt",
                                                   "Only .md, .markdown, and .txt files are supported"));
                            status = tr("Неподдерживаемый тип файла", "Unsupported file type");
                        } else {
                            open_from_path(files[0], true, true);
                        }
                    }
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (e.text.text[0] == '\t') break; // Tab is a key here, never typed text
                if (link_open) {
                    (link_field == 0 ? link_text_value : link_href_value) += e.text.text;
                    break;
                }
                if (find_open && find_focus) {
                    find_text_input(e.text.text);
                    break;
                }
                if (welcome_open || pandoc_help_open) {
                    break;
                }
                if (formula_editor_open) {
                    formula_insert_symbol(e.text.text);
                    break;
                }
                if (diag_editor_open) {
                    diag_insert_text(e.text.text);
                    break;
                }
                if (footnote_editor_open && active_footnote_id > 0) {
                    for (auto& f : doc.footnotes) {
                        if (f.id == active_footnote_id) {
                            f.text += e.text.text;
                            doc_dirty = true;
                            break;
                        }
                    }
                    break;
                }
                if (chart_editor_open) {
                    chart_insert_text(e.text.text);
                    break;
                }
                if (is_editing_stamp()) {
                    if (std::string* sf = stamp_field_str(active_stamp_field)) {
                        *sf += e.text.text;
                        doc.meta.modified_meta = true;
                        editor.blink_tick = SDL_GetTicks();
                        mark_edited();
                    }
                    break;
                }
                editor.insert_text(e.text.text);
                mark_edited();
                ensure_caret_visible();
                break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_F1) {
                    welcome_open = !welcome_open;
                    break;
                }
                if (footnote_editor_open && active_footnote_id > 0) {
                    if (e.key.key == SDLK_BACKSPACE) {
                        for (auto& f : doc.footnotes) {
                            if (f.id == active_footnote_id) {
                                if (!f.text.empty() && f.text.size() > 1) {
                                    // Удаляем последний UTF-8 символ, а не один байт.
                                    size_t cut = f.text.size() - 1;
                                    while (cut > 0 && (static_cast<unsigned char>(f.text[cut]) & 0xC0) == 0x80) {
                                        --cut;
                                    }
                                    f.text.erase(cut);
                                }
                                doc_dirty = true;
                                break;
                            }
                        }
                        break;
                    }
                    if (e.key.key == SDLK_ESCAPE) {
                        footnote_editor_open = false;
                        active_footnote_id = 0;
                        break;
                    }
                    break;
                }
                if (footnote_editor_open && active_footnote_id > 0) {
                    for (auto& f : doc.footnotes) {
                        if (f.id == active_footnote_id) {
                            f.text += e.text.text;
                            doc_dirty = true;
                            break;
                        }
                    }
                    break;
                }
                if (link_open) {
                    link_key(e.key.key, e.key.mod);
                    break;
                }
                if (e.key.key == SDLK_ESCAPE) {
                    if (find_open) {
                        close_find();
                        break;
                    }
                    if (welcome_open) {
                        welcome_open = false;
                        break;
                    }
                    if (pandoc_help_open) {
                        pandoc_help_open = false;
                        break;
                    }
                    if (formula_editor_open) {
                        formula_editor_open = false;
                        status = tr("Редактор формул закрыт", "Formula editor closed");
                        break;
                    }
                    if (diag_editor_open) {
                        diag_editor_open = false;
                        status = tr("Редактор диаграмм закрыт", "Diagram editor closed");
                        break;
                    }
                    if (chart_editor_open) {
                        chart_editor_open = false;
                        status = tr("Редактор графика закрыт", "Chart editor closed");
                        break;
                    }
                    if (is_editing_stamp()) {
                        clear_stamp_focus();
                        break;
                    }
                    ui.open_popup.clear();
                    break;
                }
                if (find_open && find_focus) {
                    find_key(e.key);
                    break;
                }
                if (welcome_open || pandoc_help_open) {
                    if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) {
                        welcome_open = false;
                        pandoc_help_open = false;
                    }
                    break;
                }
                if (formula_editor_open) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    switch (e.key.key) {
                        case SDLK_BACKSPACE: formula_backspace(); break;
                        case SDLK_DELETE: formula_delete_element(); break;
                        case SDLK_TAB: formula_move_slot(shift ? -1 : 1); break;
                        case SDLK_LEFT:
                            if (formula_off > 0) {
                                const md::MathNode* leaf =
                                    md::node_at_path(formula_edit, formula_path);
                                if (leaf && leaf->type == md::MathNode::Text) {
                                    size_t p = static_cast<size_t>(formula_off);
                                    --p;
                                    while (p > 0 && (static_cast<unsigned char>(leaf->text[p]) & 0xC0) == 0x80) --p;
                                    formula_off = static_cast<int>(p);
                                }
                            } else {
                                formula_move_slot(-1);
                            }
                            break;
                        case SDLK_RIGHT: {
                            const md::MathNode* leaf =
                                md::node_at_path(formula_edit, formula_path);
                            if (leaf && leaf->type == md::MathNode::Text
                                && formula_off < static_cast<int>(leaf->text.size())) {
                                size_t p = static_cast<size_t>(formula_off) + 1;
                                while (p < leaf->text.size()
                                       && (static_cast<unsigned char>(leaf->text[p]) & 0xC0) == 0x80) ++p;
                                formula_off = static_cast<int>(p);
                            } else {
                                formula_move_slot(1);
                            }
                            break;
                        }
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            formula_apply();
                            break;
                        default: break;
                    }
                    break;
                }
                if (diag_editor_open) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    switch (e.key.key) {
                        case SDLK_BACKSPACE: diag_backspace(); break;
                        case SDLK_DELETE: diag_delete_element(); break;
                        case SDLK_TAB: diag_move_slot(shift ? -1 : 1); break;
                        case SDLK_LEFT:
                            if (diag_off > 0) {
                                std::string* t = diag_sel_text();
                                if (t != nullptr) {
                                    size_t p = static_cast<size_t>(diag_off);
                                    --p;
                                    while (p > 0
                                           && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                        --p;
                                    }
                                    diag_off = static_cast<int>(p);
                                }
                            } else {
                                diag_move_slot(-1);
                            }
                            break;
                        case SDLK_RIGHT: {
                            std::string* t = diag_sel_text();
                            if (t != nullptr && diag_off < static_cast<int>(t->size())) {
                                size_t p = static_cast<size_t>(diag_off) + 1;
                                while (p < t->size()
                                       && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                    ++p;
                                }
                                diag_off = static_cast<int>(p);
                            } else {
                                diag_move_slot(1);
                            }
                            break;
                        }
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            diag_apply();
                            break;
                        default: break;
                    }
                    break;
                }
                if (chart_editor_open) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    switch (e.key.key) {
                        case SDLK_BACKSPACE: chart_backspace(); break;
                        case SDLK_DELETE:
                            {
                                std::string* t = chart_sel_text();
                                if (t != nullptr && !t->empty() && chart_off < static_cast<int>(t->size())) {
                                    size_t p = static_cast<size_t>(chart_off);
                                    while (p < t->size() && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) ++p;
                                    size_t q = p + 1;
                                    while (q < t->size() && (static_cast<unsigned char>((*t)[q]) & 0xC0) == 0x80) ++q;
                                    t->erase(p, q - p);
                                    chart_off = static_cast<int>(p);
                                } else {
                                    chart_delete_row();
                                }
                            }
                            break;
                        case SDLK_TAB: chart_move_slot(shift ? -1 : 1); break;
                        case SDLK_LEFT:
                            if (chart_off > 0) {
                                std::string* t = chart_sel_text();
                                if (t != nullptr) {
                                    size_t p = static_cast<size_t>(chart_off);
                                    --p;
                                    while (p > 0
                                           && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                        --p;
                                    }
                                    chart_off = static_cast<int>(p);
                                }
                            } else {
                                chart_move_slot(-1);
                            }
                            break;
                        case SDLK_RIGHT: {
                            std::string* t = chart_sel_text();
                            if (t != nullptr && chart_off < static_cast<int>(t->size())) {
                                size_t p = static_cast<size_t>(chart_off) + 1;
                                while (p < t->size()
                                       && (static_cast<unsigned char>((*t)[p]) & 0xC0) == 0x80) {
                                    ++p;
                                }
                                chart_off = static_cast<int>(p);
                            } else {
                                chart_move_slot(1);
                            }
                            break;
                        }
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            chart_apply();
                            break;
                        default: break;
                    }
                    break;
                }
                if (is_editing_stamp()) {
                    const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                    const bool ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0;
                    if (ctrl && e.key.key == SDLK_S) {
                        if (shift) save_as();
                        else save();
                        break;
                    }
                    if (ctrl && e.key.key == SDLK_Z) {
                        apply_history(shift);
                        break;
                    }
                    if (ctrl && e.key.key == SDLK_Y) {
                        apply_history(true);
                        break;
                    }
                    switch (e.key.key) {
                        case SDLK_BACKSPACE:
                            if (std::string* sf = stamp_field_str(active_stamp_field)) {
                                if (!sf->empty()) {
                                    const int prev = md::prev_char(*sf, static_cast<int>(sf->size()));
                                    sf->erase(static_cast<size_t>(prev));
                                    doc.meta.modified_meta = true;
                                    editor.blink_tick = SDL_GetTicks();
                                    mark_edited();
                                }
                            }
                            break;
                        case SDLK_TAB:
                            step_stamp_field(shift ? -1 : 1);
                            break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            step_stamp_field(1);
                            break;
                        default: break;
                    }
                    break;
                }
                handle_key(e.key);
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                float dy = e.wheel.y;
                if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
                if (dy == 0.0f) break;
                float my = 0.0f;
                SDL_GetMouseState(nullptr, &my);
                if (SDL_GetModState() & SDL_KMOD_CTRL) {
                    // Ctrl + wheel zooms the document in 10% steps.
                    const float step = 0.1f;
                    zoom_to(zoom + (dy > 0.0f ? step : -step), my);
                } else {
                    scroll_y = std::clamp(scroll_y - dy * 48.0f, 0.0f, max_scroll);
                }
                break;
            }
            default:
                break;
        }
        return true;
    }


    

    // it is painted; the second pass draws and handles clicks.





};



App* g_app = nullptr;

static void push_mouse_event(SDL_EventType type, float x, float y) {
    SDL_Event e{};
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.type = type;
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.type = type;
        e.button.x = x;
        e.button.y = y;
        e.button.button = SDL_BUTTON_LEFT;
    }
    SDL_PushEvent(&e);
}

static void push_wheel_event(float dy) {
    SDL_Event e{};
    e.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.y = dy;
    SDL_PushEvent(&e);
}

static void push_key_event(SDL_Keycode key, SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = key;
    e.key.mod = mod;
    SDL_PushEvent(&e);
}

} // namespace


int main(int argc, char** argv) {
    const bool selftest = argc > 1 && std::string(argv[1]) == "--selftest";
    const std::string screenshot = argc > 2 ? argv[2] : "selftest.bmp";

    App app;
    app.selftest_mode = selftest;
    g_app = &app;
    if (!app.init()) {
        app.shutdown();
        return 1;
    }
    std::printf("init ok: SDL %s, fonts + Cyrillic ok\n", SDL_GetRevision());

    if (selftest) {
        auto pump = [&]() {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                app.dispatch(e);
            }
        };
        auto save_frame = [&](const std::string& path) {
            app.frame();
            SDL_Surface* pixels = SDL_RenderReadPixels(app.renderer, nullptr);
            if (!pixels) {
                std::fprintf(stderr, "read pixels: %s\n", SDL_GetError());
                return false;
            }
            const bool saved = SDL_SaveBMP(pixels, path.c_str());
            SDL_DestroySurface(pixels);
            if (!saved) std::fprintf(stderr, "save bmp: %s\n", SDL_GetError());
            return saved;
        };

        app.ui_lang = 0;
        app.light_theme = false;
        app.ui.palette = ui::dark_palette();
        app.welcome_open = true;
        for (int i = 0; i < 5; ++i) {
            pump();
            app.frame();
        }
        const auto wdot = screenshot.find_last_of('.');
        const std::string welcome_path = wdot == std::string::npos
            ? screenshot + "_welcome"
            : screenshot.substr(0, wdot) + "_welcome" + screenshot.substr(wdot);
        if (!save_frame(welcome_path)) {
            app.shutdown();
            return 1;
        }
        // Close welcome modal via Escape and verify it closes
        push_key_event(SDLK_ESCAPE);
        pump();
        app.frame();
        if (app.welcome_open) {
            std::fprintf(stderr, "selftest: welcome modal did not close on Escape\n");
            app.shutdown();
            return 1;
        }
        if (!save_frame(screenshot)) {
            app.shutdown();
            return 1;
        }
        std::printf("selftest frame ok: %dx%d, screenshot %s\n", app.width, app.height,
                    screenshot.c_str());

        const auto dot = screenshot.find_last_of('.');
        app.dialect = 1;

        // Selection + caret overlay screenshot.
        app.editor.set_caret(md::Pos{2, 0, 0});
        app.editor.set_caret(md::Pos{4, 1, 5}, true);
        app.editor.blink_tick = SDL_GetTicks();
        const std::string sel_path = dot == std::string::npos
            ? screenshot + "_select"
            : screenshot.substr(0, dot) + "_select" + screenshot.substr(dot);
        if (!save_frame(sel_path)) {
            app.shutdown();
            return 1;
        }

        // Wheel scroll moves the document viewport.
        push_wheel_event(-1.0f);
        pump();
        app.frame();
        if (app.scroll_y <= 0.0f) {
            std::fprintf(stderr, "selftest: scroll failed (height=%.0f)\n",
                         app.layout.content_height);
            app.shutdown();
            return 1;
        }
        std::printf("selftest scroll ok: offset=%.0f, lines=%zu, height=%.0f\n",
                    app.scroll_y, app.layout.lines.size(), app.layout.content_height);

        // Editing through the public editor API.
        auto& ed = app.editor;
        ed.click(app.layout, app.fontbook, 5.0f, 5.0f, false);
        ed.move(app.layout, app.fontbook, md::Editor::Move::DocStart, false);
        ed.insert_text("Тест ");
        if (app.doc.blocks[0].inlines[0].text.rfind("Тест ", 0) != 0) {
            std::fprintf(stderr, "selftest: insert failed\n");
            app.shutdown();
            return 1;
        }
        const size_t blocks_before = app.doc.blocks.size();
        ed.newline();
        if (app.doc.blocks.size() != blocks_before + 1) {
            std::fprintf(stderr, "selftest: block split failed\n");
            app.shutdown();
            return 1;
        }
        ed.backspace();
        if (app.doc.blocks.size() != blocks_before) {
            std::fprintf(stderr, "selftest: block merge failed\n");
            app.shutdown();
            return 1;
        }
        // Consecutive typing must not leave a selection behind.
        ed.set_caret(md::Pos{1, 0, 0});
        ed.insert_text("a");
        ed.insert_text("b");
        ed.insert_text("c");
        if (ed.has_selection() || app.doc.blocks[1].inlines[0].text.rfind("abc", 0) != 0) {
            std::fprintf(stderr, "selftest: consecutive typing failed (%s)\n",
                         app.doc.blocks[1].inlines[0].text.c_str());
            app.shutdown();
            return 1;
        }

        // Backspace/Delete across inline (style) boundaries.
        // Block 1: plain("Лёгкий ") + bold("WYSIWYG") + plain(...).
        ed.set_caret(md::Pos{1, 2, 0});
        ed.backspace();
        if (app.doc.blocks[1].inlines[1].text != "WYSIWY") {
            std::fprintf(stderr, "selftest: backspace at inline start failed (%s)\n",
                         app.doc.blocks[1].inlines[1].text.c_str());
            app.shutdown();
            return 1;
        }
        ed.set_caret(md::Pos{1, 0,
                     static_cast<int>(app.doc.blocks[1].inlines[0].text.size())});
        ed.delete_forward();
        if (app.doc.blocks[1].inlines[1].text != "YSIWY") {
            std::fprintf(stderr, "selftest: delete at inline end failed (%s)\n",
                         app.doc.blocks[1].inlines[1].text.c_str());
            app.shutdown();
            return 1;
        }
        std::printf("selftest inline boundaries ok\n");

        // Bold on a selection, then removed again: runs split and merge back.
        ed.set_caret(md::Pos{1, 0, 0});
        ed.set_caret(md::Pos{1, 0, 3}, true);
        ed.toggle_style(md::StyleBold);
        {
            const auto& il = app.doc.blocks[1].inlines;
            if (il[0].text != "abc" || !(il[0].style & md::StyleBold)
                || il[1].text.rfind("Лёгкий", 0) != 0 || (il[1].style & md::StyleBold)) {
                std::fprintf(stderr, "selftest: bold on selection failed\n");
                app.shutdown();
                return 1;
            }
        }
        if (!(ed.active_style() & md::StyleBold)) {
            std::fprintf(stderr, "selftest: active style on selection failed\n");
            app.shutdown();
            return 1;
        }
        ed.toggle_style(md::StyleBold);
        if (app.doc.blocks[1].inlines[0].style != 0
            || app.doc.blocks[1].inlines[0].text.rfind("abcЛёгкий", 0) != 0) {
            std::fprintf(stderr, "selftest: style removal/merge failed\n");
            app.shutdown();
            return 1;
        }

        // Toggle without a selection styles the next typed text only.
        ed.set_caret(md::Pos{1, 0, 0});
        ed.toggle_style(md::StyleItalic);
        if (!(ed.active_style() & md::StyleItalic)) {
            std::fprintf(stderr, "selftest: pending style not reported\n");
            app.shutdown();
            return 1;
        }
        ed.insert_text("Z");
        ed.insert_text("Y");
        {
            const auto& il = app.doc.blocks[1].inlines;
            if (il[0].text != "ZY" || !(il[0].style & md::StyleItalic)
                || il[1].text.rfind("abc", 0) != 0 || (il[1].style & md::StyleItalic)
                || ed.has_selection()) {
                std::fprintf(stderr, "selftest: pending style on typing failed\n");
                app.shutdown();
                return 1;
            }
        }
        std::printf("selftest inline styles ok\n");

        // Italic over a whole paragraph keeps its bold runs.
        {
            const auto& blk = app.doc.blocks[3];
            ed.set_caret(md::Pos{3, 0, 0});
            ed.set_caret(md::Pos{3, static_cast<int>(blk.inlines.size()) - 1,
                                 static_cast<int>(blk.inlines.back().text.size())}, true);
            ed.toggle_style(md::StyleItalic);
            app.doc_dirty = true;
            ed.set_caret(md::Pos{0, 0, 0});
            const std::string styles_path = dot == std::string::npos
                ? screenshot + "_styles"
                : screenshot.substr(0, dot) + "_styles" + screenshot.substr(dot);
            if (!save_frame(styles_path)) {
                app.shutdown();
                return 1;
            }
        }

        // Block operations.
        {
            auto fail = [&](const char* what) {
                std::fprintf(stderr, "selftest: %s\n", what);
                app.shutdown();
                return 1;
            };
            auto& d = app.doc;

            ed.set_caret(md::Pos{1, 0, 0});
            ed.set_block(md::BlockType::Heading, 2);
            if (d.blocks[1].type != md::BlockType::Heading || d.blocks[1].level != 2) {
                return fail("heading on paragraph failed");
            }
            ed.set_block(md::BlockType::Heading, 2);
            if (d.blocks[1].type != md::BlockType::Paragraph) {
                return fail("heading toggle off failed");
            }

            // Ordered list over blocks 1..2, then Enter continues it.
            ed.set_caret(md::Pos{1, 0, 0});
            ed.set_caret(md::Pos{2, 0, 0}, true);
            ed.set_block(md::BlockType::ListItem, 0, true);
            md::layout_document(d, app.fontbook, 760.0f, app.layout);
            if (app.layout.markers[1] != "1." || app.layout.markers[2] != "2.") {
                return fail("ordered list numbering failed");
            }
            ed.set_caret(md::Pos{2, 0, static_cast<int>(d.blocks[2].inlines[0].text.size())});
            ed.newline();
            if (d.blocks[3].type != md::BlockType::ListItem || !d.blocks[3].ordered) {
                return fail("list continuation on Enter failed");
            }
            // Enter on the empty item leaves the list.
            ed.newline();
            if (d.blocks[3].type != md::BlockType::Paragraph) {
                return fail("empty list item exit failed");
            }

            // Nesting with Tab, then numbering restarts at depth 1.
            ed.set_caret(md::Pos{2, 0, 0});
            if (!ed.indent(1) || d.blocks[2].depth != 1) {
                return fail("indent failed");
            }
            md::layout_document(d, app.fontbook, 760.0f, app.layout);
            if (app.layout.markers[2] != "1.") {
                return fail("nested numbering failed");
            }
            ed.indent(-1);

            // Multi-digit numbering: the marker must never be clipped at the
            // left edge of the content column (indent follows marker width).
            {
                md::Document wide;
                for (int i = 0; i < 15; ++i) {
                    md::Block b;
                    b.type = md::BlockType::ListItem;
                    b.ordered = true;
                    b.inlines.push_back(md::Inline{"пункт", 0, ""});
                    wide.blocks.push_back(std::move(b));
                }
                md::Layout wl;
                md::layout_document(wide, app.fontbook, 760.0f, wl);
                bool wide_ok = true;
                for (const md::Line& ln : wl.lines) {
                    if (ln.block < 0 || static_cast<size_t>(ln.block) >= wl.markers.size()) continue;
                    const std::string& mk = wl.markers[static_cast<size_t>(ln.block)];
                    if (mk.empty()) continue;
                    TTF_Font* mf = app.fontbook.get(md::Family::Sans, false, false, 17.0f);
                    int mw = 0, mh = 0;
                    TTF_GetStringSize(mf, mk.c_str(), mk.size(), &mw, &mh);
                    if (ln.x0 - 10.0f - static_cast<float>(mw) < 0.0f) {
                        wide_ok = false;
                        std::fprintf(stderr, "selftest: list marker '%s' clipped (x0=%.1f mw=%d)\n",
                                     mk.c_str(), ln.x0, mw);
                    }
                }
                if (!wide_ok || wl.markers.back() != "15.") {
                    return fail("multi-digit list marker indent failed");
                }
            }
            if (d.blocks[2].depth != 0) {
                return fail("outdent failed");
            }

            // Rule on the blank paragraph 3, paragraph after it.
            ed.set_caret(md::Pos{3, 0, 0});
            ed.insert_rule();
            if (d.blocks[3].type != md::BlockType::Rule
                || d.blocks[4].type != md::BlockType::Paragraph
                || !(ed.caret() == md::Pos{4, 0, 0})) {
                return fail("insert_rule failed");
            }

            // Code block toggle on paragraph 4 and back.
            ed.set_block(md::BlockType::CodeBlock);
            if (d.blocks[4].type != md::BlockType::CodeBlock) {
                return fail("code block failed");
            }
            ed.set_block(md::BlockType::CodeBlock);

            // Backspace at the start of a list item turns it into a paragraph.
            ed.set_caret(md::Pos{1, 0, 0});
            ed.backspace();
            if (d.blocks[1].type != md::BlockType::Paragraph) {
                return fail("backspace unformat failed");
            }

            app.doc_dirty = true;
            const std::string blocks_path = dot == std::string::npos
                ? screenshot + "_blocks"
                : screenshot.substr(0, dot) + "_blocks" + screenshot.substr(dot);
            ed.set_caret(md::Pos{0, 0, 0});
            if (!save_frame(blocks_path)) {
                app.shutdown();
                return 1;
            }
            std::printf("selftest block operations ok\n");
        }

        ed.select_all();
        if (ed.selected_text().empty()) {
            std::fprintf(stderr, "selftest: select all failed\n");
            app.shutdown();
            return 1;
        }
        ed.delete_selection();
        if (app.doc.blocks.size() != 1) {
            std::fprintf(stderr, "selftest: delete selection failed\n");
            app.shutdown();
            return 1;
        }

        // hit_test and locate_caret agree at the document start.
        md::layout_document(app.doc, app.fontbook, 760.0f, app.layout);
        ed.move(app.layout, app.fontbook, md::Editor::Move::DocStart, false);
        float cx = 0;
        const int li = md::locate_caret(app.doc, app.layout, app.fontbook, ed.caret(), &cx);
        if (li < 0) {
            std::fprintf(stderr, "selftest: locate_caret failed\n");
            app.shutdown();
            return 1;
        }
        const md::Pos p = md::hit_test(app.doc, app.layout, app.fontbook, cx,
                                       app.layout.lines[li].y + 2.0f);
        if (!(p == ed.caret())) {
            std::fprintf(stderr, "selftest: hit/locate roundtrip failed\n");
            app.shutdown();
            return 1;
        }
        // Key-driven rule editing, a frame after every key like the real loop.
        const SDL_Keymod ctrl_shift = static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
        auto press = [&](SDL_Keycode key, SDL_Keymod mod) {
            push_key_event(key, mod);
            pump();
            app.frame();
        };
        auto has_rule = [&]() {
            for (const md::Block& b : app.doc.blocks) {
                if (b.type == md::BlockType::Rule) return true;
            }
            return false;
        };

        ed.insert_text("alpha");
        press(SDLK_RETURN, SDL_KMOD_NONE);
        press(SDLK_R, ctrl_shift);
        if (!has_rule()) {
            std::fprintf(stderr, "selftest: rule insert by keys failed\n");
            app.shutdown();
            return 1;
        }
        press(SDLK_BACKSPACE, SDL_KMOD_NONE);
        if (has_rule() || app.doc.blocks.size() != 2) {
            std::fprintf(stderr, "selftest: backspace did not remove rule\n");
            app.shutdown();
            return 1;
        }
        press(SDLK_R, ctrl_shift);
        ed.set_caret(md::Pos{1, 0, 0});
        press(SDLK_DELETE, SDL_KMOD_NONE);
        if (has_rule() || app.doc.blocks.size() != 2) {
            std::fprintf(stderr, "selftest: delete did not remove rule\n");
            app.shutdown();
            return 1;
        }
        std::printf("selftest rule keys ok: blocks=%zu\n", app.doc.blocks.size());

        // Markdown import/export. Every fixture must reach a fixed point after one
        // round trip, and the structure of the basic fixture must match the source.
        {
            const std::string fixtures_dir = std::string(PROJECT_SOURCE_DIR) + "/tests/fixtures/";
            struct Fixture { const char* name; bool gfm; };
            const Fixture fixtures[] = {{"basic.md", false}, {"escapes.md", false},
                                        {"gfm.md", true}, {"tables.md", true}};

            for (const Fixture& fx : fixtures) {
                std::string src;
                if (!md::read_text_file(fixtures_dir + fx.name, src)) {
                    std::fprintf(stderr, "selftest: fixture %s missing\n", fx.name);
                    app.shutdown();
                    return 1;
                }
                md::Document d1, d2;
                md::import_markdown(src, fx.gfm, d1);
                const std::string first = md::export_markdown(d1, fx.gfm);
                md::import_markdown(first, fx.gfm, d2);
                const std::string second = md::export_markdown(d2, fx.gfm);
                if (first != second) {
                    std::fprintf(stderr, "selftest: %s is not stable:\n--- first\n%s\n--- second\n%s\n",
                                 fx.name, first.c_str(), second.c_str());
                    app.shutdown();
                    return 1;
                }
                md::write_text_file(std::string(PROJECT_SOURCE_DIR) + "/build/export_" + fx.name,
                                    first);
            }

            md::Document basic;
            std::string basic_src;
            md::read_text_file(fixtures_dir + "basic.md", basic_src);
            md::import_markdown(basic_src, false, basic);

            auto has_block = [&](auto pred) {
                for (const md::Block& b : basic.blocks) {
                    if (pred(b)) return true;
                }
                return false;
            };
            auto has_inline = [&](auto pred) {
                for (const md::Block& b : basic.blocks) {
                    for (const md::Inline& inl : b.inlines) {
                        if (pred(inl)) return true;
                    }
                }
                return false;
            };

            const bool ok =
                basic.blocks[0].type == md::BlockType::Heading && basic.blocks[0].level == 1
                && has_block([](const md::Block& b) {
                    return b.type == md::BlockType::ListItem && b.depth == 1;
                })
                && has_block([](const md::Block& b) {
                    return b.type == md::BlockType::ListItem && b.ordered;
                })
                && has_block([](const md::Block& b) { return b.type == md::BlockType::Quote; })
                && has_block([](const md::Block& b) {
                    return b.type == md::BlockType::CodeBlock
                        && b.inlines[0].text.find("return 0;") != std::string::npos;
                })
                && has_block([](const md::Block& b) { return b.type == md::BlockType::Rule; })
                && has_inline([](const md::Inline& i) {
                    return (i.style & md::StyleLink) && i.href == "https://example.com";
                })
                && has_inline([](const md::Inline& i) {
                    return (i.style & md::StyleBold) && (i.style & md::StyleItalic);
                });
            if (!ok) {
                std::fprintf(stderr, "selftest: basic.md structure mismatch\n");
                app.shutdown();
                return 1;
            }

            md::Document esc;
            std::string esc_src;
            md::read_text_file(fixtures_dir + "escapes.md", esc_src);
            md::import_markdown(esc_src, false, esc);
            const bool decoded = esc.blocks.size() >= 3
                && esc.blocks[1].inlines[0].text.find("C:\\temp") != std::string::npos
                && esc.blocks[1].inlines[0].text.find("Два * три") != std::string::npos
                && esc.blocks[4].inlines[0].text.find("Слово & сущность") != std::string::npos
                && esc.blocks[4].inlines[0].text.find("Привет") != std::string::npos;
            if (!decoded) {
                std::fprintf(stderr, "selftest: escapes/entities not decoded\n");
                app.shutdown();
                return 1;
            }
            auto fixture_text = [&](const char* name) {
                std::string text;
                md::read_text_file(fixtures_dir + name, text);
                return text;
            };
            const bool detect_ok = md::looks_like_gfm(fixture_text("gfm.md"))
                && !md::looks_like_gfm(fixture_text("basic.md"))
                && !md::looks_like_gfm(fixture_text("escapes.md"))
                && !md::looks_like_gfm("```\nx ~~y~~\n| a | b |\n|---|---|\n```\n")
                && md::looks_like_gfm("text\n\n| a | b |\n|---|---|\n| 1 | 2 |\n");
            if (!detect_ok) {
                std::fprintf(stderr, "selftest: GFM detection mismatch\n");
                app.shutdown();
                return 1;
            }
            std::printf("selftest markdown io ok: round trips stable, structure matches, dialect detection ok\n");

            // Save to a known path (the dialog itself is not automated).
            app.file_path = std::string(PROJECT_SOURCE_DIR) + "/build/saved_check.md";
            app.doc = std::move(basic);
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.mark_edited();
            app.scroll_y = 0;
            const std::string import_path = dot == std::string::npos
                ? screenshot + "_import"
                : screenshot.substr(0, dot) + "_import" + screenshot.substr(dot);
            if (!save_frame(import_path)) {
                app.shutdown();
                return 1;
            }
            if (!app.modified || !app.save() || app.modified) {
                std::fprintf(stderr, "selftest: save flow failed\n");
                app.shutdown();
                return 1;
            }
            std::string saved;
            md::read_text_file(app.file_path, saved);
            if (saved != md::export_markdown(app.doc, app.gfm())) {
                std::fprintf(stderr, "selftest: saved file differs from export\n");
                app.shutdown();
                return 1;
            }
            std::printf("selftest save ok: %s\n", app.document_name().c_str());
        }

        // Formula editor: insert a fraction structure, type into its slots,
        // apply, and check that a rendered math block appears with LaTeX
        // kept only in the file model.
        {
            app.modified = false;
            app.file_path.clear();
            app.doc = md::Document{};
            app.doc.blocks.push_back(md::Block{});
            app.doc.blocks[0].inlines.push_back(md::Inline{});
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.doc_dirty = true;
            app.frame();

            app.formula_open_new();
            app.frame(); // the click targets come from the previous frame
            app.formula_insert_node("frac"); // empty fraction at caret
            app.frame();
            // numerator "a", denominator "b"
            app.formula_insert_symbol("a");
            app.frame();
            app.formula_move_slot(1);
            app.formula_insert_symbol("b");
            app.frame();
            app.formula_apply();

            const bool has_math = app.doc.blocks.size() >= 2
                && app.doc.blocks[1].type == md::BlockType::CodeBlock
                && app.doc.blocks[1].lang == "math"
                && !app.doc.blocks[1].inlines.empty()
                && app.doc.blocks[1].inlines[0].text.find("\\frac") != std::string::npos;
            if (!has_math) {
                const md::Block& bad = app.doc.blocks.size() >= 2 ? app.doc.blocks[1] : app.doc.blocks[0];
                std::fprintf(stderr, "selftest: formula apply failed (block lang='%s' text='%s')\n",
                             bad.lang.c_str(),
                             bad.inlines.empty() ? "" : bad.inlines[0].text.c_str());
                app.shutdown();
                return 1;
            }
            app.doc_dirty = true;
            for (int i = 0; i < 3; ++i) { pump(); app.frame(); }
            const auto fdot = screenshot.find_last_of('.');
            save_frame(fdot == std::string::npos
                ? screenshot + "_formula"
                : screenshot.substr(0, fdot) + "_formula" + screenshot.substr(fdot));

            // Reopen the block in the editor: the structure must survive the
            // LaTeX round trip and the numerator slot must be selectable.
            const int math_index = 1;
            app.formula_open_block(math_index);
            app.frame(); // renders the canvas and collects clickable slots
            const bool reopened = app.formula_editor_open
                && app.formula_edit.type == md::MathNode::Seq
                && !app.formula_slots.empty();
            if (!reopened) {
                std::fprintf(stderr, "selftest: formula reopen failed\n");
                app.shutdown();
                return 1;
            }
            save_frame(fdot == std::string::npos
                ? screenshot + "_editor"
                : screenshot.substr(0, fdot) + "_editor" + screenshot.substr(fdot));
            app.formula_editor_open = false;

            // Delete: build a fraction + a trailing symbol, then delete the
            // element under the caret and confirm it disappears.
            {
                app.formula_open_new();
                app.frame();
                app.formula_insert_node("frac");
                app.frame();
                app.formula_insert_symbol("a");
                app.frame();
                // move out of the fraction to root, then type a stray symbol
                app.formula_path = {};
                app.formula_insert_symbol("z");
                app.frame();
                const size_t before = app.formula_edit.slots.size();
                // caret is on the stray "z" (top-level element) -> delete it
                app.formula_delete_element();
                app.frame();
                const bool removed = app.formula_edit.slots.size() == before - 1;
                app.formula_editor_open = false;
                if (!removed) {
                    std::fprintf(stderr, "selftest: formula delete element failed\n");
                    app.shutdown();
                    return 1;
                }
            }

            // Real click on the math block must OPEN the editor and keep it
            // open (the opening click is swallowed, not fed to a panel button).
            {
                app.doc_dirty = true;
                for (int i = 0; i < 3; ++i) { pump(); app.frame(); }
                // Find the math block's on-screen centre from the layout.
                const int mi = 1;
                const md::BlockBox& mbox = app.layout.boxes[mi];
                const float bx = app.doc_viewport.x + mbox.x + mbox.w / 2.0f;
                const float by = app.doc_viewport.y + mbox.y - app.scroll_y + mbox.h / 2.0f;
                app.formula_editor_open = false;
                push_mouse_event(SDL_EVENT_MOUSE_MOTION, bx, by);
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, bx, by);
                pump();
                app.frame();
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, bx, by);
                pump();
                app.frame();
                if (!app.formula_editor_open) {
                    std::fprintf(stderr, "selftest: clicking math block did not keep editor open\n");
                    app.shutdown();
                    return 1;
                }
                app.formula_editor_open = false;
            }

            std::printf("selftest formula ok: block inserted, rendered, reopened, click-open, delete\n");
        }

        // Diagram editor: a template block renders as a picture (never code),
        // opens on click, edits elements, and keeps mermaid only in the model
        // that goes to the file.
        {
            app.modified = false;
            app.file_path.clear();
            app.doc = md::Document{};
            app.doc.blocks.push_back(md::Block{});
            app.doc.blocks[0].inlines.push_back(md::Inline{});
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.scroll_y = 0;
            app.doc_dirty = true;
            app.frame();

            // Insert the "Ветвление" template the way the Insert tab does.
            md::Block b;
            b.type = md::BlockType::CodeBlock;
            b.lang = kDiagramTemplates[1].lang;
            b.inlines.push_back(md::Inline{std::string(kDiagramTemplates[1].text_ru), 0, ""});
            app.doc.blocks.push_back(std::move(b));
            app.editor.set_caret(md::Pos{1, 0, 0, -1});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.doc_dirty = true;
            app.frame();

            auto diag_model_of = [&](int idx, bool* ok_out) {
                std::string code;
                for (const md::Inline& inl : app.doc.blocks[idx].inlines) code += inl.text;
                return md::diag_from_text(code, ok_out);
            };

            // The code parses into a visual model with elements.
            {
                bool ok = true;
                const md::DiagModel m = diag_model_of(1, &ok);
                if (!ok || m.nodes.size() < 4 || m.edges.size() < 3) {
                    std::fprintf(stderr,
                                 "selftest: diagram template did not parse "
                                 "(ok=%d nodes=%zu edges=%zu)\n",
                                 ok ? 1 : 0, m.nodes.size(), m.edges.size());
                    app.shutdown();
                    return 1;
                }
            }
            // The layout reserves the height of the picture, not code lines.
            if (app.layout.boxes.size() < 2 || app.layout.boxes[1].h < 60.0f) {
                std::fprintf(stderr, "selftest: diagram block has no picture height (%.1f)\n",
                             app.layout.boxes.size() >= 2 ? app.layout.boxes[1].h : -1.0f);
                app.shutdown();
                return 1;
            }
            const auto ddot = screenshot.find_last_of('.');
            save_frame(ddot == std::string::npos
                ? screenshot + "_diagram"
                : screenshot.substr(0, ddot) + "_diagram" + screenshot.substr(ddot));

            // Opening the block shows the editor with elements and click slots.
            if (!app.diag_open_block(1)) {
                std::fprintf(stderr, "selftest: diagram block did not open in the editor\n");
                app.shutdown();
                return 1;
            }
            app.frame(); // renders the canvas and collects clickable slots
            const bool reopened = app.diag_editor_open
                && !app.diag_edit.nodes.empty() && !app.diag_slots.empty();
            if (!reopened) {
                std::fprintf(stderr,
                             "selftest: diagram reopen failed (nodes=%zu slots=%zu)\n",
                             app.diag_edit.nodes.size(), app.diag_slots.size());
                app.shutdown();
                return 1;
            }
            save_frame(ddot == std::string::npos
                ? screenshot + "_diageditor"
                : screenshot.substr(0, ddot) + "_diageditor" + screenshot.substr(ddot));

            // Add an element, type into it and apply: the block text (the only
            // place code lives) grows a matching node.
            {
                const size_t nodes_before = app.diag_edit.nodes.size();
                app.diag_add_node();
                app.frame();
                app.diag_insert_text("Тест");
                app.frame();
                if (app.diag_edit.nodes.size() != nodes_before + 1) {
                    std::fprintf(stderr, "selftest: diagram add node failed\n");
                    app.shutdown();
                    return 1;
                }
                app.diag_apply();
                bool ok = true;
                const md::DiagModel m = diag_model_of(1, &ok);
                bool has_new = false;
                for (const md::DiagNode& n : m.nodes) {
                    if (n.label == "Тест") has_new = true;
                }
                if (!ok || !has_new) {
                    std::fprintf(stderr, "selftest: diagram apply failed (ok=%d)\n", ok ? 1 : 0);
                    app.shutdown();
                    return 1;
                }
            }

            // Delete: select the first edge and remove it.
            {
                if (!app.diag_open_block(1)) {
                    std::fprintf(stderr, "selftest: diagram reopen before delete failed\n");
                    app.shutdown();
                    return 1;
                }
                app.frame();
                const size_t edges_before = app.diag_edit.edges.size();
                app.diag_select(0, true);
                app.diag_delete_element();
                app.frame();
                if (edges_before == 0 || app.diag_edit.edges.size() != edges_before - 1) {
                    std::fprintf(stderr, "selftest: diagram delete element failed (%zu -> %zu)\n",
                                 edges_before, app.diag_edit.edges.size());
                    app.shutdown();
                    return 1;
                }
                app.diag_editor_open = false;
            }

            // Real click on the diagram block must OPEN the editor and keep
            // it open (the opening click is swallowed, like the formula one).
            {
                app.doc_dirty = true;
                for (int i = 0; i < 3; ++i) {
                    pump();
                    app.frame();
                }
                const md::BlockBox& dbox = app.layout.boxes[1];
                const float bx = app.doc_viewport.x + dbox.x + dbox.w / 2.0f;
                const float by = app.doc_viewport.y + dbox.y - app.scroll_y + dbox.h / 2.0f;
                app.diag_editor_open = false;
                push_mouse_event(SDL_EVENT_MOUSE_MOTION, bx, by);
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, bx, by);
                pump();
                app.frame();
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, bx, by);
                pump();
                app.frame();
                if (!app.diag_editor_open) {
                    std::fprintf(stderr,
                                 "selftest: clicking the diagram block did not keep the editor open\n");
                    app.shutdown();
                    return 1;
                }
                app.diag_editor_open = false;
            }

            // Sequence diagram: a different template still parses, reserves
            // picture height and opens in the editor.
            {
                md::Block sq;
                sq.type = md::BlockType::CodeBlock;
                sq.lang = kDiagramTemplates[5].lang;
                sq.inlines.push_back(
                    md::Inline{std::string(kDiagramTemplates[5].text_ru), 0, ""});
                app.doc.blocks.push_back(std::move(sq));
                app.doc_dirty = true;
                app.frame();
                const int sq_idx = static_cast<int>(app.doc.blocks.size()) - 1;
                bool ok = true;
                const md::DiagModel m = diag_model_of(sq_idx, &ok);
                if (!ok || m.kind != md::DiagKind::Sequence || m.nodes.size() < 2
                    || m.edges.size() < 3) {
                    std::fprintf(stderr,
                                 "selftest: sequence template did not parse "
                                 "(ok=%d kind=%d nodes=%zu edges=%zu)\n",
                                 ok ? 1 : 0, static_cast<int>(m.kind),
                                 m.nodes.size(), m.edges.size());
                    app.shutdown();
                    return 1;
                }
                if (app.layout.boxes.size() < 3 || app.layout.boxes[2].h < 80.0f) {
                    std::fprintf(stderr, "selftest: sequence picture has no height\n");
                    app.shutdown();
                    return 1;
                }
                save_frame(ddot == std::string::npos
                    ? screenshot + "_diagseq"
                    : screenshot.substr(0, ddot) + "_diagseq" + screenshot.substr(ddot));
                if (!app.diag_open_block(sq_idx) || !app.diag_editor_open) {
                    std::fprintf(stderr, "selftest: sequence block did not open\n");
                    app.shutdown();
                    return 1;
                }
                app.diag_editor_open = false;
            }

            // Unsupported code (class diagram): shown as a neutral placeholder
            // and refuses to open, keeping its text intact.
            {
                md::Block cls;
                cls.type = md::BlockType::CodeBlock;
                cls.lang = "mermaid";
                cls.inlines.push_back(md::Inline{
                    std::string("classDiagram\n  Animal <|-- Duck\n  Animal : +int age\n"), 0, ""});
                app.doc.blocks.push_back(std::move(cls));
                app.doc_dirty = true;
                app.frame();
                const int cls_idx = static_cast<int>(app.doc.blocks.size()) - 1;
                if (app.diag_open_block(cls_idx)) {
                    std::fprintf(stderr, "selftest: unsupported diagram opened anyway\n");
                    app.shutdown();
                    return 1;
                }
                const std::string before = app.doc.blocks[cls_idx].inlines[0].text;
                if (before.find("classDiagram") == std::string::npos) {
                    std::fprintf(stderr, "selftest: unsupported diagram text was rewritten\n");
                    app.shutdown();
                    return 1;
                }
                // Scroll the placeholder into view for the screenshot.
                if (cls_idx < static_cast<int>(app.layout.boxes.size())) {
                    app.scroll_y = std::max(
                        0.0f, app.layout.boxes[cls_idx].y
                                  + app.layout.boxes[cls_idx].h
                                        - static_cast<float>(app.height) + 160.0f);
                }
                app.frame();
                // Its placeholder box must reserve space instead of code lines.
                if (cls_idx >= static_cast<int>(app.layout.boxes.size())
                    || app.layout.boxes[cls_idx].h < 30.0f) {
                    std::fprintf(stderr, "selftest: unsupported diagram has no placeholder box\n");
                    app.shutdown();
                    return 1;
                }
                save_frame(ddot == std::string::npos
                    ? screenshot + "_diagunsupported"
                    : screenshot.substr(0, ddot) + "_diagunsupported" + screenshot.substr(ddot));
            }

            std::printf(
                "selftest diagram ok: picture rendered, editor opened, click-open, add/delete, "
                "sequence, unsupported placeholder\n");
        }

        // File menu, clicked for real: "Новый" empties the document. The modified
        // flag is cleared first so no save prompt is shown.
        {
            app.modified = false;
            app.file_path = "/tmp/opencode/not-used.md";
            app.doc = md::make_demo_document();
            app.doc_dirty = true;
            // The demo document replaced the saved one behind history's back;
            // reset it so the first click is not seen as an edit.
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.frame();

            const float fx = app.file_rect.x + app.file_rect.w / 2.0f;
            const float fy = app.file_rect.y + app.file_rect.h / 2.0f;
            push_mouse_event(SDL_EVENT_MOUSE_MOTION, fx, fy);
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, fx, fy);
            pump();
            app.frame();
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, fx, fy);
            pump();
            app.frame();
            if (app.ui.open_popup != "file") {
                std::fprintf(stderr, "selftest: file menu did not open\n");
                app.shutdown();
                return 1;
            }
            const auto file_dot = screenshot.find_last_of('.');
            save_frame(file_dot == std::string::npos
                ? screenshot + "_filemenu"
                : screenshot.substr(0, file_dot) + "_filemenu" + screenshot.substr(file_dot));

            const float ix = app.file_rect.x + 20.0f;
            const float iy = app.file_rect.y + app.file_rect.h + 4.0f + app.file_rect.h / 2.0f;
            push_mouse_event(SDL_EVENT_MOUSE_MOTION, ix, iy);
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, ix, iy);
            pump();
            app.frame();
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, ix, iy);
            pump();
            app.frame();

            const bool empty = app.doc.blocks.size() == 1 && app.doc.blocks[0].inlines.size() == 1
                && app.doc.blocks[0].inlines[0].text.empty();
            if (!empty || !app.file_path.empty() || !app.ui.open_popup.empty()) {
                std::fprintf(stderr, "selftest: file menu \"Новый\" failed\n"
                             "  blocks=%zu fpath='%s' popup='%s' modified=%d\n",
                             app.doc.blocks.size(),
                             app.file_path.c_str(), app.ui.open_popup.c_str(),
                             app.modified ? 1 : 0);
                app.shutdown();
                return 1;
            }
    app.ui.open_popup.clear();
            std::printf("selftest file menu ok: new document from menu\n");
        }

        // Narrow window: the toolbar wraps onto more rows and grows taller.
        {
            const float wide_h = app.toolbar_h;
            SDL_SetWindowSize(app.window, 560, 720);
            for (int i = 0; i < 3; ++i) {
                pump();
                app.frame();
            }
            const float narrow_h = app.toolbar_h;
            const auto dot_pos = screenshot.find_last_of('.');
            const std::string narrow_path = dot_pos == std::string::npos
                ? screenshot + "_narrow"
                : screenshot.substr(0, dot_pos) + "_narrow" + screenshot.substr(dot_pos);
            if (!save_frame(narrow_path)) {
                app.shutdown();
                return 1;
            }
            SDL_SetWindowSize(app.window, 1100, 720);
            for (int i = 0; i < 3; ++i) {
                pump();
                app.frame();
            }
            if (narrow_h <= wide_h || app.toolbar_h != wide_h) {
                std::fprintf(stderr, "selftest toolbar: wrap failed (wide %.0f, narrow %.0f, back %.0f)\n",
                             wide_h, narrow_h, app.toolbar_h);
                app.shutdown();
                return 1;
            }
            std::printf("selftest toolbar ok: wide %.0fpx, narrow %.0fpx, screenshot %s\n",
                        wide_h, narrow_h, narrow_path.c_str());
        }

        // Undo/redo. Typing in a row is one step, a delete is its own step, a new
        // edit after undo drops the redo branch.
        {
            md::Document base;
            base.blocks.push_back(md::Block{});
            base.blocks[0].inlines.push_back(md::Inline{"abc"});
            app.doc = base;
            app.editor.set_caret(md::Pos{0, 0, 3});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());

            auto text_of = [&]() {
                std::string s;
                for (const md::Inline& inl : app.doc.blocks[0].inlines) s += inl.text;
                return s;
            };
            auto text_event = [](const char* t) {
                SDL_Event e{};
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text = t;
                return e;
            };
            auto key_event = [](SDL_Keycode key, SDL_Keymod mod) {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = key;
                e.key.mod = mod;
                return e;
            };
            const SDL_Keymod none = SDL_KMOD_NONE;
            const SDL_Keymod ctrl = SDL_KMOD_CTRL;

            bool ok = true;
            auto expect = [&](const char* what, const std::string& want) {
                if (text_of() != want) {
                    std::fprintf(stderr, "selftest undo: %s: got \"%s\", want \"%s\"\n",
                                 what, text_of().c_str(), want.c_str());
                    ok = false;
                }
            };

            app.dispatch(text_event("x"));
            app.dispatch(text_event("y"));
            expect("typing", "abcxy");
            app.dispatch(key_event(SDLK_Z, ctrl));
            expect("undo typing run", "abc");
            app.dispatch(key_event(SDLK_Y, ctrl));
            expect("redo", "abcxy");
            app.dispatch(key_event(SDLK_Z, ctrl));
            app.dispatch(key_event(SDLK_Z, ctrl));
            expect("undo to start", "abc");
            app.dispatch(text_event("z"));
            app.dispatch(key_event(SDLK_Y, ctrl));
            expect("new edit drops redo", "abcz");
            app.dispatch(key_event(SDLK_BACKSPACE, none));
            expect("backspace", "abc");
            app.dispatch(key_event(SDLK_Z, ctrl));
            expect("undo backspace", "abcz");
            if (app.editor.caret().offset != 4) {
                std::fprintf(stderr, "selftest undo: caret not restored (%d)\n",
                             app.editor.caret().offset);
                ok = false;
            }
            app.dispatch(key_event(SDLK_Z, ctrl | SDL_KMOD_SHIFT));
            expect("shift+ctrl+z redo", "abc");

            if (!ok) {
                app.shutdown();
                return 1;
            }
            std::printf("selftest undo ok: typing runs, deletes, redo, caret\n");
        }

        // Tables: cells take the caret and typing, Tab and Enter move between them,
        // Tab past the last cell adds a row, row and column commands keep the grid
        // rectangular, pipes are escaped, clicks land in the right cell, and the
        // table tab is locked in CommonMark.
        {
            md::Document base;
            base.blocks.push_back(md::Block{});
            base.blocks[0].inlines.push_back(md::Inline{"x"});
            app.doc = base;
            app.doc_dirty = true;
            app.dialect = 1;
            app.editor.set_caret(md::Pos{0, 0, 1});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.frame();

            auto key_event = [](SDL_Keycode k) {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = k;
                e.key.mod = SDL_KMOD_NONE;
                return e;
            };
            auto text_event = [](const char* t) {
                SDL_Event e{};
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text = t;
                return e;
            };
            auto table_index = [&]() {
                for (size_t i = 0; i < app.doc.blocks.size(); ++i) {
                    if (app.doc.blocks[i].type == md::BlockType::Table) return static_cast<int>(i);
                }
                return -1;
            };
            auto table_rows = [&]() { return md::table_rows(app.doc.blocks[table_index()]); };
            auto table_cols = [&]() { return md::table_cols(app.doc.blocks[table_index()]); };
            auto cell_text = [&](int r, int c) {
                const md::Block& t = app.doc.blocks[table_index()];
                std::string s;
                for (const md::Inline& inl : t.cells[r * md::table_cols(t) + c]) s += inl.text;
                return s;
            };
            auto click = [&](float x, float y) {
                push_mouse_event(SDL_EVENT_MOUSE_MOTION, x, y);
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y);
                pump();
                app.frame();
                push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, x, y);
                pump();
                app.frame();
            };

            bool ok = true;
            auto expect = [&](const char* what, bool cond) {
                if (!cond) {
                    std::fprintf(stderr, "selftest table: %s failed\n", what);
                    ok = false;
                }
            };

            app.editor.insert_table(2, 2);
            app.dispatch(text_event("a"));
            app.dispatch(key_event(SDLK_TAB));
            app.dispatch(text_event("b"));
            app.dispatch(key_event(SDLK_TAB));
            app.dispatch(text_event("c"));
            app.dispatch(key_event(SDLK_RETURN));
            app.dispatch(text_event("d"));
            expect("typing and Enter", cell_text(0, 0) == "a" && cell_text(0, 1) == "b"
                && cell_text(1, 0) == "c" && cell_text(2, 0) == "d" && table_rows() == 3);

            app.dispatch(key_event(SDLK_TAB));
            app.dispatch(key_event(SDLK_TAB));
            expect("Tab past the last cell adds a row",
                   table_rows() == 4 && app.editor.caret().cell == 6);

            app.dispatch(text_event("e"));
            app.dispatch(key_event(SDLK_BACKSPACE));
            const size_t blocks = app.doc.blocks.size();
            app.dispatch(key_event(SDLK_BACKSPACE));
            expect("backspace at cell start keeps the table",
                   app.doc.blocks.size() == blocks && table_index() >= 0 && cell_text(3, 0).empty());

            app.editor.table_add_column();
            expect("add column", table_cols() == 3 && cell_text(0, 2) == "b");
            app.editor.table_remove_column();
            expect("remove column", table_cols() == 2 && cell_text(0, 1) == "b");
            app.editor.table_remove_row();
            expect("remove row", table_rows() == 3);

            app.editor.set_caret(md::Pos{table_index(), 0, 1, 0});
            app.dispatch(text_event("|"));
            expect("pipe is escaped on export",
                   md::export_markdown(app.doc, true).find("a\\|") != std::string::npos);

            app.scroll_y = 0.0f;
            app.frame();
            const md::TableGrid& grid = app.layout.tables[0];
            click(app.doc_viewport.x + (grid.xs[1] + grid.xs[2]) / 2.0f,
                  app.doc_viewport.y - app.scroll_y + (grid.ys[1] + grid.ys[2]) / 2.0f);
            expect("click lands in cell (1,1)",
                   app.editor.caret().cell == 3 && app.editor.caret().block == table_index());

            const ui::Rect theme_btn = app.theme_rect;
            click(theme_btn.x + theme_btn.w / 2.0f, theme_btn.y + theme_btn.h / 2.0f);
            expect("theme toggles to light", app.light_theme && ui::is_light_palette(app.ui.palette));
            click(theme_btn.x + theme_btn.w / 2.0f, theme_btn.y + theme_btn.h / 2.0f);
            expect("theme toggles back to dark", !app.light_theme && !ui::is_light_palette(app.ui.palette));

            const ui::Rect lang_btn = app.lang_rect;
            click(lang_btn.x + lang_btn.w / 2.0f, lang_btn.y + lang_btn.h / 2.0f);
            expect("lang toggles to EN", app.ui_lang == 1 && app.en());
            click(lang_btn.x + lang_btn.w / 2.0f, lang_btn.y + lang_btn.h / 2.0f);
            expect("lang toggles back to RU", app.ui_lang == 0 && !app.en());

            const ui::Rect help_btn = app.help_rect;
            click(help_btn.x + help_btn.w / 2.0f, help_btn.y + help_btn.h / 2.0f);
            expect("welcome modal opens via ? button", app.welcome_open);
            push_key_event(SDLK_ESCAPE);
            pump();
            app.frame();
            expect("welcome modal closes via Escape", !app.welcome_open);

            app.dialect = 1;
            app.editor.set_caret(md::Pos{table_index(), 0, 0, 1});
            app.frame();
            expect("table tools show in a table", app.editor.in_table());
            const auto table_dot = screenshot.find_last_of('.');
            save_frame(table_dot == std::string::npos
                ? screenshot + "_table"
                : screenshot.substr(0, table_dot) + "_table" + screenshot.substr(table_dot));

            app.editor.table_remove();
            app.mark_edited();
            app.frame();
            expect("table remove keeps document valid", table_index() == -1 && !app.doc.blocks.empty());

            if (!ok) {
                app.shutdown();
                return 1;
            }
            std::printf("selftest table ok: typing, Tab/Enter, rows, columns, pipes, click, lock\n");
        }

        // Open-from-path, Drag & Drop, Chart editor slots/limits, Formula nested delete,
        // Scrollbar drag, Double-click at document end, and Pandoc PDF/EPUB export.
        {
            const std::string fixtures_dir = std::string(PROJECT_SOURCE_DIR) + "/tests/fixtures/";
            app.modified = false;

            // 1. open_from_path: valid file, missing file, empty file
            if (app.open_from_path(fixtures_dir + "gfm.md", true, false) != App::OpenResult::Opened
                || app.dialect != 1 || app.welcome_open) {
                std::fprintf(stderr, "selftest: open_from_path gfm.md failed\n");
                app.shutdown();
                return 1;
            }
            const std::string prev_path = app.file_path;
            if (app.open_from_path(fixtures_dir + "does_not_exist.md", true, false) != App::OpenResult::Error
                || app.file_path != prev_path || app.dialect != 1) {
                std::fprintf(stderr, "selftest: open_from_path missing file corrupted state\n");
                app.shutdown();
                return 1;
            }
            const std::string empty_md = std::string(PROJECT_SOURCE_DIR) + "/build/empty_check.md";
            md::write_text_file(empty_md, "");
            if (app.open_from_path(empty_md, true, false) != App::OpenResult::Opened
                || app.doc.blocks.empty()) {
                std::fprintf(stderr, "selftest: open_from_path empty file failed\n");
                app.shutdown();
                return 1;
            }

            // 2. Drag & Drop: single valid file opens; multiple files and unsupported
            // types are rejected without replacing the document.
            {
                const std::string basic_md = fixtures_dir + "basic.md";
                SDL_Event eb{}; eb.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb);
                SDL_Event ef{}; ef.type = SDL_EVENT_DROP_FILE; ef.drop.data = basic_md.c_str();
                app.dispatch(ef);
                SDL_Event ec{}; ec.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec);
                if (app.file_path != basic_md || app.welcome_open) {
                    std::fprintf(stderr, "selftest: drag & drop single file failed\n");
                    app.shutdown();
                    return 1;
                }
                // Two files in one gesture: rejected as a whole, document kept.
                const std::string prev_path2 = app.file_path;
                SDL_Event eb2{}; eb2.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb2);
                SDL_Event e1{}; e1.type = SDL_EVENT_DROP_FILE; e1.drop.data = basic_md.c_str();
                app.dispatch(e1);
                const std::string other_md = fixtures_dir + "gfm.md";
                SDL_Event e2{}; e2.type = SDL_EVENT_DROP_FILE; e2.drop.data = other_md.c_str();
                app.dispatch(e2);
                SDL_Event ec2{}; ec2.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec2);
                if (app.file_path != prev_path2) {
                    std::fprintf(stderr, "selftest: multi-file drop replaced document\n");
                    app.shutdown();
                    return 1;
                }
                // Unsupported extension: rejected, document kept.
                SDL_Event eb3{}; eb3.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb3);
                const std::string png = fixtures_dir + "not_a_markdown.png";
                SDL_Event e3{}; e3.type = SDL_EVENT_DROP_FILE; e3.drop.data = png.c_str();
                app.dispatch(e3);
                SDL_Event ec3{}; ec3.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec3);
                if (app.file_path != prev_path2) {
                    std::fprintf(stderr, "selftest: unsupported drop replaced document\n");
                    app.shutdown();
                    return 1;
                }
                // A new gesture after rejections must still open files.
                SDL_Event eb4{}; eb4.type = SDL_EVENT_DROP_BEGIN;
                app.dispatch(eb4);
                SDL_Event e4{}; e4.type = SDL_EVENT_DROP_FILE; e4.drop.data = other_md.c_str();
                app.dispatch(e4);
                SDL_Event ec4{}; ec4.type = SDL_EVENT_DROP_COMPLETE;
                app.dispatch(ec4);
                if (app.file_path != other_md) {
                    std::fprintf(stderr, "selftest: drop after rejections failed\n");
                    app.shutdown();
                    return 1;
                }
            }

            // 3. Chart editor: slots selectable when 0/empty, row cap at 12
            {
                app.chart_open_new();
                app.frame();
                for (int i = 0; i < 4; ++i) app.chart_add_row();
                app.chart_select(1, true); // value cell of an empty row
                app.frame();
                if (app.chart_slots.size() < 10 || app.chart_sel < 0 || !app.chart_sel_value) {
                    std::fprintf(stderr, "selftest: chart slots missing (%zu, sel=%d val=%d)\n",
                                 app.chart_slots.size(), app.chart_sel,
                                 app.chart_sel_value ? 1 : 0);
                    app.shutdown();
                    return 1;
                }
                for (int i = 0; i < 20; ++i) app.chart_add_row();
                if (app.chart_edit.labels.size() > App::kMaxChartRows) {
                    std::fprintf(stderr, "selftest: chart row limit not enforced (%zu)\n",
                                 app.chart_edit.labels.size());
                    app.shutdown();
                    return 1;
                }
                app.chart_apply();
                app.frame();
            }

            // 4. Formula editor: operator split & nested delete keeps outer formula intact
            {
                app.formula_open_new();
                app.frame();
                app.formula_insert_symbol("a");
                app.formula_insert_operator("+");
                app.formula_insert_node("frac");
                app.frame();
                app.formula_insert_symbol("x");
                app.frame();
                // First delete clears "x" inside numerator without deleting "a +" or the fraction
                app.formula_delete_element();
                app.frame();
                if (app.formula_edit.slots.size() < 3) {
                    std::fprintf(stderr, "selftest: formula delete wiped whole formula (%zu)\n",
                                 app.formula_edit.slots.size());
                    app.shutdown();
                    return 1;
                }
                // Second delete on empty numerator removes the fraction, keeping "a +"
                app.formula_delete_element();
                app.frame();
                if (app.formula_edit.slots.size() < 2) {
                    std::fprintf(stderr, "selftest: formula structure delete wiped siblings (%zu)\n",
                                 app.formula_edit.slots.size());
                    app.shutdown();
                    return 1;
                }
                app.formula_editor_open = false;
            }

            // 5. Double-click at the bottom when last block is a chart/code block creates a new paragraph
            {
                const size_t before_blocks = app.doc.blocks.size();
                app.scroll_y = app.max_scroll;
                app.frame();
                const float click_x = app.doc_viewport.x + 40.0f;
                const float click_y = app.doc_viewport.y + app.doc_viewport.h - 4.0f;
                SDL_Event md{};
                md.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                md.button.button = SDL_BUTTON_LEFT;
                md.button.x = click_x;
                md.button.y = click_y;
                md.button.clicks = 2;
                app.dispatch(md);
                SDL_Event mu{};
                mu.type = SDL_EVENT_MOUSE_BUTTON_UP;
                mu.button.button = SDL_BUTTON_LEFT;
                mu.button.x = click_x;
                mu.button.y = click_y;
                app.dispatch(mu);
                app.frame();
                if (app.doc.blocks.size() != before_blocks + 1
                    || app.doc.blocks.back().type != md::BlockType::Paragraph
                    || app.editor.caret().block != static_cast<int>(app.doc.blocks.size()) - 1) {
                    std::fprintf(stderr, "selftest: double-click at document end did not create paragraph\n");
                    app.shutdown();
                    return 1;
                }
            }

            // 6. Export EPUB, DOCX and PDF via Pandoc (+ Typst) if available
            {
                app.doc = md::make_demo_document();
                app.doc_dirty = true;
                app.frame();
                if (!app.typst_path.empty()) {
                    const std::string pdf_out = std::string(PROJECT_SOURCE_DIR) + "/build/selftest_out.pdf";
                    if (!app.export_gost_pdf(pdf_out)) {
                        std::fprintf(stderr, "selftest: GOST PDF export on demo doc failed\n");
                        app.shutdown();
                        return 1;
                    }
                }
                if (app.has_pandoc) {
                    const std::string epub_out = std::string(PROJECT_SOURCE_DIR) + "/build/selftest_out.epub";
                    if (!app.export_with_pandoc("epub", epub_out)) {
                        std::fprintf(stderr, "selftest: EPUB export failed\n");
                        app.shutdown();
                        return 1;
                    }
                    const std::string docx_out = std::string(PROJECT_SOURCE_DIR) + "/build/selftest_out.docx";
                    if (!app.export_with_pandoc("docx", docx_out)) {
                        std::fprintf(stderr, "selftest: DOCX export failed\n");
                        app.shutdown();
                        return 1;
                    }
                    std::printf("selftest pandoc export ok: EPUB + DOCX%s\n",
                                app.typst_path.empty() ? "" : " + PDF (Typst)");
                }
            }

            std::printf("selftest v1.0.1 fixes ok: open_from_path, drop, chart, formula, dblclick\n");
        }

        {
            md::import_markdown("# Заголовок\n\nПривет мир, привет **всем**\n\n## Ещё\n\nтекст\n",
                                false, app.doc);
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.history.reset(app.doc, app.editor.caret(), app.editor.anchor());
            app.doc_dirty = true;
            app.frame();

            auto key_mod = [](SDL_Keycode k, SDL_Keymod m) {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.key = k;
                e.key.mod = m;
                return e;
            };
            auto text_in = [](const char* t) {
                SDL_Event e{};
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text = t;
                return e;
            };

            bool ok = true;
            auto expect = [&](const char* what, bool cond) {
                if (!cond) {
                    std::fprintf(stderr, "selftest find: %s failed\n", what);
                    ok = false;
                }
            };

            expect("case-insensitive Cyrillic search", md::find_all(app.doc, "ПРИВЕТ").size() == 2);
            expect("word count", md::count_words(app.doc) == 7);

            app.dispatch(key_mod(SDLK_F, SDL_KMOD_CTRL));
            expect("ctrl+F opens find", app.find_open && app.find_focus && !app.find_replace_mode);
            app.dispatch(text_in("привет"));
            expect("typing selects first match", app.find_matches.size() == 2
                && app.find_current == 0 && app.editor.has_selection());
            app.dispatch(key_mod(SDLK_RETURN, SDL_KMOD_NONE));
            expect("Enter selects next match", app.find_current == 1);

            app.dispatch(key_mod(SDLK_H, SDL_KMOD_CTRL));
            expect("ctrl+H opens replace", app.find_open && app.find_replace_mode);
            app.dispatch(key_mod(SDLK_TAB, SDL_KMOD_NONE));
            app.dispatch(text_in("Hi"));
            app.dispatch(key_mod(SDLK_RETURN, SDL_KMOD_CTRL));
            expect("replace all", md::find_all(app.doc, "привет").empty()
                && md::find_all(app.doc, "hi").size() == 2);

            app.frame();
            expect("word count unchanged by replace", app.word_count == 7);
            const std::string find_shot = screenshot.substr(0, screenshot.find_last_of('.'))
                + "_find.bmp";
            if (!save_frame(find_shot)) {
                app.shutdown();
                return 1;
            }

            app.dispatch(key_mod(SDLK_ESCAPE, SDL_KMOD_NONE));
            expect("Escape closes find", !app.find_open);

            app.dispatch(key_mod(SDLK_T, static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT)));
            app.frame();
            expect("outline lists headings", app.outline_open && app.outline_rows.size() == 2);
            const ui::Rect row = app.outline_rows[1].first;
            expect("outline click moves caret", app.outline_click(row.x + 5.0f, row.y + 5.0f)
                && app.editor.caret().block == 2);
            app.frame();
            if (!save_frame(find_shot.substr(0, find_shot.size() - 4) + "_outline.bmp")) {
                app.shutdown();
                return 1;
            }

            if (!ok) {
                app.shutdown();
                return 1;
            }
        }

        {
            md::Document mdoc;
            md::import_markdown("Цена $5 и $10, формула $x^2$ в строке.", true, mdoc);
            md::Layout lay;
            md::layout_document(mdoc, app.fontbook, 760.0f, lay);
            int math_spans = 0;
            for (const md::Line& line : lay.lines) {
                for (const md::Span& span : line.spans) math_spans += span.math ? 1 : 0;
            }
            const int text_pos = static_cast<int>(mdoc.blocks[0].inlines[0].text.find("$x^2$")) + 2;
            const md::MathReveal inside = md::math_reveal_at(mdoc, md::Pos{0, -1, 0, -1});
            const md::MathReveal in_formula = md::math_reveal_at(mdoc, md::Pos{0, 0, text_pos, -1});
            if (math_spans != 1 || !(inside == md::MathReveal{}) || in_formula.begin < 0) {
                std::fprintf(stderr, "selftest: inline formulas failed (spans=%d)\n", math_spans);
                app.shutdown();
                return 1;
            }
        }
        {
            md::import_markdown("Текст", true, app.doc);
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.editor.set_caret(md::Pos{0, 0, 10});
            app.formula_editor_open = true;
            app.formula_edit = md::math_from_latex("x^2");
            app.formula_insert_inline();
            const std::string& text = app.doc.blocks[0].inlines[0].text;
            if (text.rfind("Текст $", 0) != 0 || text.find('$', 8) == std::string::npos
                || app.formula_editor_open) {
                std::fprintf(stderr, "selftest: inline formula insert failed (%s)\n", text.c_str());
                app.shutdown();
                return 1;
            }
        }
        {
            md::import_markdown("```rust\nfn main() {}\n```\n", true, app.doc);
            app.doc_dirty = true;
            app.scroll_y = 0.0f;
            app.frame();
            const float by = app.doc_viewport.y + app.layout.boxes[0].y;
            const float top = std::max(by + 8.0f, app.doc_viewport.y + 4.0f);
            const float bx = app.doc_viewport.x + 14.0f + 20.0f;
            push_mouse_event(SDL_EVENT_MOUSE_MOTION, bx, top + 12.0f);
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, bx, top + 12.0f);
            pump();
            app.frame();
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, bx, top + 12.0f);
            pump();
            app.frame();
            const bool opened = app.ui.open_popup == "code_lang_0";
            const float row_y = top + 24.0f + 4.0f + 24.0f + 12.0f; // second entry: Python
            push_mouse_event(SDL_EVENT_MOUSE_MOTION, bx, row_y);
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, bx, row_y);
            pump();
            app.frame();
            push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, bx, row_y);
            pump();
            app.frame();
            if (!opened || app.doc.blocks[0].lang != "python") {
                std::fprintf(stderr, "selftest: code language menu failed (open=%d lang=%s)\n",
                             opened ? 1 : 0, app.doc.blocks[0].lang.c_str());
                app.shutdown();
                return 1;
            }
        }
        {
            md::import_markdown("```\nab\ncd\n```\n", true, app.doc);
            app.doc_dirty = true;
            app.frame();
            app.editor.set_caret(md::Pos{0, 0, 1});
            push_key_event(SDLK_TAB);
            pump();
            app.frame();
            const bool single = app.doc.blocks[0].inlines[0].text == "a    b\ncd";
            md::import_markdown("```\nab\ncd\n```\n", true, app.doc);
            app.doc_dirty = true;
            app.frame();
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.editor.set_caret(md::Pos{0, 0, 5}, true);
            push_key_event(SDLK_TAB);
            pump();
            app.frame();
            const bool indented = app.doc.blocks[0].inlines[0].text == "    ab\n    cd";
            push_key_event(SDLK_TAB, SDL_KMOD_LSHIFT);
            pump();
            app.frame();
            const bool outdented = app.doc.blocks[0].inlines[0].text == "ab\ncd";
            if (!single || !indented || !outdented) {
                std::fprintf(stderr, "selftest: code block Tab failed (%d%d%d %s)\n", single ? 1 : 0,
                             indented ? 1 : 0, outdented ? 1 : 0, app.doc.blocks[0].inlines[0].text.c_str());
                app.shutdown();
                return 1;
            }
        }
        {
            md::import_markdown("Текст рядом\n", true, app.doc);
            app.doc_dirty = true;
            app.frame();
            app.editor.set_caret(md::Pos{0, 0, 11});
            app.link_open_dialog();
            app.link_text_value = "сайт";
            app.link_href_value = "https://example.com";
            app.link_apply();
            app.link_text_value = "фото";
            app.link_href_value = "pics/cat.png";
            app.link_apply();
            app.frame();
            int icons = 0;
            for (const md::Line& line : app.layout.lines) {
                for (const md::Span& span : line.spans) icons += span.icon ? 1 : 0;
            }
            const std::string md_text = md::export_markdown(app.doc, true);
            const bool links_ok = md_text.find("[сайт](https://example.com)") != std::string::npos
                && md_text.find("![фото](pics/cat.png)") != std::string::npos;
            if (!links_ok || icons != 1 || app.link_open) {
                std::fprintf(stderr, "selftest: links failed (icons=%d export=%s)\n", icons, md_text.c_str());
                app.shutdown();
                return 1;
            }
        }
        {
            app.light_theme = true;
            app.ui.palette = ui::light_palette();
            md::import_markdown(
                "# Введение\n\n"
                "Настоящий документ выполнен по ГОСТ Р 2.105–2019 и ГОСТ Р 2.106–2019.\n\n"
                "---\n\n"
                "# Технические требования\n\n"
                "## Общие положения\n\n"
                "### Требования к питанию\n\n"
                "- первый элемент перечисления;\n"
                "- второй элемент перечисления.\n\n"
                "> Значения приведены при нормальных климатических условиях.\n\n"
                "$$\n\\rho = \\frac{m}{V}\n$$\n\n"
                "где m — масса образца, кг;\n\n"
                "```mermaid\nflowchart LR\nA[Вход] --> B[Расчёт] --> C[Выход]\n```\n\n"
                "| Наименование показателя | Значение |\n"
                "| --- | --- |\n"
                "| 1 Напряжение питания, В | 24 |\n",
                true, app.doc);
            app.editor.set_caret(md::Pos{0, 0, 0});
            app.doc.meta.dev = "Иванов";
            app.doc.meta.check = "Петров";
            app.doc.meta.ncheck = "Сидоров";
            app.doc.meta.approve = "Смирнов";
            app.doc_dirty = true;
            app.frame();
            save_frame("selftest_gost_body.bmp");
            app.scroll_y = 260.0f;
            app.frame();
            save_frame("selftest_gost_mid.bmp");
            const float page_h = 297.0f * app.layout.mm_px;
            app.scroll_y = std::max(0.0f, page_h - app.doc_viewport.h + 24.0f);
            app.frame();
            for (const md::StampSlot& s : app.stamp_slots) {
                if (s.field == md::StampField::Code) {
                    push_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, s.rect.x + s.rect.w * 0.5f, s.rect.y + s.rect.h * 0.5f);
                    pump();
                    push_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, s.rect.x + s.rect.w * 0.5f, s.rect.y + s.rect.h * 0.5f);
                    pump();
                    break;
                }
            }
            app.frame();
            save_frame("selftest_stamp.bmp");
            const size_t lines_100 = app.layout.lines.size();
            const size_t pages_100 = app.layout.page_tops.size();
            app.zoom_to(0.5f, app.doc_viewport.y + app.doc_viewport.h * 0.5f);
            app.scroll_y = 0.0f;
            app.frame();
            save_frame("selftest_zoom50.bmp");
            if (app.layout.lines.size() != lines_100 || app.layout.page_tops.size() != pages_100) {
                std::fprintf(stderr, "selftest: zoom changed line/page count (%zu->%zu, %zu->%zu)\n",
                             lines_100, app.layout.lines.size(), pages_100, app.layout.page_tops.size());
                app.shutdown();
                return 1;
            }
            app.zoom_to(0.55f, app.doc_viewport.y + app.doc_viewport.h * 0.5f);
            app.zoom_anchor_doc_y = -1.0f;

            // Проверка Титульного листа (раздел 8 ГОСТ Р 2.105) и автоматического «Содержания» (п. 6.2).
            app.doc.meta.has_title_page = true;
            app.doc.meta.has_toc = true;
            app.doc.meta.title_org = "Министерство науки и высшего образования Российской Федерации";
            app.doc.meta.title_product = "РЕГУЛЯТОР ДАВЛЕНИЯ РД-25";
            app.doc.meta.title_doc_kind = "Пояснительная записка";
            app.doc.meta.title_agrees.push_back(md::TitleSign{"Главный метролог", "А.В. Орлов", "02.09.2026"});
            app.doc_dirty = true;
            app.scroll_y = 0.0f;
            app.frame();
            save_frame("selftest_title.bmp");

            app.zoom_to(1.0f, app.doc_viewport.y + app.doc_viewport.h * 0.5f);
            app.zoom_anchor_doc_y = -1.0f;
            app.frame();
            // Переходим ко 2-му листу (1-й текстовый лист с элементом «Содержание» и Формой 2).
            if (app.layout.page_tops.size() >= 2) {
                app.scroll_y = app.layout.page_tops[1];
            }
            app.frame();
            save_frame("selftest_toc.bmp");
            if (app.layout.toc_entries.size() != 3
                || app.layout.toc_entries[0].page_num != 2
                || app.layout.toc_entries[1].page_num != 3) {
                std::fprintf(stderr, "selftest: TOC entries failed (count=%zu p0=%d p1=%d)\n",
                             app.layout.toc_entries.size(),
                             app.layout.toc_entries.empty() ? -1 : app.layout.toc_entries[0].page_num,
                             app.layout.toc_entries.size() < 2 ? -1 : app.layout.toc_entries[1].page_num);
                app.shutdown();
                return 1;
            }
            // Проверка Листа регистрации изменений (ЛРИ) и полностраничной формы Спецификации (Форма 1 ГОСТ Р 2.106).
            app.doc.meta.has_lri = true;
            app.doc_dirty = true;
            app.zoom_to(0.55f, app.doc_viewport.y + app.doc_viewport.h * 0.5f);
            app.zoom_anchor_doc_y = -1.0f;
            app.frame();
            if (!app.layout.page_tops.empty()) {
                app.scroll_y = app.layout.page_tops.back();
            }
            app.frame();
            save_frame("selftest_lri.bmp");

            md::import_markdown(
                "| Формат | Зона | Поз. | Обозначение | Наименование | Кол. | Приме- чание |\n"
                "| :---: | :---: | :---: | :--- | :--- | :---: | :--- |\n"
                "|  |  |  |  | Документация |  |  |\n"
                "| А4 |  |  | АБВГ.123456.001 СБ | Сборочный чертеж |  |  |\n"
                "| А4 |  |  | АБВГ.123456.001 ПЗ | Пояснительная записка |  |  |\n"
                "|  |  |  |  | Сборочные единицы |  |  |\n"
                "| А4 |  | 1 | АБВГ.123456.010 | Корпус | 1 |  |\n"
                "|  |  |  |  | Детали |  |  |\n"
                "| А4 |  | 2 | АБВГ.711111.001 | Крышка | 1 |  |\n"
                "| А4 |  | 3 | АБВГ.711111.002 | Втулка | 2 |  |\n"
                "|  |  |  |  | Стандартные изделия |  |  |\n"
                "|  |  | 4 |  | Винт М4-6g×12.58 ГОСТ 17473-80 | 4 |  |\n"
                "|  |  | 5 |  | Шайба 4.65Г ГОСТ 6402-70 | 4 |  |\n",
                true, app.doc);
            app.doc.meta.form_type = 1;
            app.doc.meta.has_title_page = false;
            app.doc.meta.has_toc = false;
            app.doc.meta.has_lri = false;
            app.doc.meta.code = "АБВГ.123456.001";
            app.doc.meta.name = "Регулятор давления РД-25";
            app.doc.meta.dev = "Иванов";
            app.doc.meta.check = "Петров";
            app.doc.meta.ncheck = "Сидоров";
            app.doc.meta.approve = "Смирнов";
            app.editor.set_caret(md::Pos{0, 0, 0, 0});
            app.scroll_y = 0.0f;
            app.doc_dirty = true;
            app.frame();
            save_frame("selftest_spec.bmp");

            std::printf("selftest gost stamp, title page, TOC, LRI, spec & invariant zoom ok: active=%d lines=%zu\n",
                        static_cast<int>(app.active_stamp_field), lines_100);
        }
        std::printf("selftest links ok: insert, export, file thumbnail\n");
        std::printf("selftest code block tab ok: indent, outdent, selection\n");
        std::printf("selftest inline formulas and code language ok\n");
        std::printf("selftest find/outline ok: search, replace, word count, outline\n");
        std::printf("selftest edit ok: insert/split/merge/select/delete/roundtrip\n");
        app.shutdown();
        return 0;
    }

    // Handle CLI arguments for normal launch:
    // Accept at most one positional file path to open on startup.
    if (argc > 1) {
        std::vector<std::string> positional;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg.empty()) continue;
            if (arg.rfind("-", 0) == 0) {
                dialogs::show_error(std::string(app.tr("Неизвестный параметр запуска:\n",
                                                       "Unknown command-line option:\n")) + arg);
                app.shutdown();
                return 2;
            }
            positional.push_back(arg);
        }
        if (positional.size() > 1) {
            dialogs::show_error(app.tr("Укажите только один файл для открытия при запуске.",
                                       "Please specify only one file to open on startup."));
            app.shutdown();
            return 2;
        }
        if (positional.size() == 1) {
            if (app.open_from_path(positional[0], true, true) != App::OpenResult::Opened) {
                app.shutdown();
                return 1;
            }
        }
    }

    if (argc <= 1) app.offer_recovery();

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            running = app.dispatch(e) && running;
        }
        app.frame();
        app.autosave_tick();
    }
    if (!app.modified) app.remove_autosave();
    app.shutdown();
    return 0;
}
