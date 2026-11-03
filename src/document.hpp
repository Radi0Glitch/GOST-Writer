#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <vector>

#include "math_model.hpp"

namespace md {

enum Style : std::uint8_t {
    StyleBold = 1 << 0,
    StyleItalic = 1 << 1,
    StyleCode = 1 << 2,
    StyleLink = 1 << 3,
    StyleStrike = 1 << 4,
    StyleImage = 1 << 5, // with StyleLink: an image, its href is the file or address
};

enum class BlockType {
    Paragraph,
    Heading,
    Quote,
    CodeBlock,
    ListItem,
    Rule,  // horizontal rule, no text
    Table, // GFM table, text lives in Block::cells
};

struct Inline {
    std::string text; // UTF-8, no markdown markers
    std::uint8_t style = 0;
    std::string href; // link target, set when style has StyleLink
    int footnote_id = 0; // >0 — маркер сноски по ГОСТ Р 2.105 п. 6.13 (редактируется в отдельной панели)

    Inline() = default;
    Inline(std::string t, std::uint8_t s = 0, std::string h = {})
        : text(std::move(t)), style(s), href(std::move(h)) {}
};

// A link to the web or by mail, as opposed to a file on disk.
inline bool is_web_link(const std::string& href) {
    return href.find("://") != std::string::npos || href.rfind("mailto:", 0) == 0;
}

// Links to files and every image are drawn as file thumbnails instead of text.
inline bool is_file_link(const Inline& inl) {
    if (inl.style & StyleImage) return true;
    return (inl.style & StyleLink) && !inl.href.empty() && !is_web_link(inl.href);
}

enum class TableAlign : std::uint8_t { None, Left, Center, Right };

struct Block {
    Block() = default;
    Block(BlockType t, int lvl, std::vector<Inline> in)
        : type(t), level(lvl), inlines(std::move(in)) {}

    BlockType type = BlockType::Paragraph;
    int level = 0; // heading 1..6, unused otherwise
    std::vector<Inline> inlines;
    int depth = 0;        // list nesting, ListItem only
    bool ordered = false; // numbered list item, ListItem only
    std::string lang;     // fenced code block language (e.g. "mermaid", "latex", "math")

    // Table only. `inlines` holds one empty placeholder so that every block has
    // text. Cells are row-major; row 0 is the header.
    std::vector<std::vector<Inline>> cells;
    std::vector<TableAlign> aligns; // one per column
};

inline int table_cols(const Block& b) { return static_cast<int>(b.aligns.size()); }

inline int table_rows(const Block& b) {
    const int cols = table_cols(b);
    return cols > 0 ? static_cast<int>(b.cells.size()) / cols : 0;
}

// The text container a position points into: the block's inlines, or one table cell.
inline std::vector<Inline>& inlines_of(Block& b, int cell) {
    return cell >= 0 ? b.cells[static_cast<size_t>(cell)] : b.inlines;
}

inline const std::vector<Inline>& inlines_of(const Block& b, int cell) {
    return cell >= 0 ? b.cells[static_cast<size_t>(cell)] : b.inlines;
}

// Одна запись подписи на титульном листе или листе утверждения (п. 8.9 ГОСТ Р 2.105-2019).
struct TitleSign {
    std::string role;
    std::string name;
    std::string date;

    TitleSign() = default;
    TitleSign(std::string r, std::string n, std::string d = "")
        : role(std::move(r)), name(std::move(n)), date(std::move(d)) {}
};

inline bool operator==(const TitleSign& a, const TitleSign& b) {
    return a.role == b.role && a.name == b.name && a.date == b.date;
}

// Реквизиты основной надписи по ГОСТ 2.104 (формы 2 и 2а) и параметры оформления по ГОСТ Р 2.105-2019.
struct GostMeta {
    std::string code = "АБВГ.123456.001 ПЗ";     // графа 2: обозначение документа
    std::string name = "Наименование документа"; // графа 1: наименование изделия и документа
    std::string org  = "Организация";            // графа 9: организация, выпускающая документ
    std::string dev     = "";                    // графа 11: Разраб.
    std::string check   = "";                    // графа 11: Пров.
    std::string tcheck  = "";                    // графа 11: Т.контр.
    std::string ncheck  = "";                    // графа 11: Н.контр.
    std::string approve = "";                    // графа 11: Утв.
    int font_pt          = 14;                   // п. 5.1.1: 11, 12, 13 или 14 пт
    int line_spacing_idx = 3;                    // п. 5.1.2: 0=1.0, 1=1.15, 2=1.25, 3=1.5, 4=1.75, 5=2.0
    bool auto_num_headings = true;               // п. 6.5: автонумерация разделов (1, 1.1, 1.1.1)
    bool section_num_media = false;              // п. 6.8.2, 6.9.3, 6.10.4: false = сквозная (1), true = по разделам (1.1)
    bool has_title_page    = false;              // раздел 8: включать ли титульный лист
    bool title_by_lu       = false;              // п. 8.11, Прил. Г: титульный лист при наличии ЛУ («Утвержден ...-ЛУ»)
    bool has_toc           = false;              // п. 6.2: включать ли автоматическое «Содержание»
    bool has_lri           = false;              // п. 5.1.8, ГОСТ 2.503: включать ли Лист регистрации изменений (ЛРИ) в конце
    int form_type          = 0;                  // 0=Форма 9/9а (текст), 1=Форма 1/1а (Спецификация), 2=Форма 4/4а (ВД), 3=Форма 8/8а (ТП/ЭП/ПТ)

    // Поля титульного листа (раздел 8 ГОСТ Р 2.105-2019, рис. 23, Приложения Б и В):
    std::string title_org          = "Наименование ведомства или организации"; // поле 1
    std::string title_okpd         = "";                                       // поле 2: код по ОКПД 2
    std::string title_agree_role   = "Руководитель подразделения";             // поле 3 слева (первое)
    std::string title_agree_name   = "И.О. Фамилия";
    std::string title_approve_role = "Руководитель организации";               // поле 3 справа (первое)
    std::string title_approve_name = "И.О. Фамилия";
    std::string title_product      = "НАИМЕНОВАНИЕ ИЗДЕЛИЯ";                   // поле 4 (прописными)
    std::string title_doc_kind     = "Пояснительная записка";                  // поле 4 (вид документа)
    std::string title_year         = "2026";                                   // год внизу листа

    // Списки множественных подписей на титульном листе (п. 8.9, рис. В.1, В.2):
    std::vector<TitleSign> title_agrees = {
        TitleSign{"Руководитель подразделения", "И.О. Фамилия", "01.09.2026"},
    };
    std::vector<TitleSign> title_approves = {
        TitleSign{"Руководитель организации", "И.О. Фамилия", "01.09.2026"},
    };
    std::vector<TitleSign> title_devs = {
        TitleSign{"Главный инженер", "И.О. Фамилия", "01.09.2026"},
        TitleSign{"Руководитель разработки", "И.О. Фамилия", "01.09.2026"},
    };

    bool modified_meta  = false;                 // сохранять ли блок реквизитов в .md
};

inline float gost_line_spacing(int idx) {
    static constexpr float kSpacings[6] = {1.0f, 1.15f, 1.25f, 1.5f, 1.75f, 2.0f};
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    return kSpacings[idx];
}

inline bool operator==(const GostMeta& a, const GostMeta& b) {
    return a.code == b.code && a.name == b.name && a.org == b.org
        && a.dev == b.dev && a.check == b.check && a.tcheck == b.tcheck
        && a.ncheck == b.ncheck && a.approve == b.approve
        && a.font_pt == b.font_pt && a.line_spacing_idx == b.line_spacing_idx
        && a.auto_num_headings == b.auto_num_headings
        && a.section_num_media == b.section_num_media
        && a.has_title_page == b.has_title_page
        && a.title_by_lu == b.title_by_lu
        && a.has_toc == b.has_toc
        && a.has_lri == b.has_lri
        && a.form_type == b.form_type
        && a.title_org == b.title_org && a.title_okpd == b.title_okpd
        && a.title_agree_role == b.title_agree_role && a.title_agree_name == b.title_agree_name
        && a.title_approve_role == b.title_approve_role && a.title_approve_name == b.title_approve_name
        && a.title_product == b.title_product && a.title_doc_kind == b.title_doc_kind
        && a.title_year == b.title_year
        && a.title_agrees == b.title_agrees
        && a.title_approves == b.title_approves
        && a.title_devs == b.title_devs;
}

struct Footnote {
    int id = 0;
    std::string text;
};

inline bool operator==(const Footnote& a, const Footnote& b) {
    return a.id == b.id && a.text == b.text;
}

struct Document {
    std::vector<Block> blocks;
    GostMeta meta;
    std::vector<Footnote> footnotes;
    int next_footnote_id = 1;
};

inline bool operator==(const Inline& a, const Inline& b) {
    return a.text == b.text && a.style == b.style && a.href == b.href && a.footnote_id == b.footnote_id;
}

inline bool operator==(const Block& a, const Block& b) {
    return a.type == b.type && a.level == b.level && a.depth == b.depth
        && a.ordered == b.ordered && a.lang == b.lang && a.inlines == b.inlines
        && a.cells == b.cells && a.aligns == b.aligns;
}

inline bool operator==(const Document& a, const Document& b) {
    return a.blocks == b.blocks && a.meta == b.meta && a.footnotes == b.footnotes && a.next_footnote_id == b.next_footnote_id;
}

// Read-only showcase document for stage 1.
Document make_demo_document();

} // namespace md
