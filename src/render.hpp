#pragma once

#include "document.hpp"
#include "fontbook.hpp"
#include "layout.hpp"
#include "ui.hpp"

namespace md {

// Редактируемые графы основной надписи ГОСТ 2.104 (формы 2 и 2а) и полей титульного листа (раздел 8 ГОСТ Р 2.105).
enum class StampField {
    None = -1,
    Code = 0,             // графа 2: Обозначение документа
    Name = 1,             // графа 1: Наименование документа
    Org = 2,              // графа 9: Организация
    Dev = 3,              // графа 11: Разраб.
    Check = 4,            // графа 11: Пров.
    TCheck = 5,           // графа 11: Т.контр.
    NCheck = 6,           // графа 11: Н.контр.
    Approve = 7,          // графа 11: Утв.
    TitleOrg = 8,         // Титульный лист, поле 1: Ведомство / организация
    TitleOkpd = 9,        // Титульный лист, поле 2: Код ОКПД 2
    TitleAgreeRole = 10,  // Титульный лист, поле 3 слева: СОГЛАСОВАНО (должность)
    TitleAgreeName = 11,  // Титульный лист, поле 3 слева: СОГЛАСОВАНО (И.О. Фамилия)
    TitleApproveRole = 12,// Титульный лист, поле 3 справа: УТВЕРЖДАЮ (должность)
    TitleApproveName = 13,// Титульный лист, поле 3 справа: УТВЕРЖДАЮ (И.О. Фамилия)
    TitleProduct = 14,    // Титульный лист, поле 4: НАИМЕНОВАНИЕ ИЗДЕЛИЯ
    TitleDocKind = 15,    // Титульный лист, поле 4: Вид документа
    TitleYear = 16,       // Титульный лист, внизу: Год
};

struct StampSlot {
    StampField field = StampField::None;
    std::string* target = nullptr; // прямое редактируемое поле (для динамических списков подписей)
    int action = 0;                // >0 для кнопок управления подписями на титульном листе
    ui::Rect rect{};
};

// What the editor overlays on top of the document.
struct EditView {
    bool selecting = false;
    Pos sel_start, sel_end; // ordered
    bool caret_visible = false;
    Pos caret;
    StampField active_stamp_field = StampField::None;
    const std::string* active_target = nullptr;
    std::vector<StampSlot>* stamp_slots = nullptr;
};

// Draws the laid out document into `viewport` (window coordinates), shifted by
// scroll_y. Sets and resets the renderer clip rect.
void render_document(ui::Context& ctx, const Document& doc, const Layout& layout,
                     FontBook& fonts, ui::Rect viewport, float scroll_y,
                     const EditView* edit);

} // namespace md
