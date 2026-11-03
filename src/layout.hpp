#pragma once

#include <string>
#include <vector>

#include "document.hpp"
#include "fontbook.hpp"

namespace md {

// A piece of one Inline that fits on a single line. Byte offsets refer to
// Inline::text of the block the line belongs to.
struct Span {
    int inl = 0;
    int byte_begin = 0;
    int byte_end = 0;
    float x = 0;
    float width = 0;
    bool math = false;    // an inline formula "$...$", drawn as a formula
    bool icon = false;    // the file thumbnail in front of a file link, no text
    int footnote = 0;     // >0: порядковый номер сноски на странице (после сбора), 0 до сбора
    int footnote_id = 0;  // >0: оригинальный footnote_id из Document.footnotes
};

struct Line {
    int block = 0;
    int cell = -1;               // table cell index, -1 outside tables
    std::uint8_t extra_style = 0; // added to every span's style, e.g. bold headers
    float y = 0;        // top, document coordinates
    float height = 0;
    float baseline = 0; // distance from y to the baseline
    float x0 = 0;       // text indent, also the caret x for empty lines
    std::vector<Span> spans;
};

struct BlockBox {
    float x = 0, y = 0, w = 0, h = 0; // background/border geometry
};

// Grid of one table: column edges (x, relative to the content column) and row
// edges (y, document coordinates). Edges are n + 1 values for n columns or rows.
struct TableGrid {
    int block = 0;
    int cols = 0;
    std::vector<float> xs;
    std::vector<float> ys;
};

// Вычисленные по ГОСТ Р 2.105-2019 номера и подписи для одного блока документа.
struct GostBlockNum {
    std::string heading_prefix; // например "1 ", "1.1 ", "1.1.1 ", "А.1 " (пусто, если не нумеруется или номер уже введён)
    std::string table_caption;  // например "Таблица 1", "Таблица 1.1 — Параметры", "Таблица А.1"
    std::string figure_caption; // например "Рисунок 1 — Схема", "Рисунок 1.1", "Рисунок А.1"
    std::string formula_number; // например "(1)", "(1.1)", "(А.1)"
    std::string note_prefix;    // для примечаний (п. 6.12): "Примечание — " или "1 ", "2 "
    bool note_group_header = false; // true у первого из нескольких подряд примечаний (над ним пишется "Примечания")
    bool starts_new_page = false;   // true для разделов/приложений, начинающихся с нового листа (п. 6.3.4)
    bool centered_heading = false;  // true для "Содержание", "Приложение А" (п. 6.2.5, 6.3.4)
};

inline int gost_title_pages(const GostMeta& m) {
    if (!m.has_title_page) return 0;
    if (m.title_by_lu) return 1;
    if (m.title_agrees.size() > 3 || m.title_approves.size() > 2 || m.title_devs.size() > 2) {
        return 2;
    }
    return 1;
}

std::vector<GostBlockNum> compute_gost_numbering(const Document& doc);

// Строка автоматического элемента «Содержание» по ГОСТ Р 2.105-2019 (п. 6.2).
struct TocEntry {
    int block = -1;
    int level = 1;       // 1 = раздел/приложение/структурный элемент, 2 = подраздел (с отступом 2 знака по п. 6.2.3)
    std::string label;   // номер и заголовок
    int page_num = 1;    // реальный номер листа ТД
    float y = 0.0f;      // y строки содержания в координатах документа
    float height = 0.0f; // высота строки содержания
};

// Сноска внизу страницы по ГОСТ Р 2.105-2019 (п. 6.13).
struct PageFootnote {
    int page = 0;        // индекс листа А4 (0-based)
    int num = 1;         // порядковый номер сноски на данной странице (1, 2, 3...)
    std::string text;    // текст пояснения сноски
    float y = 0.0f;      // y строки сноски в координатах документа
    bool first_on_page = false; // true у первой сноски на листе (над ней рисуется короткая линия слева)
};

struct Layout {
    std::vector<Line> lines;
    std::vector<BlockBox> boxes;           // one per block
    std::vector<std::string> markers;      // one per block, list bullet/number or empty
    std::vector<GostBlockNum> block_nums;  // one per block, GOST R 2.105 numbering
    std::vector<TableGrid> tables;
    std::vector<float> page_tops;          // y coordinates of A4 sheets (210x297 mm)
    float toc_title_y = -1.0f;             // y заголовка «Содержание» (< 0 если выключено)
    std::vector<TocEntry> toc_entries;     // строки автоматического «Содержания»
    std::vector<PageFootnote> footnotes;   // сноски внизу страниц по п. 6.13
    float mm_px = 4.0f;                    // screen pixels per 1 mm of the A4 sheet
    float content_height = 0;
};

// Shared by layout and render so both always agree on metrics.
TTF_Font* font_for(FontBook& fonts, const Block& block, std::uint8_t style);

// An inline formula whose source is shown instead of the rendered formula, because
// the caret is inside it. Offsets are byte offsets in the inline text.
struct MathReveal {
    int block = -1;
    int cell = -1;
    int inl = -1;
    int begin = -1; // the opening $
    int end = -1;   // just past the closing $
    bool operator==(const MathReveal& o) const {
        return block == o.block && cell == o.cell && inl == o.inl && begin == o.begin && end == o.end;
    }
};

void layout_document(const Document& doc, FontBook& fonts, float width, Layout& out,
                     const MathReveal& reveal = {});

// Font size of the text in a block, used for inline formulas too.
float math_size(const Block& block, float zoom);

// Width of the file thumbnail in front of a file link, text not included.
float file_icon_width(float zoom);

// A caret position inside the document model. Offsets are UTF-8 byte offsets.
// `cell` is set only when the block is a table: it selects the cell.
struct Pos {
    int block = 0;
    int inl = 0;
    int offset = 0;
    int cell = -1;
};

bool operator==(const Pos& a, const Pos& b);
bool operator<(const Pos& a, const Pos& b);

// Maps a point in document coordinates to a model position.
Pos hit_test(const Document& doc, const Layout& layout, FontBook& fonts, float x, float y);

// The formula under the caret, if the caret is strictly inside one.
MathReveal math_reveal_at(const Document& doc, const Pos& caret);

// Finds the line and x coordinate of a caret. Returns the line index or -1.
int locate_caret(const Document& doc, const Layout& layout, FontBook& fonts,
                 Pos pos, float* x_out);

} // namespace md
