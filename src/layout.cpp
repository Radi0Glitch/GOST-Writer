#include "layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "chart_draw.hpp"
#include "chart_model.hpp"
#include "diag_draw.hpp"
#include "diag_model.hpp"
#include "math_draw.hpp"
#include "math_model.hpp"
#include "text.hpp"

namespace md {

namespace {

constexpr float kCellPadX = 8.0f;
constexpr float kCellPadY = 5.0f;
constexpr float kMinColumn = 56.0f;
// Height of the title strip painted at the top of a code block card.
constexpr float kCodeHeaderH = 30.0f;

float size_for(const Block& block, int base_pt = 14) {
    const float scale = static_cast<float>(std::clamp(base_pt, 11, 14)) / 14.0f;
    switch (block.type) {
        case BlockType::Heading: {
            // ГОСТ Р 2.105 п. 6.6.2: заголовки разделов выделяют увеличенным размером шрифта,
            // заголовки подразделов — меньше, чем у разделов, но больше основного текста.
            static constexpr float sizes[6] = {21.0f, 19.0f, 17.5f, 17.0f, 17.0f, 17.0f};
            const int level = std::clamp(block.level, 1, 6);
            return sizes[level - 1] * scale;
        }
        case BlockType::Table:
        case BlockType::Quote:
            // ГОСТ Р 2.105 п. 5.1.1: для таблиц и примечаний — на 1–2 пт меньше основного текста.
            return 14.5f * scale;
        case BlockType::CodeBlock:
            return 15.0f * scale;
        default:
            return 17.0f * scale;
    }
}

float measure(TTF_Font* font, const std::string& text, int begin, int end) {
    // NB: TTF_GetStringSize treats length 0 as "the whole C string".
    if (end <= begin) return 0.0f;
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str() + begin, static_cast<size_t>(end - begin), &w, &h);
    return static_cast<float>(w) + 2.0f;
}

struct Atom {
    int inl = 0;
    int begin = 0;
    int end = 0;
    bool space = false;
    float width = 0;
    TTF_Font* font = nullptr;
    bool math = false;  // an inline formula: width and height come from the formula
    float height = 0;
    float ascent = 0;
    bool icon = false;  // the file thumbnail in front of a file link, no text
    int footnote = 0;   // >0: footnote_id (деprecated, use footnote_id)
    int footnote_id = 0;// >0: оригинальный footnote_id из Document.footnotes
};

// Стандартные обозначения единиц физических величин по ГОСТ 8.417-2002 (п. 6.16.6 ГОСТ Р 2.105-2019).
bool is_gost_unit_token(const std::string& s) {
    std::string w = s;
    while (!w.empty() && (w.back() == '.' || w.back() == ',' || w.back() == ';' || w.back() == ':' || w.back() == ')')) {
        w.pop_back();
    }
    static const char* kUnits[] = {
        "мм", "см", "дм", "м", "км", "мг", "г", "кг", "т", "с", "мс", "мкс", "нс", "мин", "ч", "сут",
        "Гц", "кГц", "МГц", "ГГц", "В", "мВ", "кВ", "А", "мкА", "мА", "кА", "Вт", "мВт", "кВт", "МВт",
        "Ом", "кОм", "МОм", "Па", "кПа", "МПа", "ГПа", "Н", "кН", "МН", "Дж", "кДж", "МДж",
        "К", "°С", "°C", "%", "рад", "ср", "л", "мл", "Ф", "пФ", "нФ", "мкФ", "Гн", "мГн",
        "Тл", "Вб", "лм", "лк", "кд", "Б", "дБ", "бит", "байт", "Кбайт", "Мбайт", "Гбайт", "шт"
    };
    for (const char* u : kUnits) {
        if (w == u) return true;
    }
    return false;
}

// The closing $ of an inline formula that opens at `open`, or -1. The rules follow
// the usual ones: no space right after the opening $ or before the closing one,
// no digit right after the closing one (so "$5 and $10" stays plain text).
int find_math_close(const std::string& text, int open) {
    const int size = static_cast<int>(text.size());
    if (open + 2 >= size || text[open + 1] == ' ' || text[open + 1] == '$') return -1;
    for (int q = open + 1; q < size; ++q) {
        if (text[q] == '\n') return -1;
        if (text[q] != '$') continue;
        if (text[q - 1] == ' ') return -1;
        if (q + 1 < size && text[q + 1] >= '0' && text[q + 1] <= '9') return -1;
        return q;
    }
    return -1;
}

struct BlockMetrics {
    float indent = 0;       // text indent inside the content column
    float first_indent = 0; // additional first-line paragraph indent (ГОСТ 5.1.4: 12.5 мм)
    float pad_before = 0;
    float pad_after = 0;
    float leading = 10.0f;  // extra space between lines
    bool preformatted = false;
};

BlockMetrics metrics_for(const Block& block, bool first, int line_spacing_idx = 3) {
    const float ls_scale = gost_line_spacing(line_spacing_idx) / 1.5f;
    BlockMetrics m;
    m.leading = 10.0f * ls_scale;
    switch (block.type) {
        case BlockType::Heading:
            // ГОСТ Р 2.105 п. 6.5.4, 6.6.3: с абзацного отступа (12,5 мм), расстояние не менее 2 высот шрифта.
            m.first_indent = 48.0f;
            m.pad_before = first ? 6.0f : 26.0f;
            m.pad_after = 16.0f;
            break;
        case BlockType::Quote:
            m.first_indent = 48.0f;
            m.pad_before = 6.0f;
            m.pad_after = 10.0f;
            break;
        case BlockType::CodeBlock:
            m.indent = 0.0f;
            m.pad_before = 10.0f;
            m.pad_after = 14.0f;
            m.leading = 2.0f;
            m.preformatted = true;
            break;
        case BlockType::ListItem:
            // ГОСТ Р 2.105 п. 6.7.2: перечисления записывают с абзацного отступа.
            m.indent = 48.0f + 24.0f * static_cast<float>(block.depth);
            m.pad_after = 6.0f * ls_scale;
            break;
        case BlockType::Rule:
            m.pad_before = 12.0f;
            m.pad_after = 14.0f;
            break;
        case BlockType::Table:
            m.pad_before = 10.0f;
            m.pad_after = 14.0f;
            break;
        default:
            // ГОСТ Р 2.105 п. 6.10.1: строка пояснения формулы «где ...» начинается без абзацного отступа.
            if (!block.inlines.empty() && block.inlines[0].text.rfind("где ", 0) == 0) {
                m.first_indent = 0.0f;
            } else {
                m.first_indent = 48.0f; // ~12.5 мм (5 знаков по ГОСТ Р 2.105 п. 5.1.4)
            }
            m.pad_after = 8.0f * ls_scale;
            break;
    }
    return m;
}

// Accumulates lines for one text container.
class LineBuilder {
public:
    LineBuilder(int block_index, int cell, std::uint8_t extra_style,
                const BlockMetrics& metrics, float width)
        : block_index_(block_index), cell_(cell), extra_(extra_style),
          metrics_(metrics), width_(width) {
        new_line(true);
    }

    void add(const Atom& atom) {
        if (atom.math) {
            max_height_ = std::max(max_height_, atom.height);
            max_ascent_ = std::max(max_ascent_, atom.ascent);
        } else {
            update_metrics(atom.font);
        }
        line_.spans.push_back(Span{atom.inl, atom.begin, atom.end, cursor_, atom.width,
                                   atom.math, atom.icon, atom.footnote, atom.footnote_id});
        cursor_ += atom.width;
    }

    bool fits(const Atom& atom) const {
        return cursor_ + atom.width <= width_;
    }

    bool empty() const { return line_.spans.empty(); }

    void set_default_font(TTF_Font* font) {
        default_height_ = static_cast<float>(TTF_GetFontHeight(font));
        default_ascent_ = static_cast<float>(TTF_GetFontAscent(font));
    }

    void finish_line(Layout& out, float& y) {
        line_.height = max_height_ > 0 ? max_height_ : default_height_;
        line_.baseline = max_ascent_ > 0 ? max_ascent_ : default_ascent_;
        line_.y = y;
        out.lines.push_back(line_);
        y += line_.height + metrics_.leading;
        max_height_ = 0;
        max_ascent_ = 0;
        new_line(false);
    }

private:
    void new_line(bool first_line) {
        line_ = Line{};
        line_.block = block_index_;
        line_.cell = cell_;
        line_.extra_style = extra_;
        line_.x0 = metrics_.indent + (first_line ? metrics_.first_indent : 0.0f);
        cursor_ = line_.x0;
    }

    void update_metrics(TTF_Font* font) {
        if (!font) return;
        max_height_ = std::max(max_height_, static_cast<float>(TTF_GetFontHeight(font)));
        max_ascent_ = std::max(max_ascent_, static_cast<float>(TTF_GetFontAscent(font)));
    }

    int block_index_;
    int cell_;
    std::uint8_t extra_;
    const BlockMetrics& metrics_;
    float width_;
    Line line_;
    float cursor_ = 0;
    float max_height_ = 0;
    float max_ascent_ = 0;
    float default_height_ = 0;
    float default_ascent_ = 0;
};

// Lays out the inlines of one text container (a block or a table cell) from `y`
// downwards. `width` is the right edge of the usable column.
void layout_text(const Block& block, const std::vector<Inline>& inlines, int block_index,
                 int cell, std::uint8_t extra_style, FontBook& fonts,
                 const BlockMetrics& metrics, float width, float& y, Layout& out,
                 const MathReveal& reveal) {
    LineBuilder builder(block_index, cell, extra_style, metrics, width);
    builder.set_default_font(font_for(fonts, block, extra_style));
    const float avail = width - metrics.indent;

    // Tokenize into words and space runs.
    std::vector<Atom> atoms;
    for (size_t ii = 0; ii < inlines.size(); ++ii) {
        const Inline& inl = inlines[ii];
        TTF_Font* font = font_for(fonts, block, inl.style | extra_style);
        if (inl.footnote_id > 0) {
            TTF_Font* f_sup = fonts.get(Family::Serif, false, false, 11.5f);
            int tw = 0, th = 0;
            if (TTF_GetStringSize(f_sup, "1)", 2, &tw, &th)) {
            } else {
                tw = 8;
            }
            Atom a{static_cast<int>(ii), 0, 1, false, static_cast<float>(tw) + 2.0f, f_sup};
            a.footnote = inl.footnote_id;   // исходный ID — später заменится на порядковый номер на странице
            a.footnote_id = inl.footnote_id;
            atoms.push_back(a);
            continue;
        }
        const std::string& text = inl.text;
        int pos = 0;
        const bool continues_chip = ii > 0 && is_file_link(inlines[ii - 1])
                                    && inlines[ii - 1].href == inl.href;
        if (is_file_link(inl) && !inl.text.empty() && !continues_chip) {
            Atom icon{static_cast<int>(ii), 0, 0, false, file_icon_width(fonts.zoom()), font};
            icon.icon = true;
            atoms.push_back(icon);
        }
        while (pos < static_cast<int>(text.size())) {
            // A formula starts at a word boundary; the revealed one stays plain text.
            if (text[pos] == '$' && (pos == 0 || text[pos - 1] == ' ') && !(inl.style & StyleCode)
                && !(reveal.block == block_index && reveal.cell == cell && reveal.inl == static_cast<int>(ii)
                     && reveal.begin == pos)) {
                const int close = find_math_close(text, pos);
                if (close > 0) {
                    const float fsize = math_size(block, fonts.zoom());
                    const auto sz = mathdraw::render(nullptr, fonts, math_from_latex(text.substr(
                                                         pos + 1, close - pos - 1)),
                                                     0, 0, fsize, nullptr, nullptr);
                    Atom a{static_cast<int>(ii), pos, close + 1, false, sz.w + 4.0f, font};
                    a.math = true;
                    a.height = sz.h;
                    a.ascent = sz.h * 0.8f;
                    atoms.push_back(a);
                    pos = close + 1;
                    continue;
                }
            }
            // Старый формат ^[...] при наличии footnote_id в инлайне игнорируется; конвертируется при импорте
            const bool space = text[pos] == ' ';
            int end = pos;
            while (end < static_cast<int>(text.size())
                   && (text[end] == ' ') == space) {
                end = next_char(text, end);
            }
            atoms.push_back(Atom{static_cast<int>(ii), pos, end, space,
                                 measure(font, text, pos, end), font});
            pos = end;
        }
    }

    // ГОСТ Р 2.105 п. 6.16.6: недопустимо отделять единицу физической величины от числового значения
    // переносом на другую строку. Склеиваем «число + пробел + единица» в один неделимый атом.
    for (size_t k = 0; k + 2 < atoms.size();) {
        Atom& a0 = atoms[k];
        const Atom& a1 = atoms[k + 1];
        const Atom& a2 = atoms[k + 2];
        if (!a0.space && !a0.math && !a0.icon && a0.footnote == 0
            && a1.space && !a2.space && !a2.math && !a2.icon && a2.footnote == 0
            && a0.inl == a1.inl && a1.inl == a2.inl) {
            const std::string& txt = inlines[static_cast<size_t>(a0.inl)].text;
            if (a0.end > a0.begin) {
                const char last_ch = txt[static_cast<size_t>(a0.end - 1)];
                const std::string unit_str = txt.substr(static_cast<size_t>(a2.begin),
                                                        static_cast<size_t>(a2.end - a2.begin));
                if (last_ch >= '0' && last_ch <= '9' && is_gost_unit_token(unit_str)) {
                    a0.end = a2.end;
                    a0.width = measure(a0.font, txt, a0.begin, a0.end);
                    atoms.erase(atoms.begin() + static_cast<std::ptrdiff_t>(k + 1),
                                atoms.begin() + static_cast<std::ptrdiff_t>(k + 3));
                    continue;
                }
            }
        }
        ++k;
    }

    if (atoms.empty()) {
        builder.finish_line(out, y); // keep empty blocks and cells visible
    }

    for (const Atom& atom : atoms) {
        if (builder.empty() && atom.space) {
            continue; // no leading spaces after a wrap
        }
        if (!builder.fits(atom) && !builder.empty()) {
            builder.finish_line(out, y);
            if (atom.space) continue;
        }
        if (!builder.fits(atom) && builder.empty() && !atom.space && !atom.math
            && atom.footnote == 0 && !atom.icon) {
            // Single word wider than the column: split by UTF-8 chars.
            const std::string& text = inlines[atom.inl].text;
            int start = atom.begin;
            while (start < atom.end) {
                int best = -1;
                int pos = start;
                while (pos < atom.end) {
                    const int next = next_char(text, pos);
                    const float w = measure(atom.font, text, start, next);
                    if (w > avail) break;
                    best = next;
                    pos = next;
                }
                if (best <= start) best = next_char(text, start); // always progress
                Atom piece{atom.inl, start, best, false,
                           measure(atom.font, text, start, best), atom.font};
                builder.add(piece);
                builder.finish_line(out, y);
                start = best;
            }
            continue;
        }
        builder.add(atom);
    }
    if (!builder.empty()) {
        builder.finish_line(out, y);
    }
}

float cell_text_width(FontBook& fonts, const Block& block, const std::vector<Inline>& cell,
                      std::uint8_t extra_style) {
    float w = 0;
    for (const Inline& inl : cell) {
        TTF_Font* font = font_for(fonts, block, inl.style | extra_style);
        w += measure(font, inl.text, 0, static_cast<int>(inl.text.size()));
    }
    return w;
}

// Shifts the lines laid out since `from` inside [left, right] so the text is
// centred or right-aligned. Left alignment needs no shift.
void align_lines(Layout& out, size_t from, TableAlign align, float left, float right) {
    if (align == TableAlign::None || align == TableAlign::Left) return;
    for (size_t i = from; i < out.lines.size(); ++i) {
        Line& line = out.lines[i];
        float used = 0;
        if (!line.spans.empty()) {
            const Span& last = line.spans.back();
            used = last.x + last.width - line.x0;
        }
        const float free = (right - left) - used;
        if (free <= 0) continue;
        const float shift = align == TableAlign::Center ? free / 2.0f : free;
        for (Span& span : line.spans) span.x += shift;
        line.x0 += shift;
    }
}

// Columns share the content width in proportion to their natural width. Rows
// grow to fit wrapped text. The header row is bold.
void layout_table(const Block& block, int bi, int form_type, FontBook& fonts, float width, float& y,
                  Layout& out, const MathReveal& reveal) {
    const int cols = table_cols(block);
    const int rows = table_rows(block);
    if (cols == 0 || rows == 0) {
        out.boxes.push_back(BlockBox{0, y, width, 0});
        return;
    }

    const float top = y;
    const float z = fonts.zoom();
    const float mm = std::max(1.0f, width / 175.0f);
    const bool is_fullpage_form = (form_type == 1 && cols == 7)
                               || (form_type == 2 && cols == 3)
                               || (form_type == 3 && cols == 7);

    if (!is_fullpage_form) {
        // ГОСТ Р 2.105 п. 6.8.1: место над таблицей для наименования «Таблица N»
        y += 22.0f * z;
    }

    TableGrid grid;
    grid.block = bi;
    grid.cols = cols;

    if (is_fullpage_form) {
        // Полностраничная форма ГОСТ Р 2.106-2019 (Приложение А) от левой до правой линии рамки (185 мм):
        grid.xs.push_back(-5.0f * mm);
        static const float kSpecCols[7] = {6.0f, 6.0f, 8.0f, 70.0f, 63.0f, 10.0f, 22.0f};
        static const float kVdCols[3]   = {7.0f, 70.0f, 108.0f};
        static const float kTpCols[7]   = {7.0f, 8.0f, 70.0f, 64.0f, 8.0f, 8.0f, 20.0f};
        const float* col_mm = (form_type == 1) ? kSpecCols : (form_type == 2) ? kVdCols : kTpCols;
        for (int c = 0; c < cols; ++c) {
            grid.xs.push_back(grid.xs.back() + col_mm[c] * mm);
        }
    } else {
        std::vector<float> natural(static_cast<size_t>(cols), kMinColumn);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const std::uint8_t extra = r == 0 ? StyleBold : 0;
                const float w = cell_text_width(fonts, block, block.cells[r * cols + c], extra)
                    + 2.0f * kCellPadX * z;
                natural[c] = std::max(natural[c], w);
            }
        }
        float total = 0;
        for (float w : natural) total += w;
        const float scale = total > 0 ? width / total : 1.0f;

        grid.xs.push_back(0);
        for (int c = 0; c < cols; ++c) {
            grid.xs.push_back(grid.xs.back() + std::max(kMinColumn, natural[c] * scale));
        }
    }

    grid.ys.push_back(y);
    const float min_row_h = 8.0f * mm; // ГОСТ Р 2.105 п. 6.8.5: высота строк не менее 8 мм
    for (int r = 0; r < rows; ++r) {
        float row_bottom = y;
        const float pad_x = is_fullpage_form ? 2.5f * z : kCellPadX * z;

        // Проверяем, является ли строка заголовком раздела в графе «Наименование» (остальные графы пустые)
        bool is_sec_header_row = false;
        if (is_fullpage_form && r > 0) {
            int non_empty_cols = 0;
            for (int c = 0; c < cols; ++c) {
                bool empty_c = true;
                for (const Inline& inl : block.cells[static_cast<size_t>(r * cols + c)]) {
                    if (!inl.text.empty()) { empty_c = false; break; }
                }
                if (!empty_c) ++non_empty_cols;
            }
            if (non_empty_cols == 1) is_sec_header_row = true;
        }

        for (int c = 0; c < cols; ++c) {
            const int index = r * cols + c;
            const float col_w = grid.xs[c + 1] - grid.xs[c];
            const bool vertical_hdr = (is_fullpage_form && r == 0 && col_w <= 12.5f * mm);
            const float cell_px = (is_fullpage_form && r == 0) ? 1.0f * z : pad_x;
            BlockMetrics cell_metrics;
            cell_metrics.indent = grid.xs[c] + cell_px;
            float cy = y + kCellPadY * z;
            const size_t first_line = out.lines.size();
            layout_text(block, block.cells[index], bi, index, r == 0 ? StyleBold : 0, fonts,
                        cell_metrics, vertical_hdr ? (grid.xs[c] + 200.0f * mm) : (grid.xs[c + 1] - cell_px),
                        cy, out, reveal);
            const TableAlign eff_align = (is_sec_header_row || (is_fullpage_form && r == 0))
                ? TableAlign::Center
                : block.aligns[c];
            align_lines(out, first_line, eff_align, cell_metrics.indent,
                        grid.xs[c + 1] - cell_px);
            if (!vertical_hdr) {
                row_bottom = std::max(row_bottom, cy - cell_metrics.leading);
            }
        }
        const float req_h = (is_fullpage_form && r == 0)
            ? ((form_type == 1 ? 15.0f : 20.0f) * mm)
            : min_row_h;
        y = std::max(row_bottom + kCellPadY * z, y + req_h);
        grid.ys.push_back(y);
    }

    out.tables.push_back(grid);
    out.boxes.push_back(BlockBox{is_fullpage_form ? -5.0f * mm : 0.0f, top,
                                 is_fullpage_form ? 185.0f * mm : width, y - top});
}

// Index of the grid cell (column or row) that contains `v`. `edges` has n + 1 values.
int edge_cell(const std::vector<float>& edges, float v) {
    const int n = static_cast<int>(edges.size()) - 1;
    for (int i = 0; i < n; ++i) {
        if (v < edges[static_cast<size_t>(i) + 1]) return i;
    }
    return std::max(0, n - 1);
}

const TableGrid* grid_for(const Layout& layout, int block) {
    for (const TableGrid& g : layout.tables) {
        if (g.block == block) return &g;
    }
    return nullptr;
}

std::string block_plain_text(const Block& b) {
    std::string s;
    for (const Inline& inl : b.inlines) s += inl.text;
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

bool starts_with_ci_utf8(const std::string& text, const char* prefix) {
    const size_t plen = std::strlen(prefix);
    if (text.size() < plen) return false;
    return text.compare(0, plen, prefix) == 0;
}

bool is_unnumbered_structural_heading(const std::string& t) {
    static const char* kStructural[] = {
        "Содержание", "СОДЕРЖАНИЕ", "содержание",
        "Предисловие", "ПРЕДИСЛОВИЕ", "предисловие",
        "Введение", "ВВЕДЕНИЕ", "введение",
        "Обозначения и сокращения", "ОБОЗНАЧЕНИЯ И СОКРАЩЕНИЯ",
        "Термины и определения", "ТЕРМИНЫ И ОПРЕДЕЛЕНИЯ",
        "Ссылочные нормативные документы", "ССЫЛОЧНЫЕ НОРМАТИВНЫЕ ДОКУМЕНТЫ",
        "Ссылочные документы", "ССЫЛОЧНЫЕ ДОКУМЕНТЫ",
        "Библиография", "БИБЛИОГРАФИЯ", "библиография",
        "Список использованных источников", "СПИСОК ИСПОЛЬЗОВАННЫХ ИСТОЧНИКОВ"
    };
    for (const char* s : kStructural) {
        if (t == s) return true;
    }
    return false;
}

} // namespace

std::vector<GostBlockNum> compute_gost_numbering(const Document& doc) {
    // ГОСТ Р 2.105 п. 6.3.5: прописные буквы русского алфавита без Ё, З, Й, О, Ч, Ь, Ы, Ъ
    static const char* kAppLetters[] = {
        "А", "Б", "В", "Г", "Д", "Е", "Ж", "И", "К", "Л", "М", "Н",
        "П", "Р", "С", "Т", "У", "Ф", "Х", "Ц", "Ш", "Щ", "Э", "Ю", "Я"
    };
    constexpr int kAppLetterCount = static_cast<int>(sizeof(kAppLetters) / sizeof(kAppLetters[0]));

    std::vector<GostBlockNum> res(doc.blocks.size());
    int sec_num = 0;
    int sub_num = 0;
    int item_num = 0;
    int subitem_num = 0;

    int app_idx = 0;
    std::string cur_app; // пусто в основной части, иначе буква приложения ("А", "Б", ...)
    int app_sec = 0;
    int app_sub = 0;
    int app_item = 0;

    int global_tbl = 0, sec_tbl = 0, app_tbl = 0;
    int global_fig = 0, sec_fig = 0, app_fig = 0;
    int global_math = 0, sec_math = 0, app_math = 0;

    for (size_t bi = 0; bi < doc.blocks.size(); ++bi) {
        const Block& b = doc.blocks[bi];
        GostBlockNum& gn = res[bi];

        if (b.type == BlockType::Heading) {
            const std::string txt = block_plain_text(b);
            const int lvl = std::clamp(b.level, 1, 6);

            // Проверяем, не начинается ли заголовок со слова «Приложение»
            if (starts_with_ci_utf8(txt, "Приложение") || starts_with_ci_utf8(txt, "ПРИЛОЖЕНИЕ")) {
                cur_app = kAppLetters[app_idx % kAppLetterCount];
                // Если буква уже указана после слова «Приложение », используем её
                const size_t sp = txt.find(' ');
                if (sp != std::string::npos) {
                    std::string rest = txt.substr(sp + 1);
                    while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
                    for (int k = 0; k < kAppLetterCount; ++k) {
                        if (starts_with_ci_utf8(rest, kAppLetters[k])) {
                            cur_app = kAppLetters[k];
                            app_idx = k;
                            break;
                        }
                    }
                }
                ++app_idx;
                app_sec = 0;
                app_sub = 0;
                app_item = 0;
                app_tbl = 0;
                app_fig = 0;
                app_math = 0;
                gn.starts_new_page = (bi > 0);
                gn.centered_heading = true;
                continue;
            }

            if (lvl == 1 && is_unnumbered_structural_heading(txt)) {
                if (txt == "Содержание" || txt == "СОДЕРЖАНИЕ" || txt == "содержание") {
                    gn.centered_heading = true;
                }
                continue;
            }

            // Проверяем, не ввёл ли пользователь номер вручную в начале заголовка (например "1 Введение" или "1.2 Расчёт")
            bool manual_num = (!txt.empty() && txt[0] >= '1' && txt[0] <= '9');
            if (manual_num) {
                const int parsed = std::atoi(txt.c_str());
                if (lvl == 1 && cur_app.empty() && parsed > 0) {
                    sec_num = parsed;
                    sub_num = 0;
                    item_num = 0;
                    subitem_num = 0;
                    sec_tbl = 0;
                    sec_fig = 0;
                    sec_math = 0;
                } else if (lvl == 2 && cur_app.empty()) {
                    const size_t dot = txt.find('.');
                    if (dot != std::string::npos && dot + 1 < txt.size()) {
                        const int s2 = std::atoi(txt.c_str() + dot + 1);
                        if (s2 > 0) sub_num = s2;
                    }
                    item_num = 0;
                    subitem_num = 0;
                }
                continue;
            }

            if (!cur_app.empty()) {
                // Внутри приложения (п. 6.3.7): А.1, А.1.1, А.1.1.1
                if (lvl <= 2) {
                    ++app_sec;
                    app_sub = 0;
                    app_item = 0;
                    if (doc.meta.auto_num_headings) {
                        gn.heading_prefix = cur_app + "." + std::to_string(app_sec) + " ";
                    }
                } else if (lvl == 3) {
                    if (app_sec == 0) app_sec = 1;
                    ++app_sub;
                    app_item = 0;
                    if (doc.meta.auto_num_headings) {
                        gn.heading_prefix = cur_app + "." + std::to_string(app_sec) + "." + std::to_string(app_sub) + " ";
                    }
                } else {
                    if (app_sec == 0) app_sec = 1;
                    ++app_item;
                    if (doc.meta.auto_num_headings) {
                        if (app_sub > 0) {
                            gn.heading_prefix = cur_app + "." + std::to_string(app_sec) + "." + std::to_string(app_sub) + "." + std::to_string(app_item) + " ";
                        } else {
                            gn.heading_prefix = cur_app + "." + std::to_string(app_sec) + "." + std::to_string(app_item) + " ";
                        }
                    }
                }
            } else {
                // Основная часть документа (п. 6.5.4–6.5.9)
                if (lvl == 1) {
                    ++sec_num;
                    sub_num = 0;
                    item_num = 0;
                    subitem_num = 0;
                    sec_tbl = 0;
                    sec_fig = 0;
                    sec_math = 0;
                    if (doc.meta.auto_num_headings) {
                        gn.heading_prefix = std::to_string(sec_num) + " ";
                    }
                } else if (lvl == 2) {
                    if (sec_num == 0) sec_num = 1;
                    ++sub_num;
                    item_num = 0;
                    subitem_num = 0;
                    if (doc.meta.auto_num_headings) {
                        gn.heading_prefix = std::to_string(sec_num) + "." + std::to_string(sub_num) + " ";
                    }
                } else if (lvl == 3) {
                    if (sec_num == 0) sec_num = 1;
                    ++item_num;
                    subitem_num = 0;
                    if (doc.meta.auto_num_headings) {
                        if (sub_num > 0) {
                            // п. 6.5.6: есть подразделы -> 1.1.1
                            gn.heading_prefix = std::to_string(sec_num) + "." + std::to_string(sub_num) + "." + std::to_string(item_num) + " ";
                        } else {
                            // п. 6.5.5: нет подразделов -> 1.1
                            gn.heading_prefix = std::to_string(sec_num) + "." + std::to_string(item_num) + " ";
                        }
                    }
                } else {
                    if (sec_num == 0) sec_num = 1;
                    if (item_num == 0) item_num = 1;
                    ++subitem_num;
                    if (doc.meta.auto_num_headings) {
                        if (sub_num > 0) {
                            gn.heading_prefix = std::to_string(sec_num) + "." + std::to_string(sub_num) + "."
                                              + std::to_string(item_num) + "." + std::to_string(subitem_num) + " ";
                        } else {
                            gn.heading_prefix = std::to_string(sec_num) + "." + std::to_string(item_num) + "."
                                              + std::to_string(subitem_num) + " ";
                        }
                    }
                }
            }
        } else if (b.type == BlockType::Table) {
            ++global_tbl;
            ++sec_tbl;
            ++app_tbl;
            std::string num_part;
            if (!cur_app.empty()) {
                num_part = cur_app + "." + std::to_string(app_tbl);
            } else if (doc.meta.section_num_media && sec_num > 0) {
                num_part = std::to_string(sec_num) + "." + std::to_string(sec_tbl);
            } else {
                num_part = std::to_string(global_tbl);
            }
            gn.table_caption = "Таблица " + num_part;
            if (!b.lang.empty()) {
                gn.table_caption += " — " + b.lang;
            }
        } else if (b.type == BlockType::CodeBlock) {
            const std::string& l = b.lang;
            const bool is_math = (l == "math" || l == "latex" || l == "tex");
            const bool is_diag = is_diagram_lang(l) && !is_math;
            if (is_math) {
                ++global_math;
                ++sec_math;
                ++app_math;
                if (!cur_app.empty()) {
                    gn.formula_number = "(" + cur_app + "." + std::to_string(app_math) + ")";
                } else if (doc.meta.section_num_media && sec_num > 0) {
                    gn.formula_number = "(" + std::to_string(sec_num) + "." + std::to_string(sec_math) + ")";
                } else {
                    gn.formula_number = "(" + std::to_string(global_math) + ")";
                }
            } else if (is_diag) {
                ++global_fig;
                ++sec_fig;
                ++app_fig;
                std::string num_part;
                if (!cur_app.empty()) {
                    num_part = cur_app + "." + std::to_string(app_fig);
                } else if (doc.meta.section_num_media && sec_num > 0) {
                    num_part = std::to_string(sec_num) + "." + std::to_string(sec_fig);
                } else {
                    num_part = std::to_string(global_fig);
                }
                gn.figure_caption = "Рисунок " + num_part;
                std::string code = block_plain_text(b);
                if (md::is_chart_text(code)) {
                    bool ok_c = false;
                    const ChartModel cm = chart_from_text(code, &ok_c);
                    if (ok_c && !cm.title.empty()) {
                        gn.figure_caption += " — " + cm.title;
                    }
                }
            }
        } else if (b.type == BlockType::Quote) {
            // ГОСТ Р 2.105 п. 6.12.2–6.12.3: одиночное «Примечание — ...» или группа «Примечания» 1, 2...
            const std::string txt = block_plain_text(b);
            if (starts_with_ci_utf8(txt, "Примечани") || starts_with_ci_utf8(txt, "Пример")) {
                continue;
            }
            size_t group_start = bi;
            while (group_start > 0 && doc.blocks[group_start - 1].type == BlockType::Quote) {
                --group_start;
            }
            size_t group_end = bi + 1;
            while (group_end < doc.blocks.size() && doc.blocks[group_end].type == BlockType::Quote) {
                ++group_end;
            }
            const size_t group_len = group_end - group_start;
            if (group_len == 1) {
                gn.note_prefix = "Примечание — ";
            } else {
                const int idx = static_cast<int>(bi - group_start) + 1;
                if (bi == group_start) gn.note_group_header = true;
                gn.note_prefix = std::to_string(idx) + " ";
            }
        }
    }

    return res;
}

TTF_Font* font_for(FontBook& fonts, const Block& block, std::uint8_t style) {
    // Code blocks are plain monospace: emphasis inside them is not shown.
    const bool code_block = block.type == BlockType::CodeBlock;
    const bool bold = (!code_block && (style & StyleBold))
        || block.type == BlockType::Heading;
    const bool italic = !code_block && (style & StyleItalic);
    const bool strike = !code_block && (style & StyleStrike);
    const Family family = (style & StyleCode) || block.type == BlockType::CodeBlock
        ? Family::Mono : Family::Serif;
    return fonts.get(family, bold, italic, size_for(block, fonts.base_pt()), strike);
}

void layout_document(const Document& doc, FontBook& fonts, float width, Layout& out,
                     const MathReveal& reveal) {
    out = Layout{};
    out.block_nums = compute_gost_numbering(doc);

    // Эталонная ширина текстовой области листа А4 (175 мм при 1 мм = 4.0 ед.) = 700.0f.
    // Вёрстка всегда выполняется в эталонных координатах А4 (zoom = 1.0), а затем масштабируется
    // на page_scale = width / 700.0f, чтобы соотношение размеров страницы и текста никогда не искажалось.
    constexpr float kRefWidth = 700.0f;
    const float page_scale = (width > 1.0f) ? (width / kRefWidth) : 1.0f;
    const float saved_zoom = fonts.zoom();
    fonts.set_base_pt(doc.meta.font_pt);
    fonts.set_zoom(1.0f);
    width = kRefWidth;

    float y = 0;
    std::vector<int> numbers(doc.blocks.size(), 0); // ordered list numbers

    for (size_t bi = 0; bi < doc.blocks.size(); ++bi) {
        const Block& block = doc.blocks[bi];
        const GostBlockNum& gn = out.block_nums[bi];
        BlockMetrics metrics = metrics_for(block, bi == 0, doc.meta.line_spacing_idx);
        const float z = fonts.zoom();
        metrics.pad_before *= z;
        metrics.pad_after *= z;
        metrics.indent *= z;
        metrics.first_indent *= z;
        metrics.leading *= z;

        if (gn.centered_heading) {
            metrics.first_indent = 0.0f;
        } else if (!gn.heading_prefix.empty()) {
            TTF_Font* hf = font_for(fonts, block, 0);
            metrics.first_indent += measure(hf, gn.heading_prefix, 0, static_cast<int>(gn.heading_prefix.size()));
        } else if (!gn.note_prefix.empty()) {
            TTF_Font* qf = font_for(fonts, block, 0);
            metrics.first_indent += measure(qf, gn.note_prefix, 0, static_cast<int>(gn.note_prefix.size()));
        }
        if (gn.note_group_header) {
            metrics.pad_before += 20.0f * z; // место под заголовок «Примечания»
        }

        // A list ends with a little more air before the next block.
        if (bi > 0 && doc.blocks[bi - 1].type == BlockType::ListItem
            && block.type != BlockType::ListItem) {
            y += 8.0f * z;
        }

        // ГОСТ Р 2.105 п. 6.7.2: дефис «-», строчные буквы со скобкой «а)» или цифры «1)» / «1.».
        static const char* kGostLetters[] = {
            "а", "б", "в", "г", "д", "е", "ж", "и", "к", "л", "м", "н",
            "п", "р", "с", "т", "у", "ф", "х", "ц", "ш", "щ", "э", "ю", "я"
        };
        constexpr int kGostLetterCount = static_cast<int>(sizeof(kGostLetters) / sizeof(kGostLetters[0]));
        std::string marker;
        if (block.type == BlockType::ListItem && block.ordered) {
            int n = 1;
            for (size_t j = bi; j-- > 0;) {
                const Block& prev = doc.blocks[j];
                if (prev.type != BlockType::ListItem || prev.depth < block.depth) break;
                if (prev.depth == block.depth) {
                    if (prev.ordered) n = numbers[j] + 1;
                    break;
                }
            }
            numbers[bi] = n;
            if (block.level == 2) {
                if (block.depth == 0) {
                    marker = std::string(kGostLetters[(n - 1) % kGostLetterCount]) + ")";
                } else {
                    marker = std::to_string(n) + ")";
                }
            } else {
                marker = std::to_string(n) + ".";
            }
        } else if (block.type == BlockType::ListItem) {
            marker = "-";
        }
        out.markers.push_back(marker);

        // The marker is painted left of the text column (x0 - 10 - marker_width),
        // so two- and three-digit numbers need a wider indent than the default
        // bullet layout, otherwise the leading digits get cut off.
        if (block.type == BlockType::ListItem && !marker.empty()) {
            TTF_Font* mf = font_for(fonts, block, 0);
            const float mw = measure(mf, marker, 0, static_cast<int>(marker.size()));
            const float needed = 10.0f * z + mw + 6.0f * z;
            if (metrics.indent < needed) metrics.indent = needed;
        }

        y += metrics.pad_before;

        if (block.type == BlockType::Table) {
            if (doc.meta.form_type > 0 && bi == 0) {
                y -= metrics.pad_before; // Полностраничная форма примыкает к верхней линии рамки
            }
            layout_table(block, static_cast<int>(bi), doc.meta.form_type, fonts, width, y, out, reveal);
            y += metrics.pad_after;
            continue;
        }

        const float box_top = y;

        if (metrics.preformatted) {
            const std::string& bl = block.lang;
            if (bl == "math" || bl == "latex" || bl == "tex") {
                // A formula block reserves the height of its rendered tree.
                std::string tex;
                for (const Inline& inl : block.inlines) tex += inl.text;
                const MathNode node = math_from_latex(tex);
                const auto sz = mathdraw::render(nullptr, fonts, node, 0, 0, 18.0f * z,
                                                 nullptr, nullptr);
                LineBuilder builder(static_cast<int>(bi), -1, 0, metrics, width);
                builder.set_default_font(font_for(fonts, block, 0));
                Atom a{0, 0, 0, false, 0.0f, font_for(fonts, block, 0)};
                builder.add(a);
                builder.finish_line(out, y);
                y += sz.h;
            } else if (is_diagram_lang(bl)) {
                // A diagram block reserves the height of its drawn picture;
                // the code itself never reaches the screen.
                std::string code;
                for (const Inline& inl : block.inlines) code += inl.text;
                bool is_chart = md::is_chart_text(code);
                md::diagdraw::Size sz{};
                if (is_chart) {
                    const auto csz = chartdraw::render_block(nullptr, fonts, code, 0, 0, width - 28.0f);
                    sz.w = csz.w; sz.h = csz.h;
                } else {
                    sz = diagdraw::render_block(nullptr, fonts, code, 0, 0, width - 28.0f);
                }
                LineBuilder builder(static_cast<int>(bi), -1, 0, metrics, width);
                builder.set_default_font(font_for(fonts, block, 0));
                Atom a{0, 0, 0, false, 0.0f, font_for(fonts, block, 0)};
                builder.add(a);
                builder.finish_line(out, y);
                y += sz.h + 24.0f * z; // ГОСТ Р 2.105 п. 6.9.4: место под подрисуночную подпись «Рисунок N»
            } else {
                // Code block: one layout line per source line, no wrapping.
                y += kCodeHeaderH * z;
                LineBuilder builder(static_cast<int>(bi), -1, 0, metrics, width);
                builder.set_default_font(font_for(fonts, block, 0));
                for (size_t ii = 0; ii < block.inlines.size(); ++ii) {
                    const Inline& inl = block.inlines[ii];
                    TTF_Font* font = font_for(fonts, block, inl.style);
                    const std::string& text = inl.text;
                    int start = 0;
                    for (int pos = 0; pos <= static_cast<int>(text.size()); ++pos) {
                        if (pos == static_cast<int>(text.size()) || text[pos] == '\n') {
                            Atom a{static_cast<int>(ii), start, pos, false,
                                   measure(font, text, start, pos), font};
                            builder.add(a);
                            builder.finish_line(out, y);
                            start = pos + 1;
                        }
                    }
                }
                if (block.inlines.empty()) {
                    builder.finish_line(out, y);
                }
            }
        } else {
            const size_t first_line_idx = out.lines.size();
            layout_text(block, block.inlines, static_cast<int>(bi), -1, 0, fonts, metrics,
                        width, y, out, reveal);
            if (gn.centered_heading) {
                align_lines(out, first_line_idx, TableAlign::Center, 0.0f, width);
            }
        }

        const float box_bottom = y - metrics.leading;
        y = box_bottom + metrics.pad_after;

        // Background geometry for render: code blocks and quotes get padding.
        const std::string& bl2 = block.lang;
        const bool is_formula_b = block.type == BlockType::CodeBlock && (bl2 == "math" || bl2 == "latex" || bl2 == "tex");
        const bool is_diagram_b = block.type == BlockType::CodeBlock && is_diagram_lang(bl2);
        const float pad = (block.type == BlockType::CodeBlock && !is_formula_b && !is_diagram_b) ? 10.0f * z
                        : block.type == BlockType::Quote ? 8.0f * z : 0.0f;
        out.boxes.push_back(BlockBox{0, box_top - pad, width,
                                     box_bottom - box_top + pad * 2.0f});
    }

    // Постраничная разбивка на листы А4 (210x297 мм) с рамкой (185x287 мм)
    // и основной надписью ГОСТ 2.104 (Форма 2 = 40 мм на 1-м листе, Форма 2а = 15 мм на последующих).
    const float mm = std::max(1.0f, width / 175.0f);
    out.mm_px = mm;
    const float page_h = 297.0f * mm;
    const float page_gap = 12.0f * mm;
    const float page_step = page_h + page_gap;
    const float text_top_rel = (doc.meta.form_type > 0) ? (5.0f * mm) : (15.0f * mm);

    const int first_text_page = gost_title_pages(doc.meta);

    auto page_top_of = [&](int p) { return static_cast<float>(p) * page_step; };
    auto page_bottom_of = [&](int p) {
        const float stamp_h = (p == first_text_page) ? 40.0f * mm : 15.0f * mm;
        return page_top_of(p) + 292.0f * mm - stamp_h - 8.0f * mm;
    };

    auto shift_from_block = [&](size_t start_bi, size_t start_li, float dy) {
        if (dy <= 0.0f) return;
        for (size_t li = start_li; li < out.lines.size(); ++li) {
            out.lines[li].y += dy;
        }
        for (size_t bi = start_bi; bi < out.boxes.size(); ++bi) {
            out.boxes[bi].y += dy;
        }
        for (TableGrid& g : out.tables) {
            if (static_cast<size_t>(g.block) >= start_bi) {
                for (float& gy : g.ys) gy += dy;
            }
        }
    };

    int cur_page = first_text_page;
    float initial_text_y = page_top_of(first_text_page) + text_top_rel;

    // ГОСТ Р 2.105 п. 6.2: автоматический структурный элемент «Содержание».
    if (doc.meta.has_toc) {
        out.toc_title_y = initial_text_y;
        float ty = initial_text_y + 38.0f;
        for (size_t bi = 0; bi < doc.blocks.size(); ++bi) {
            const Block& b = doc.blocks[bi];
            if (b.type != BlockType::Heading || b.level > 2) continue;
            const std::string plain = block_plain_text(b);
            if (plain.empty() || plain == "Содержание" || plain == "СОДЕРЖАНИЕ") continue;
            TocEntry te;
            te.block = static_cast<int>(bi);
            te.level = std::clamp(b.level, 1, 2);
            te.label = out.block_nums[bi].heading_prefix + plain;
            while (!te.label.empty() && te.label.back() == '.') te.label.pop_back();
            if (ty + 24.0f > page_bottom_of(cur_page)) {
                ++cur_page;
                ty = page_top_of(cur_page) + text_top_rel;
            }
            te.y = ty;
            te.height = 24.0f;
            out.toc_entries.push_back(std::move(te));
            ty += 26.0f;
        }
        initial_text_y = ty + 20.0f;
        if (initial_text_y + 30.0f > page_bottom_of(cur_page)) {
            ++cur_page;
            initial_text_y = page_top_of(cur_page) + text_top_rel;
        }
    }

    // Сдвигаем начало текста на вычисленную стартовую позицию первого текстового листа.
    shift_from_block(0, 0, initial_text_y);
    for (size_t bi = 0; bi < out.boxes.size() && bi < doc.blocks.size(); ++bi) {
        const Block& block = doc.blocks[bi];
        const GostBlockNum& gn = out.block_nums[bi];
        BlockBox& box = out.boxes[bi];

        // Находим диапазон строк этого блока.
        size_t first_li = out.lines.size();
        size_t end_li = out.lines.size();
        for (size_t li = 0; li < out.lines.size(); ++li) {
            if (out.lines[li].block == static_cast<int>(bi)) {
                if (first_li == out.lines.size()) first_li = li;
                end_li = li + 1;
            }
        }

        // ГОСТ Р 2.105 п. 6.3.4: каждое приложение начинается с новой страницы,
        // а блок Rule («Разрыв листа») принудительно переводит следующий за ним блок на новый лист А4.
        const bool after_page_break = (bi > 0 && doc.blocks[bi - 1].type == BlockType::Rule);
        if ((after_page_break || (gn.starts_new_page && box.y > page_top_of(cur_page) + text_top_rel + 1.0f))) {
            ++cur_page;
            const float target_y = page_top_of(cur_page) + text_top_rel;
            const float dy = target_y - box.y;
            shift_from_block(bi, first_li, dy);
        }

        const bool atomic_block = (block.type == BlockType::Table
                                || block.type == BlockType::CodeBlock
                                || block.type == BlockType::Heading
                                || block.type == BlockType::Rule);

        if (atomic_block || first_li >= end_li) {
            while (box.y + box.h > page_bottom_of(cur_page)
                   && box.y > page_top_of(cur_page) + text_top_rel + 1.0f) {
                ++cur_page;
                const float target_y = page_top_of(cur_page) + text_top_rel;
                const float dy = target_y - box.y;
                shift_from_block(bi, first_li, dy);
            }
            while (box.y + box.h > page_bottom_of(cur_page)) {
                ++cur_page;
            }
        } else {
            // Обычный текстовый блок (абзац, список, цитата): может переноситься построчно.
            for (size_t li = first_li; li < end_li; ++li) {
                Line& line = out.lines[li];
                if (line.y + line.height > page_bottom_of(cur_page)) {
                    ++cur_page;
                    const float target_y = page_top_of(cur_page) + text_top_rel;
                    const float dy = target_y - line.y;
                    if (li == first_li) {
                        shift_from_block(bi, li, dy);
                    } else {
                        for (size_t k = li; k < out.lines.size(); ++k) out.lines[k].y += dy;
                        box.h += dy;
                        for (size_t b2 = bi + 1; b2 < out.boxes.size(); ++b2) out.boxes[b2].y += dy;
                        for (TableGrid& g : out.tables) {
                            if (static_cast<size_t>(g.block) > bi) {
                                for (float& gy : g.ys) gy += dy;
                            }
                        }
                    }
                }
            }
        }
    }

    // ГОСТ Р 2.105 п. 6.13: сноски внизу страницы не должны накладываться на
    // основной текст. Если они не помещаются под содержимым — переносим блок
    // (и всё последующее) на следующий лист, чтобы граница сносок совпадала
    // с разрывом страницы.
    for (int fn_pass = 0; fn_pass < 10; ++fn_pass) {
        std::vector<int> tmp_fn_counts(static_cast<size_t>(cur_page + 1), 0);
        for (const Line& line : out.lines) {
            if (line.block < 0 || static_cast<size_t>(line.block) >= doc.blocks.size()) continue;
            const int p_idx = std::clamp(static_cast<int>(line.y / page_step), 0, cur_page);
            const Block& blk = doc.blocks[static_cast<size_t>(line.block)];
            const std::vector<Inline>& inls = inlines_of(blk, line.cell);
            for (const Span& span : line.spans) {
                if (span.footnote <= 0) continue;
                if (span.inl < 0 || static_cast<size_t>(span.inl) >= inls.size()) continue;
                ++tmp_fn_counts[static_cast<size_t>(p_idx)];
            }
        }

        int overflow_page = -1;
        size_t overflow_bi = out.boxes.size();
        for (int p = 0; p <= cur_page; ++p) {
            const int count = tmp_fn_counts[static_cast<size_t>(p)];
            if (count == 0) continue;

            const float stamp_h = (p == first_text_page) ? 40.0f * mm : 15.0f * mm;
            const float bottom_edge = page_top_of(p) + 292.0f * mm - stamp_h - 3.0f * mm;
            const float row_step = 5.5f * mm;
            const float fn_top = bottom_edge - static_cast<float>(count) * row_step;

            float content_bottom = page_top_of(p) + text_top_rel;
            size_t content_bi = out.boxes.size();
            for (const Line& line : out.lines) {
                const int lp = std::clamp(static_cast<int>(line.y / page_step), 0, cur_page);
                if (lp != p) continue;
                if (line.y + line.height > content_bottom) {
                    content_bottom = line.y + line.height;
                    content_bi = static_cast<size_t>(line.block);
                }
            }

            const float gap = 8.0f * mm;
            if (content_bottom > fn_top - gap) {
                overflow_page = p;
                overflow_bi = content_bi;
                break;
            }
        }

        if (overflow_page < 0 || overflow_bi >= out.boxes.size()) break;

        // Не переносим блок, который уже начинается на самом верху этой страницы (иначе будет бесконечный перенос).
        if (out.boxes[overflow_bi].y <= page_top_of(overflow_page) + text_top_rel + 2.0f) break;

        const float target_y = page_top_of(overflow_page + 1) + text_top_rel;
        const float dy = target_y - out.boxes[overflow_bi].y;
        if (dy <= 0.0f) break;

        size_t first_li = out.lines.size();
        for (size_t li = 0; li < out.lines.size(); ++li) {
            if (out.lines[li].block == static_cast<int>(overflow_bi)) {
                first_li = li;
                break;
            }
        }
        shift_from_block(overflow_bi, first_li, dy);

        for (const Line& line : out.lines) {
            cur_page = std::max(cur_page, std::clamp(static_cast<int>(line.y / page_step), 0, 1000000));
        }
    }

    // Сбор сносок по страницам (п. 6.13.5 ГОСТ Р 2.105-2019: на каждой странице отдельная нумерация сносок).
    out.footnotes.clear();
    std::vector<int> page_fn_counts(static_cast<size_t>(cur_page + 1), 0);
    for (Line& line : out.lines) {
        if (line.block < 0 || static_cast<size_t>(line.block) >= doc.blocks.size()) continue;
        const Block& blk = doc.blocks[static_cast<size_t>(line.block)];
        const std::vector<Inline>& inls = inlines_of(blk, line.cell);
        const int p_idx = std::clamp(static_cast<int>(line.y / page_step), 0, cur_page);
        for (Span& span : line.spans) {
            if (span.footnote <= 0) continue;
            if (span.inl < 0 || static_cast<size_t>(span.inl) >= inls.size()) continue;
            const int fid = span.footnote;
            const int fn_num = ++page_fn_counts[static_cast<size_t>(p_idx)];
            span.footnote_id = fid;      // оригинальный ID из Document.footnotes
            span.footnote = fn_num;      // порядковый номер сноски на этой странице
            PageFootnote pf;
            pf.page = p_idx;
            pf.num = fn_num;
            pf.first_on_page = (fn_num == 1);
            for (const auto& f : doc.footnotes) {
                if (f.id == fid) { pf.text = f.text; break; }
            }
            if (pf.text.empty()) pf.text = "сноска";
            out.footnotes.push_back(std::move(pf));
        }
    }
    for (PageFootnote& pf : out.footnotes) {
        const int count_on_p = page_fn_counts[static_cast<size_t>(pf.page)];
        const float stamp_h = (pf.page == first_text_page) ? 40.0f * mm : 15.0f * mm;
        const float bottom_edge = page_top_of(pf.page) + 292.0f * mm - stamp_h - 3.0f * mm;
        const float row_step = 5.5f * mm;
        pf.y = (bottom_edge - static_cast<float>(count_on_p - pf.num + 1) * row_step) * page_scale;
    }

    // После пагинации определяем реальные номера листов для каждой строки «Содержания».
    for (TocEntry& te : out.toc_entries) {
        if (te.block >= 0 && static_cast<size_t>(te.block) < out.boxes.size()) {
            const float by = out.boxes[static_cast<size_t>(te.block)].y;
            te.page_num = std::max(1, static_cast<int>(by / page_step) + 1);
        }
    }
    if (out.toc_title_y >= 0.0f) {
        out.toc_title_y *= page_scale;
    }
    for (TocEntry& te : out.toc_entries) {
        te.y *= page_scale;
        te.height *= page_scale;
    }

    // Если включён Лист регистрации изменений (ЛРИ по п. 5.1.8 ГОСТ Р 2.105 и ГОСТ 2.503),
    // он помещается последним листом документа и включается в общее количество листов.
    if (doc.meta.has_lri) {
        ++cur_page;
    }

    out.page_tops.clear();
    for (int p = 0; p <= cur_page; ++p) {
        out.page_tops.push_back(page_top_of(p) * page_scale);
    }
    out.mm_px = mm * page_scale;
    out.content_height = (page_top_of(cur_page) + page_h + 10.0f * mm) * page_scale;

    for (Line& line : out.lines) {
        line.y *= page_scale;
        line.height *= page_scale;
        line.baseline *= page_scale;
        line.x0 *= page_scale;
        for (Span& span : line.spans) {
            span.x *= page_scale;
            span.width *= page_scale;
        }
    }
    for (BlockBox& box : out.boxes) {
        box.x *= page_scale;
        box.y *= page_scale;
        box.w *= page_scale;
        box.h *= page_scale;
    }
    for (TableGrid& g : out.tables) {
        for (float& gx : g.xs) gx *= page_scale;
        for (float& gy : g.ys) gy *= page_scale;
    }

    fonts.set_zoom(saved_zoom);
}

bool operator==(const Pos& a, const Pos& b) {
    return a.block == b.block && a.cell == b.cell && a.inl == b.inl && a.offset == b.offset;
}

bool operator<(const Pos& a, const Pos& b) {
    if (a.block != b.block) return a.block < b.block;
    if (a.cell != b.cell) return a.cell < b.cell;
    if (a.inl != b.inl) return a.inl < b.inl;
    return a.offset < b.offset;
}

float file_icon_width(float zoom) {
    return 22.0f * zoom;
}

float math_size(const Block& block, float zoom) {
    return size_for(block) * zoom;
}

MathReveal math_reveal_at(const Document& doc, const Pos& caret) {
    MathReveal none;
    if (caret.block < 0 || caret.block >= static_cast<int>(doc.blocks.size())) return none;
    const Block& block = doc.blocks[caret.block];
    if (block.type == BlockType::CodeBlock || block.type == BlockType::Rule) return none;
    const std::vector<Inline>& inls = inlines_of(block, caret.cell);
    for (int ii = 0; ii < static_cast<int>(inls.size()); ++ii) {
        const Inline& inl = inls[ii];
        if (inl.style & StyleCode) continue;
        if (inl.footnote_id > 0) continue;
        const std::string& text = inl.text;
        for (int pos = 0; pos < static_cast<int>(text.size());) {
            if (text[pos] == '$' && (pos == 0 || text[pos - 1] == ' ')) {
                const int close = find_math_close(text, pos);
                if (close > 0) {
                    if (caret.inl == ii && caret.offset > pos && caret.offset <= close) {
                        return MathReveal{caret.block, caret.cell, ii, pos, close + 1};
                    }
                    pos = close + 1;
                    continue;
                }
            }
            if (text[pos] == '^' && pos + 1 < static_cast<int>(text.size()) && text[pos + 1] == '[') {
                size_t closep = text.find(']', pos + 2);
                if (closep != std::string::npos && text.find('\n', pos + 2) > closep) {
                    if (caret.inl == ii && caret.offset > pos && caret.offset <= static_cast<int>(closep) + 1) {
                        return MathReveal{caret.block, caret.cell, ii, pos, static_cast<int>(closep) + 1};
                    }
                    pos = static_cast<int>(closep) + 1;
                    continue;
                }
            }
            pos = next_char(text, pos);
        }
    }
    return none;
}

Pos hit_test(const Document& doc, const Layout& layout, FontBook& fonts, float x, float y) {
    if (layout.lines.empty()) return {};

    const Line* line = &layout.lines.back();
    for (const Line& l : layout.lines) {
        if (y <= l.y + l.height) {
            line = &l;
            break;
        }
    }

    // Inside a table, first find the cell under the pointer, then its line.
    if (const TableGrid* grid = grid_for(layout, line->block)) {
        const int cell = edge_cell(grid->ys, y) * grid->cols + edge_cell(grid->xs, x);
        const Line* in_cell = nullptr;
        for (const Line& l : layout.lines) {
            if (l.block != line->block || l.cell != cell) continue;
            in_cell = &l;
            if (y <= l.y + l.height) break;
        }
        if (in_cell) line = in_cell;
    }

    if (line->spans.empty()) return Pos{line->block, 0, 0, line->cell};

    const Block& block = doc.blocks[line->block];
    const std::vector<Inline>& inls = inlines_of(block, line->cell);

    const Span& first = line->spans.front();
    if (x <= first.x) return Pos{line->block, first.inl, first.byte_begin, line->cell};
    const Span& last = line->spans.back();
    if (x >= last.x + last.width) return Pos{line->block, last.inl, last.byte_end, line->cell};

    for (const Span& span : line->spans) {
        if (x >= span.x + span.width) continue;
        if (span.math || span.footnote > 0) {
            // A formula or footnote is one unit: the caret goes to whichever side is closer.
            const bool left = x < span.x + span.width / 2.0f;
            return Pos{line->block, span.inl, left ? span.byte_begin : span.byte_end, line->cell};
        }
        const Inline& inl = inls[span.inl];
        const float page_scale = layout.mm_px / 4.0f;
        const float saved_zoom = fonts.zoom();
        fonts.set_base_pt(doc.meta.font_pt);
        fonts.set_zoom(1.0f);
        TTF_Font* font = font_for(fonts, block, inl.style | line->extra_style);

        // Walk UTF-8 boundaries and pick the closest one by midpoint.
        int prev = span.byte_begin;
        float prev_w = 0.0f;
        while (prev < span.byte_end) {
            const int next = next_char(inl.text, prev);
            const float w = measure(font, inl.text, span.byte_begin, next) * page_scale;
            if (x < span.x + (prev_w + w) / 2.0f) {
                fonts.set_zoom(saved_zoom);
                return Pos{line->block, span.inl, prev, line->cell};
            }
            prev = next;
            prev_w = w;
        }
        fonts.set_zoom(saved_zoom);
        return Pos{line->block, span.inl, span.byte_end, line->cell};
    }
    return Pos{line->block, last.inl, last.byte_end, line->cell};
}

int locate_caret(const Document& doc, const Layout& layout, FontBook& fonts,
                 Pos pos, float* x_out) {
    if (pos.block < 0 || pos.block >= static_cast<int>(doc.blocks.size())) return -1;
    const Block& block = doc.blocks[pos.block];
    const std::vector<Inline>& inls = inlines_of(block, pos.cell);
    auto mine = [&](const Line& l) {
        return l.block == pos.block && l.cell == pos.cell;
    };

    int first_line = -1, last_line = -1;
    for (size_t i = 0; i < layout.lines.size(); ++i) {
        const Line& line = layout.lines[i];
        if (!mine(line)) continue;
        if (first_line < 0) first_line = static_cast<int>(i);
        last_line = static_cast<int>(i);

        for (const Span& span : line.spans) {
            if (span.inl != pos.inl) continue;
            if (pos.offset >= span.byte_begin && pos.offset < span.byte_end) {
                if (span.math || span.footnote > 0) {
                    *x_out = span.x;
                    return static_cast<int>(i);
                }
                const Inline& inl = inls[span.inl];
                const float page_scale = layout.mm_px / 4.0f;
                const float saved_zoom = fonts.zoom();
                fonts.set_base_pt(doc.meta.font_pt);
                fonts.set_zoom(1.0f);
                TTF_Font* font = font_for(fonts, block, inl.style | line.extra_style);
                *x_out = span.x + measure(font, inl.text, span.byte_begin, pos.offset) * page_scale;
                fonts.set_zoom(saved_zoom);
                return static_cast<int>(i);
            }
        }
    }
    if (first_line < 0) return -1;

    // Caret exactly at a span end (e.g. end of an inline or of the block).
    for (int i = first_line; i <= last_line; ++i) {
        const Line& line = layout.lines[i];
        if (!mine(line)) continue;
        for (const Span& span : line.spans) {
            if (span.inl == pos.inl && pos.offset == span.byte_end) {
                *x_out = span.x + span.width;
                return i;
            }
        }
    }

    // Caret in an empty inline: place it at the start of the next content.
    for (int i = first_line; i <= last_line; ++i) {
        const Line& line = layout.lines[i];
        if (!mine(line)) continue;
        for (const Span& span : line.spans) {
            if (span.inl >= pos.inl) {
                *x_out = span.x;
                return i;
            }
        }
    }

    const Line& line = layout.lines[last_line];
    *x_out = line.spans.empty() ? line.x0
        : line.spans.back().x + line.spans.back().width;
    return last_line;
}

} // namespace md
