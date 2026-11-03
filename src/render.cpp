#include "render.hpp"

#include <algorithm>
#include <cmath>

#include "chart_draw.hpp"
#include "chart_model.hpp"
#include "diag_draw.hpp"
#include "math_draw.hpp"
#include "math_model.hpp"

namespace md {

namespace {

const SDL_Color kCodeBg{44, 39, 33, 255};
const SDL_Color kCodeText{226, 192, 141, 255};
const SDL_Color kQuoteBar{106, 96, 84, 255};
const SDL_Color kSelection{140, 70, 50, 110};
const SDL_Color kTableHead{50, 44, 38, 255};

float span_prefix_width(TTF_Font* font, const std::string& text, int begin, int end) {
    // NB: TTF_GetStringSize treats length 0 as "the whole C string".
    if (end <= begin) return 0.0f;
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str() + begin, static_cast<size_t>(end - begin), &w, &h);
    return static_cast<float>(w) + 2.0f;
}

// A small page with a folded corner and the file type, drawn in front of a file link.
void draw_file_icon(ui::Context& ctx, FontBook& fonts, const std::string& href, float x, float y) {
    const float zoom = fonts.zoom();
    const float w = 16.0f * zoom;
    const float h = 20.0f * zoom;
    const ui::Rect page{x, y, w, h};
    ui::fill_rounded_rect(ctx, page, 2.0f * zoom, ctx.palette.panel_bg);
    ui::stroke_rect(ctx, page, ctx.palette.panel_border);
    ui::fill_rect(ctx, {x + w - 5.0f * zoom, y, 5.0f * zoom, 5.0f * zoom}, ctx.palette.panel_border);

    std::string ext = "FILE";
    const size_t dot = href.find_last_of('.');
    const size_t slash = href.find_last_of("/\\");
    const bool has_ext = dot != std::string::npos && dot + 1 < href.size() && href.size() - dot - 1 <= 4
        && (slash == std::string::npos || dot > slash);
    if (has_ext) {
        ext = href.substr(dot + 1);
        for (char& c : ext) {
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        }
    }
    TTF_Font* font = fonts.get(md::Family::Sans, true, false, 6.0f);
    const float tw = ui::text_width(font, ext);
    ui::draw_text(ctx, font, ext, x + (w - tw) / 2.0f,
                  y + h - ui::text_height(font) - 2.0f * zoom, ctx.palette.accent);
}

void draw_span_text(ui::Context& ctx, TTF_Font* font, const std::string& text,
                    int begin, int end, float x, float y, SDL_Color color) {
    if (end <= begin) return;
    const std::string piece = text.substr(begin, end - begin);
    SDL_Surface* surface = TTF_RenderText_Blended(font, piece.c_str(), piece.size(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(ctx.renderer, surface);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        const SDL_FRect dst{std::round(x), std::round(y), static_cast<float>(surface->w),
                            static_cast<float>(surface->h)};
        SDL_RenderTexture(ctx.renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

// Soft drop shadow: translucent rounded rectangles growing out of the card and
// shifted a little downwards, so the card reads as raised above the page.
void card_shadow(ui::Context& ctx, ui::Rect r, float radius, Uint8 strength) {
    // Symmetric soft shadow: nested rounded rectangles that grow out of the card on
    // every side. Layers overlap, so the shade is strongest next to the edge and
    // fades outwards; the whole shape is nudged down by a couple of pixels.
    constexpr int kLayers = 14;
    const Uint8 alpha = static_cast<Uint8>(std::max(1, strength / 2));
    for (int i = 0; i < kLayers; ++i) {
        const float grow = static_cast<float>(i) * 1.2f;
        const ui::Rect s{r.x - grow, r.y + 2.0f - grow, r.w + grow * 2.0f, r.h + grow * 2.0f};
        ui::fill_rounded_rect(ctx, s, radius + grow, SDL_Color{0, 0, 0, alpha});
    }
}

void draw_rotated_text(ui::Context& ctx, TTF_Font* font, const std::string& text,
                       ui::Rect cell_rect, SDL_Color color) {
    if (text.empty() || !font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(ctx.renderer, surface);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        const float tw = static_cast<float>(surface->w);
        const float th = static_cast<float>(surface->h);
        const SDL_FRect dst{
            std::round(cell_rect.x + (cell_rect.w - tw) * 0.5f),
            std::round(cell_rect.y + (cell_rect.h - th) * 0.5f),
            tw, th
        };
        const SDL_FPoint center{tw * 0.5f, th * 0.5f};
        SDL_RenderTextureRotated(ctx.renderer, texture, nullptr, &dst, -90.0, &center, SDL_FLIP_NONE);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

} // namespace

void render_document(ui::Context& ctx, const Document& doc, const Layout& layout,
                     FontBook& fonts, ui::Rect viewport, float scroll_y,
                     const EditView* edit) {
    const bool light = ui::is_light_palette(ctx.palette);
    const SDL_Color code_bg = light ? SDL_Color{236, 230, 216, 255} : kCodeBg;
    const SDL_Color code_text = light ? SDL_Color{160, 60, 34, 255} : kCodeText;
    const SDL_Color selection_col = light ? SDL_Color{192, 67, 38, 60} : kSelection;

    const float mm = std::max(1.0f, layout.mm_px);
    const int clip_pad_l = static_cast<int>(28.0f * mm) + 24;
    const int clip_pad_r = static_cast<int>(14.0f * mm) + 24;
    const SDL_Rect clip{static_cast<int>(viewport.x) - clip_pad_l, static_cast<int>(viewport.y),
                        static_cast<int>(viewport.w) + clip_pad_l + clip_pad_r,
                        static_cast<int>(viewport.h)};
    SDL_SetRenderClipRect(ctx.renderer, &clip);

    const float origin_x = viewport.x;
    const float origin_y = viewport.y - scroll_y;

    if (edit && edit->stamp_slots) {
        edit->stamp_slots->clear();
    }

    // Отрисовка листов А4 (210x297 мм), рамки (185x287 мм), бокового штампа и основной надписи (Форма 2 / 2а).
    const SDL_Color paper_bg = light ? SDL_Color{255, 255, 255, 255} : SDL_Color{32, 29, 25, 255};
    const SDL_Color paper_border = light ? SDL_Color{205, 198, 188, 255} : SDL_Color{62, 56, 48, 255};
    const SDL_Color frame_line = light ? SDL_Color{30, 28, 25, 255} : SDL_Color{210, 202, 190, 255};
    const SDL_Color thin_line = light ? SDL_Color{110, 104, 96, 255} : SDL_Color{120, 112, 102, 255};
    const SDL_Color field_hover = light ? SDL_Color{192, 67, 38, 28} : SDL_Color{226, 120, 80, 40};
    const SDL_Color field_active = light ? SDL_Color{192, 67, 38, 48} : SDL_Color{226, 120, 80, 68};

    TTF_Font* f_lab = fonts.get(Family::Serif, false, true, 9.5f);
    TTF_Font* f_val_sm = fonts.get(Family::Serif, false, false, 10.5f);
    TTF_Font* f_val_md = fonts.get(Family::Serif, false, false, 13.0f);
    TTF_Font* f_val_lg = fonts.get(Family::Serif, false, false, 15.0f);

    auto draw_line_h = [&](float x, float y, float w, bool thick) {
        ui::fill_rect(ctx, {std::round(x), std::round(y), std::round(w), thick ? 2.0f : 1.0f},
                      thick ? frame_line : thin_line);
    };
    auto draw_line_v = [&](float x, float y, float h, bool thick) {
        ui::fill_rect(ctx, {std::round(x), std::round(y), thick ? 2.0f : 1.0f, std::round(h)},
                      thick ? frame_line : thin_line);
    };
    auto stroke_box_thick = [&](float x, float y, float w, float h) {
        draw_line_h(x, y, w, true);
        draw_line_h(x, y + h - 1.0f, w, true);
        draw_line_v(x, y, h, true);
        draw_line_v(x + w - 1.0f, y, h, true);
    };
    auto draw_centered = [&](TTF_Font* font, const std::string& txt, ui::Rect r, SDL_Color col) {
        if (txt.empty() || !font) return;
        const float tw = ui::text_width(font, txt);
        const float th = ui::text_height(font);
        ui::draw_text(ctx, font, txt, r.x + (r.w - tw) * 0.5f, r.y + (r.h - th) * 0.5f, col);
    };
    auto draw_left = [&](TTF_Font* font, const std::string& txt, ui::Rect r, SDL_Color col) {
        if (txt.empty() || !font) return;
        const float th = ui::text_height(font);
        ui::draw_text(ctx, font, txt, r.x + 1.0f * mm, r.y + (r.h - th) * 0.5f, col);
    };
    auto draw_editable_field = [&](StampField field, ui::Rect r, TTF_Font* font,
                                   const std::string& val_txt, bool centered, const char* placeholder = "",
                                   std::string* target_ptr = nullptr) {
        if (edit && edit->stamp_slots) {
            edit->stamp_slots->push_back(StampSlot{field, target_ptr, 0, r});
        }
        const bool is_active = edit && ((field != StampField::None && edit->active_stamp_field == field)
                                     || (target_ptr != nullptr && edit->active_target == target_ptr));
        const bool is_hover = ui::contains(r, ctx.input.mouse_x, ctx.input.mouse_y);
        if (is_active) {
            ui::fill_rect(ctx, {r.x + 1.0f, r.y + 1.0f, r.w - 2.0f, r.h - 2.0f}, field_active);
            ui::stroke_rect(ctx, {r.x + 1.0f, r.y + 1.0f, r.w - 2.0f, r.h - 2.0f}, ctx.palette.accent);
        } else if (is_hover) {
            ui::fill_rect(ctx, {r.x + 1.0f, r.y + 1.0f, r.w - 2.0f, r.h - 2.0f}, field_hover);
        }
        const std::string& shown = (!val_txt.empty() || is_active) ? val_txt : std::string(placeholder);
        const SDL_Color col = (!val_txt.empty() || is_active) ? ctx.palette.text : ctx.palette.text_dim;
        const float tw = shown.empty() ? 0.0f : ui::text_width(font, shown);
        const float th = ui::text_height(font);
        const float tx = centered ? (r.x + (r.w - tw) * 0.5f) : (r.x + 1.0f * mm);
        const float ty = r.y + (r.h - th) * 0.5f;
        if (!shown.empty()) {
            ui::draw_text(ctx, font, shown, tx, ty, col);
        }
        if (is_active && edit && edit->caret_visible) {
            const float cx = centered ? (r.x + (r.w + tw) * 0.5f + 1.0f) : (tx + tw + 1.0f);
            ui::fill_rect(ctx, {cx, ty + 1.0f, 2.0f, th - 2.0f}, ctx.palette.accent);
        }
    };
    auto draw_action_chip = [&](int action_id, ui::Rect r, const char* label) {
        if (!edit) return; // При экспорте не рисуем кнопки управления ("+", "-", "По ЛУ").
        if (edit->stamp_slots) {
            edit->stamp_slots->push_back(StampSlot{StampField::None, nullptr, action_id, r});
        }
        const bool is_hover = ui::contains(r, ctx.input.mouse_x, ctx.input.mouse_y);
        ui::fill_rounded_rect(ctx, r, 3.0f, is_hover ? field_active : field_hover);
        ui::stroke_rect(ctx, r, is_hover ? ctx.palette.accent : thin_line);
        draw_centered(f_lab, label, r, is_hover ? ctx.palette.accent : ctx.palette.text_dim);
    };
    auto draw_sign_entry = [&](float sx, float sy, TitleSign& ts, TTF_Font* f_reg) {
        draw_editable_field(StampField::None,
                            ui::Rect{sx, sy, 75.0f * mm, 6.5f * mm},
                            f_reg, ts.role, false, "Должность", &ts.role);
        draw_line_h(sx, sy + 13.0f * mm, 30.0f * mm, false);
        draw_centered(f_lab, "(подпись)", ui::Rect{sx, sy + 13.2f * mm, 30.0f * mm, 4.0f * mm}, ctx.palette.text_dim);
        draw_editable_field(StampField::None,
                            ui::Rect{sx + 32.0f * mm, sy + 7.5f * mm, 43.0f * mm, 6.5f * mm},
                            f_reg, ts.name, false, "И.О. Фамилия", &ts.name);
        draw_editable_field(StampField::None,
                            ui::Rect{sx, sy + 17.5f * mm, 32.0f * mm, 5.5f * mm},
                            f_val_sm, ts.date, false, "ДД.ММ.ГГГГ", &ts.date);
    };

    const int total_pages = std::max(1, static_cast<int>(layout.page_tops.size()));
    for (int p = 0; p < total_pages; ++p) {
        const float pt = layout.page_tops.empty() ? 0.0f : layout.page_tops[static_cast<size_t>(p)];
        const float page_x = origin_x - 25.0f * mm;
        const float page_y = origin_y + pt;
        const float page_w = 210.0f * mm;
        const float page_h = 297.0f * mm;
        if (page_y + page_h < viewport.y - 20.0f || page_y > viewport.y + viewport.h + 20.0f) continue;

        const ui::Rect sheet{page_x, page_y, page_w, page_h};
        if (edit) {
            card_shadow(ctx, sheet, 3.0f, 8);
            ui::fill_rect(ctx, sheet, paper_bg);
            ui::stroke_rect(ctx, sheet, paper_border);
        } else {
            ui::fill_rect(ctx, sheet, paper_bg);
        }

        // Рамка листа А4: отступ 20 мм слева, 5 мм с остальных сторон (185 x 287 мм)
        const float fx = page_x + 20.0f * mm;
        const float fy = page_y + 5.0f * mm;
        const float fw = 185.0f * mm;
        const float fh = 287.0f * mm;
        stroke_box_thick(fx, fy, fw, fh);

        // Боковой штамп слева (графы 19–23): 12 x 145 мм, примыкает к левой стороне рамки внизу
        {
            const float sx = page_x + 8.0f * mm;
            const float sy = page_y + 147.0f * mm;
            const float sw = 12.0f * mm;
            const float sh = 145.0f * mm;
            stroke_box_thick(sx, sy, sw, sh);
            draw_line_v(sx + 5.0f * mm, sy, sh, true);
            static const float hs[5] = {35.0f, 25.0f, 25.0f, 35.0f, 25.0f};
            static const char* s_labels[5] = {
                "Подп. и дата", "Инв. № дубл.", "Взам. инв. №", "Подп. и дата", "Инв. № подл."
            };
            float cy = sy;
            for (int i = 0; i < 5; ++i) {
                const float ch = hs[i] * mm;
                if (i > 0) draw_line_h(sx, cy, sw, true);
                draw_rotated_text(ctx, f_lab, s_labels[i], ui::Rect{sx, cy, 5.0f * mm, ch}, ctx.palette.text_dim);
                cy += ch;
            }
        }

        // Основная надпись в правом нижнем углу рамки или поля Титульного листа (раздел 8 ГОСТ Р 2.105)
        const int first_text_page = gost_title_pages(doc.meta);
        auto& mut_meta = const_cast<GostMeta&>(doc.meta);
        if (doc.meta.has_title_page && p < first_text_page) {
            TTF_Font* f_t_bold = fonts.get(Family::Serif, true, false, 16.0f);
            TTF_Font* f_t_prod = fonts.get(Family::Serif, true, false, 19.0f);
            TTF_Font* f_t_reg  = fonts.get(Family::Serif, false, false, 14.5f);

            if (p == 0) {
                draw_action_chip(7, ui::Rect{fx + fw - 34.0f * mm, fy + 2.5f * mm, 31.0f * mm, 6.0f * mm},
                                 doc.meta.title_by_lu ? "✓ По ЛУ (8.11)" : "По ЛУ (8.11)");

                if (doc.meta.title_by_lu) {
                    // п. 8.11 и Приложение Г (рис. Г.1): титульный лист при утверждении по ЛУ
                    draw_left(f_t_bold, "Утвержден", ui::Rect{fx + 15.0f * mm, fy + 22.0f * mm, 65.0f * mm, 7.0f * mm}, ctx.palette.text);
                    draw_left(f_t_bold, doc.meta.code + "-ЛУ", ui::Rect{fx + 15.0f * mm, fy + 30.0f * mm, 90.0f * mm, 7.0f * mm}, ctx.palette.text);
                } else {
                    // Поле 1: наименование ведомства / организации (вверху по центру)
                    draw_editable_field(StampField::TitleOrg,
                                        ui::Rect{fx + 10.0f * mm, fy + 10.0f * mm, fw - 20.0f * mm, 12.0f * mm},
                                        f_t_reg, doc.meta.title_org, true, "Наименование ведомства или организации",
                                        &mut_meta.title_org);

                    // Поле 2: код по ОКПД 2 (слева)
                    draw_left(f_lab, "Код ОКПД 2:", ui::Rect{fx + 10.0f * mm, fy + 24.0f * mm, 24.0f * mm, 6.5f * mm}, ctx.palette.text_dim);
                    draw_editable_field(StampField::TitleOkpd,
                                        ui::Rect{fx + 34.0f * mm, fy + 24.0f * mm, 45.0f * mm, 6.5f * mm},
                                        f_val_sm, doc.meta.title_okpd, false, "—", &mut_meta.title_okpd);

                    // Поле 3: слева — гриф СОГЛАСОВАНО (опционально), справа — гриф УТВЕРЖДАЮ (опционально)
                    const float p3_y = fy + 36.0f * mm;
                    if (mut_meta.title_agrees.empty()) {
                        draw_action_chip(1, ui::Rect{fx + 10.0f * mm, p3_y, 38.0f * mm, 6.0f * mm}, "+ СОГЛАСОВАНО");
                    } else {
                        draw_left(f_t_bold, "СОГЛАСОВАНО", ui::Rect{fx + 10.0f * mm, p3_y, 44.0f * mm, 6.5f * mm}, ctx.palette.text);
                        draw_action_chip(1, ui::Rect{fx + 55.0f * mm, p3_y + 0.5f * mm, 6.5f * mm, 5.5f * mm}, "+");
                        draw_action_chip(2, ui::Rect{fx + 62.5f * mm, p3_y + 0.5f * mm, 6.5f * mm, 5.5f * mm}, "−");
                        draw_sign_entry(fx + 10.0f * mm, p3_y + 7.0f * mm, mut_meta.title_agrees[0], f_t_reg);
                    }

                    if (mut_meta.title_approves.empty()) {
                        draw_action_chip(3, ui::Rect{fx + 102.0f * mm, p3_y, 36.0f * mm, 6.0f * mm}, "+ УТВЕРЖДАЮ");
                    } else {
                        draw_left(f_t_bold, "УТВЕРЖДАЮ", ui::Rect{fx + 102.0f * mm, p3_y, 42.0f * mm, 6.5f * mm}, ctx.palette.text);
                        draw_action_chip(3, ui::Rect{fx + 145.0f * mm, p3_y + 0.5f * mm, 6.5f * mm, 5.5f * mm}, "+");
                        draw_action_chip(4, ui::Rect{fx + 152.5f * mm, p3_y + 0.5f * mm, 6.5f * mm, 5.5f * mm}, "−");
                        for (size_t ai = 0; ai < std::min<size_t>(2, mut_meta.title_approves.size()); ++ai) {
                            draw_sign_entry(fx + 102.0f * mm, p3_y + 7.0f * mm + 25.0f * static_cast<float>(ai) * mm,
                                            mut_meta.title_approves[ai], f_t_reg);
                        }
                    }
                }

                // Поле 4: наименование изделия (прописными буквами) и документа (по центру листа)
                const float p4_y = fy + 115.0f * mm;
                draw_editable_field(StampField::TitleProduct,
                                    ui::Rect{fx + 15.0f * mm, p4_y, fw - 30.0f * mm, 12.0f * mm},
                                    f_t_prod, doc.meta.title_product, true, "НАИМЕНОВАНИЕ ИЗДЕЛИЯ",
                                    &mut_meta.title_product);
                draw_editable_field(StampField::TitleDocKind,
                                    ui::Rect{fx + 15.0f * mm, p4_y + 14.0f * mm, fw - 30.0f * mm, 10.0f * mm},
                                    f_t_reg, doc.meta.title_doc_kind, true, "Пояснительная записка",
                                    &mut_meta.title_doc_kind);

                // Поле 6: обозначение документа (прописными буквами)
                draw_editable_field(StampField::Code,
                                    ui::Rect{fx + 25.0f * mm, p4_y + 28.0f * mm, fw - 50.0f * mm, 10.0f * mm},
                                    f_t_bold, doc.meta.code, true, "АБВГ.ХХХХХХ.ХХХ ПЗ",
                                    &mut_meta.code);

                if (!doc.meta.title_by_lu) {
                    // Поле 8: слева — дополнительные грифы СОГЛАСОВАНО (п. 8.9, рис. В.1, В.2),
                    // справа — подписи разработчиков / руководства разработки (может быть пустым, если все в Форме 2).
                    const float p8_y = fy + 195.0f * mm;
                    if (mut_meta.title_agrees.size() > 1) {
                        draw_left(f_t_bold, "СОГЛАСОВАНО", ui::Rect{fx + 10.0f * mm, p8_y, 60.0f * mm, 6.5f * mm}, ctx.palette.text);
                        for (size_t gi = 1; gi < std::min<size_t>(3, mut_meta.title_agrees.size()); ++gi) {
                            draw_sign_entry(fx + 10.0f * mm, p8_y + 7.0f * mm + 25.0f * static_cast<float>(gi - 1) * mm,
                                            mut_meta.title_agrees[gi], f_t_reg);
                        }
                    }

                    if (mut_meta.title_devs.empty()) {
                        draw_action_chip(5, ui::Rect{fx + 102.0f * mm, p8_y, 44.0f * mm, 6.0f * mm}, "+ Подпись (поле 8)");
                    } else {
                        draw_left(f_lab, "Подписи (поле 8):", ui::Rect{fx + 102.0f * mm, p8_y, 38.0f * mm, 6.5f * mm}, ctx.palette.text_dim);
                        draw_action_chip(5, ui::Rect{fx + 141.0f * mm, p8_y + 0.5f * mm, 6.5f * mm, 5.5f * mm}, "+");
                        draw_action_chip(6, ui::Rect{fx + 148.5f * mm, p8_y + 0.5f * mm, 6.5f * mm, 5.5f * mm}, "−");
                        for (size_t di = 0; di < std::min<size_t>(2, mut_meta.title_devs.size()); ++di) {
                            draw_sign_entry(fx + 102.0f * mm, p8_y + 7.0f * mm + 25.0f * static_cast<float>(di) * mm,
                                            mut_meta.title_devs[di], f_t_reg);
                        }
                    }
                }

                if (first_text_page > 1) {
                    // п. 8.9: «В этом случае в конце первого листа указывают: Продолжение на следующем листе»
                    draw_centered(f_t_reg, "Продолжение на следующем листе",
                                  ui::Rect{fx + 95.0f * mm, fy + fh - 14.0f * mm, 85.0f * mm, 8.0f * mm},
                                  ctx.palette.text);
                } else {
                    draw_editable_field(StampField::TitleYear,
                                        ui::Rect{fx + 72.0f * mm, fy + fh - 14.0f * mm, 41.0f * mm, 8.0f * mm},
                                        f_t_reg, doc.meta.title_year, true, "2026", &mut_meta.title_year);
                }
            } else {
                // Второй лист титульного листа (п. 8.9: «Продолжение титульного листа» в правом верхнем углу)
                draw_left(f_t_reg, "Продолжение титульного листа",
                          ui::Rect{fx + 110.0f * mm, fy + 10.0f * mm, 70.0f * mm, 7.0f * mm}, ctx.palette.text);
                draw_centered(f_t_bold, doc.meta.title_product,
                              ui::Rect{fx + 15.0f * mm, fy + 24.0f * mm, fw - 30.0f * mm, 9.0f * mm}, ctx.palette.text);
                draw_centered(f_t_reg, doc.meta.code,
                              ui::Rect{fx + 15.0f * mm, fy + 34.0f * mm, fw - 30.0f * mm, 8.0f * mm}, ctx.palette.text);

                const float cont_y = fy + 52.0f * mm;
                if (mut_meta.title_agrees.size() > 3) {
                    draw_left(f_t_bold, "СОГЛАСОВАНО", ui::Rect{fx + 10.0f * mm, cont_y, 60.0f * mm, 6.5f * mm}, ctx.palette.text);
                    for (size_t gi = 3; gi < mut_meta.title_agrees.size(); ++gi) {
                        draw_sign_entry(fx + 10.0f * mm, cont_y + 7.0f * mm + 26.0f * static_cast<float>(gi - 3) * mm,
                                        mut_meta.title_agrees[gi], f_t_reg);
                    }
                }
                float right_y = cont_y;
                if (mut_meta.title_approves.size() > 2) {
                    draw_left(f_t_bold, "УТВЕРЖДАЮ", ui::Rect{fx + 102.0f * mm, right_y, 60.0f * mm, 6.5f * mm}, ctx.palette.text);
                    right_y += 7.0f * mm;
                    for (size_t ai = 2; ai < mut_meta.title_approves.size(); ++ai) {
                        draw_sign_entry(fx + 102.0f * mm, right_y, mut_meta.title_approves[ai], f_t_reg);
                        right_y += 26.0f * mm;
                    }
                }
                for (size_t di = 2; di < mut_meta.title_devs.size(); ++di) {
                    draw_sign_entry(fx + 102.0f * mm, right_y, mut_meta.title_devs[di], f_t_reg);
                    right_y += 26.0f * mm;
                }
                draw_editable_field(StampField::TitleYear,
                                    ui::Rect{fx + 72.0f * mm, fy + fh - 14.0f * mm, 41.0f * mm, 8.0f * mm},
                                    f_t_reg, doc.meta.title_year, true, "2026", &mut_meta.title_year);
            }
        } else if (p == first_text_page) {
            // Форма 2 (первый или заглавный лист ТД): 185 x 40 мм
            const float st_x = fx;
            const float st_y = fy + fh - 40.0f * mm;
            const float st_w = 185.0f * mm;
            const float st_h = 40.0f * mm;
            stroke_box_thick(st_x, st_y, st_w, st_h);

            // Горизонтальная линия y = 15 мм на всю ширину
            draw_line_h(st_x, st_y + 15.0f * mm, st_w, true);
            // Вертикальная линия x = 65 мм на всю высоту 40 мм
            draw_line_v(st_x + 65.0f * mm, st_y, st_h, true);
            // Вертикальная линия x = 132 мм от y = 15 до 40 мм
            draw_line_v(st_x + 132.0f * mm, st_y + 15.0f * mm, 25.0f * mm, true);

            // Верхние 3 строки слева (y = 0..15 мм): 2 пустые + шапка "Изм. | Лист | № докум. | Подп. | Дата"
            draw_line_h(st_x, st_y + 5.0f * mm, 65.0f * mm, false);
            draw_line_h(st_x, st_y + 10.0f * mm, 65.0f * mm, false);
            static const float cx_mm[6] = {0.0f, 7.0f, 17.0f, 40.0f, 55.0f, 65.0f};
            static const char* chg_lbl[5] = {"Изм.", "Лист", "№ докум.", "Подп.", "Дата"};
            for (int i = 1; i < 5; ++i) {
                const float vx = st_x + cx_mm[i] * mm;
                // В 1-й колонки (7 мм) линия идёт только на y = 0..15 мм, остальные идут до 40 мм
                draw_line_v(vx, st_y, (i == 1 ? 15.0f : 40.0f) * mm, true);
            }
            for (int i = 0; i < 5; ++i) {
                draw_centered(f_lab, chg_lbl[i],
                              ui::Rect{st_x + cx_mm[i] * mm, st_y + 10.0f * mm,
                                       (cx_mm[i + 1] - cx_mm[i]) * mm, 5.0f * mm},
                              ctx.palette.text_dim);
            }

            // 5 строк ролей и подписей (y = 15..40 мм)
            static const char* role_lbl[5] = {"Разраб.", "Пров.", "Т.контр.", "Н.контр.", "Утв."};
            static const StampField role_fields[5] = {
                StampField::Dev, StampField::Check, StampField::TCheck, StampField::NCheck, StampField::Approve
            };
            const std::string* role_vals[5] = {
                &doc.meta.dev, &doc.meta.check, &doc.meta.tcheck, &doc.meta.ncheck, &doc.meta.approve
            };
            for (int r = 0; r < 5; ++r) {
                const float ry = st_y + (15.0f + 5.0f * static_cast<float>(r)) * mm;
                if (r > 0) draw_line_h(st_x, ry, 65.0f * mm, false);
                if (r != 2 || !doc.meta.tcheck.empty()
                    || (edit && edit->active_stamp_field == StampField::TCheck)) {
                    draw_left(f_lab, role_lbl[r], ui::Rect{st_x, ry, 17.0f * mm, 5.0f * mm}, ctx.palette.text_dim);
                }
                draw_editable_field(role_fields[r],
                                    ui::Rect{st_x + 17.0f * mm, ry, 23.0f * mm, 5.0f * mm},
                                    f_val_sm, *role_vals[r], false);
            }

            // Графа (2) Обозначение документа: 120 x 15 мм
            draw_editable_field(StampField::Code,
                                ui::Rect{st_x + 65.0f * mm, st_y, 120.0f * mm, 15.0f * mm},
                                f_val_lg, doc.meta.code, true, "Обозначение");

            // Графа (1) Наименование документа: 67 x 25 мм
            draw_editable_field(StampField::Name,
                                ui::Rect{st_x + 65.0f * mm, st_y + 15.0f * mm, 67.0f * mm, 25.0f * mm},
                                f_val_md, doc.meta.name, true, "Наименование документа");

            // Правый блок: Лит. (15 мм = 3x5), Лист (15 мм), Листов (23 мм)
            draw_line_h(st_x + 132.0f * mm, st_y + 20.0f * mm, 53.0f * mm, true);
            draw_line_h(st_x + 132.0f * mm, st_y + 25.0f * mm, 53.0f * mm, true);
            draw_line_v(st_x + 147.0f * mm, st_y + 15.0f * mm, 10.0f * mm, true);
            draw_line_v(st_x + 162.0f * mm, st_y + 15.0f * mm, 10.0f * mm, true);
            draw_line_v(st_x + 137.0f * mm, st_y + 20.0f * mm, 5.0f * mm, false);
            draw_line_v(st_x + 142.0f * mm, st_y + 20.0f * mm, 5.0f * mm, false);

            draw_centered(f_lab, "Лит.", ui::Rect{st_x + 132.0f * mm, st_y + 15.0f * mm, 15.0f * mm, 5.0f * mm}, ctx.palette.text_dim);
            draw_centered(f_lab, "Лист", ui::Rect{st_x + 147.0f * mm, st_y + 15.0f * mm, 15.0f * mm, 5.0f * mm}, ctx.palette.text_dim);
            draw_centered(f_lab, "Листов", ui::Rect{st_x + 162.0f * mm, st_y + 15.0f * mm, 23.0f * mm, 5.0f * mm}, ctx.palette.text_dim);

            draw_centered(f_val_sm, std::to_string(p + 1),
                          ui::Rect{st_x + 147.0f * mm, st_y + 20.0f * mm, 15.0f * mm, 5.0f * mm}, ctx.palette.text);
            draw_centered(f_val_sm, std::to_string(total_pages),
                          ui::Rect{st_x + 162.0f * mm, st_y + 20.0f * mm, 23.0f * mm, 5.0f * mm}, ctx.palette.text);

            // Графа (9) Организация: 53 x 15 мм
            draw_editable_field(StampField::Org,
                                ui::Rect{st_x + 132.0f * mm, st_y + 25.0f * mm, 53.0f * mm, 15.0f * mm},
                                f_val_md, doc.meta.org, true, "Организация");
        } else {
            // Форма 2а (последующие листы): 185 x 15 мм
            const float st_x = fx;
            const float st_y = fy + fh - 15.0f * mm;
            const float st_w = 185.0f * mm;
            const float st_h = 15.0f * mm;
            stroke_box_thick(st_x, st_y, st_w, st_h);

            draw_line_h(st_x, st_y + 5.0f * mm, 65.0f * mm, false);
            draw_line_h(st_x, st_y + 10.0f * mm, 65.0f * mm, false);
            static const float cx_mm[6] = {0.0f, 7.0f, 17.0f, 40.0f, 55.0f, 65.0f};
            static const char* chg_lbl[5] = {"Изм.", "Лист", "№ докум.", "Подп.", "Дата"};
            for (int i = 1; i <= 5; ++i) {
                draw_line_v(st_x + cx_mm[i] * mm, st_y, st_h, true);
            }
            for (int i = 0; i < 5; ++i) {
                draw_centered(f_lab, chg_lbl[i],
                              ui::Rect{st_x + cx_mm[i] * mm, st_y + 10.0f * mm,
                                       (cx_mm[i + 1] - cx_mm[i]) * mm, 5.0f * mm},
                              ctx.palette.text_dim);
            }

            // Графа (2) Обозначение: 110 x 15 мм
            draw_editable_field(StampField::Code,
                                ui::Rect{st_x + 65.0f * mm, st_y, 110.0f * mm, 15.0f * mm},
                                f_val_md, doc.meta.code, true, "Обозначение");

            // Графа (7) Лист: 10 x 15 мм (7 мм шапка + 8 мм номер листа)
            draw_line_v(st_x + 175.0f * mm, st_y, st_h, true);
            draw_line_h(st_x + 175.0f * mm, st_y + 7.0f * mm, 10.0f * mm, true);
            draw_centered(f_lab, "Лист", ui::Rect{st_x + 175.0f * mm, st_y, 10.0f * mm, 7.0f * mm}, ctx.palette.text_dim);
            draw_centered(f_val_sm, std::to_string(p + 1),
                          ui::Rect{st_x + 175.0f * mm, st_y + 7.0f * mm, 10.0f * mm, 8.0f * mm}, ctx.palette.text);
        }

        // Под рамкой (в нижнем поле 5 мм): Копировал и Формат А4
        draw_left(f_lab, "Копировал", ui::Rect{fx + 66.0f * mm, fy + fh, 45.0f * mm, 5.0f * mm}, ctx.palette.text_dim);
        draw_left(f_lab, "Формат А4", ui::Rect{fx + 135.0f * mm, fy + fh, 45.0f * mm, 5.0f * mm}, ctx.palette.text_dim);

        // Лист регистрации изменений (ЛРИ) на последнем листе документа (п. 5.1.8 ГОСТ Р 2.105 и ГОСТ 2.503 Прил. В, Форма 2)
        if (doc.meta.has_lri && p == total_pages - 1) {
            const float lri_bottom = fy + fh - 15.0f * mm;
            const float h_top = fy + 8.0f * mm;
            const float h_mid = fy + 15.0f * mm;
            const float h_bot = fy + 28.0f * mm;
            draw_line_h(fx, h_top, fw, true);
            draw_line_h(fx + 10.0f * mm, h_mid, 74.0f * mm, true);
            draw_line_h(fx, h_bot, fw, true);
            draw_line_h(fx, h_bot + 2.0f, fw, true); // двойная линия под головкой ЛРИ

            draw_centered(f_val_md, "Лист регистрации изменений",
                          ui::Rect{fx, fy, fw, 8.0f * mm}, ctx.palette.text);
            draw_centered(f_lab, "Номера листов (страниц)",
                          ui::Rect{fx + 10.0f * mm, h_top, 74.0f * mm, 7.0f * mm}, ctx.palette.text);

            static const float lri_w[10] = {10.0f, 18.0f, 18.0f, 18.0f, 20.0f, 22.0f, 25.0f, 28.0f, 14.0f, 12.0f};
            static const char* lri_lbl[10] = {
                "Изм.", "изменен-", "заменен-", "новых", "аннулиро-",
                "Всего листов", "Номер", "Входящий №", "Подп.", "Дата"
            };
            static const char* lri_lbl2[10] = {
                "", "ных", "ных", "", "ванных",
                "в докум.", "докум.", "сопров. док.", "", ""
            };
            float cx = fx;
            for (int c = 0; c < 10; ++c) {
                const float cw = lri_w[c] * mm;
                if (c > 0) {
                    const float vy = (c >= 2 && c <= 4) ? h_mid : h_top;
                    draw_line_v(cx, vy, lri_bottom - vy, (c == 1 || c == 5));
                }
                const float ly = (c >= 1 && c <= 4) ? h_mid : h_top;
                const float lh = h_bot - ly;
                if (lri_lbl2[c][0] != '\0') {
                    draw_centered(f_lab, lri_lbl[c], ui::Rect{cx, ly + 1.0f * mm, cw, lh * 0.5f}, ctx.palette.text);
                    draw_centered(f_lab, lri_lbl2[c], ui::Rect{cx, ly + lh * 0.45f, cw, lh * 0.5f}, ctx.palette.text);
                } else {
                    draw_centered(f_lab, lri_lbl[c], ui::Rect{cx, ly, cw, lh}, ctx.palette.text);
                }
                cx += cw;
            }
            for (float ry = h_bot + 8.0f * mm; ry < lri_bottom - 2.0f * mm; ry += 8.0f * mm) {
                draw_line_h(fx, ry, fw, false);
            }
        }
    }

    // ГОСТ Р 2.105 п. 6.2: отрисовка автоматического элемента «Содержание» с отточием и номерами листов.
    if (layout.toc_title_y >= 0.0f) {
        TTF_Font* f_toc_title = fonts.get(Family::Serif, true, false, 19.0f);
        TTF_Font* f_toc_row   = fonts.get(Family::Serif, false, false, 16.0f);
        const float ty = origin_y + layout.toc_title_y;
        if (ty + 36.0f >= viewport.y && ty <= viewport.y + viewport.h) {
            draw_centered(f_toc_title, "Содержание",
                          ui::Rect{origin_x, ty, viewport.w, 28.0f * fonts.zoom()},
                          ctx.palette.text);
        }
        const float dot_step = std::max(4.0f, ui::text_width(f_toc_row, " . "));
        for (const TocEntry& te : layout.toc_entries) {
            const float ry = origin_y + te.y;
            if (ry + te.height < viewport.y || ry > viewport.y + viewport.h) continue;
            // п. 6.2.3: номера подразделов приводят после абзацного отступа, равного двум знакам
            const float indent = (te.level > 1) ? (18.0f * fonts.zoom()) : 0.0f;
            const std::string pg_str = std::to_string(te.page_num);
            const float lw = ui::text_width(f_toc_row, te.label);
            const float pw = ui::text_width(f_toc_row, pg_str);
            ui::draw_text(ctx, f_toc_row, te.label, origin_x + indent, ry, ctx.palette.text);
            ui::draw_text(ctx, f_toc_row, pg_str, origin_x + viewport.w - pw, ry, ctx.palette.text);

            // п. 6.2.2: отточие между заголовком и номером страницы
            float dx = origin_x + indent + lw + 8.0f * fonts.zoom();
            const float max_dx = origin_x + viewport.w - pw - 8.0f * fonts.zoom();
            while (dx + dot_step < max_dx) {
                ui::draw_text(ctx, f_toc_row, ".", dx, ry, ctx.palette.text_dim);
                dx += dot_step;
            }
        }
    }

    // Block backgrounds first, then text on top.
    TTF_Font* f_body = fonts.get(Family::Serif, false, false, 16.0f);
    for (size_t bi = 0; bi < doc.blocks.size() && bi < layout.boxes.size(); ++bi) {
        const Block& block = doc.blocks[bi];
        const BlockBox& box = layout.boxes[bi];
        const GostBlockNum gn = (bi < layout.block_nums.size()) ? layout.block_nums[bi] : GostBlockNum{};
        const float by = origin_y + box.y;
        if (by + box.h < viewport.y || by > viewport.y + viewport.h) continue;

        if (block.type == BlockType::CodeBlock) {
            const auto is_formula = [&]() {
                const std::string& l = block.lang;
                return l == "math" || l == "latex" || l == "tex";
            };
            const bool is_diagram = is_diagram_lang(block.lang) && !is_formula();

            std::string draw_code;
            if (is_diagram) {
                for (const Inline& inl : block.inlines) draw_code += inl.text;
            }
            const bool is_chart = is_diagram && md::is_chart_text(draw_code);
            const float zc = fonts.zoom();

            if (is_formula()) {
                // ГОСТ Р 2.105 п. 6.10: формула по центру строки, справа в круглых скобках номер.
                std::string tex;
                for (const Inline& inl : block.inlines) tex += inl.text;
                const MathNode node = math_from_latex(tex);
                const auto msz = mathdraw::render(nullptr, fonts, node, 0.0f, 0.0f, 18.0f * zc,
                                                  nullptr, nullptr);
                const float fx = origin_x + std::max(0.0f, (box.w - msz.w) * 0.5f);
                const float fy_math = by + std::max(0.0f, (box.h - msz.h) * 0.5f);
                mathdraw::render(&ctx, fonts, node, fx, fy_math, 18.0f * zc, nullptr, nullptr);

                const std::string& num_str = gn.formula_number;
                if (!num_str.empty()) {
                    const float nw = ui::text_width(f_body, num_str);
                    const float nh = ui::text_height(f_body);
                    ui::draw_text(ctx, f_body, num_str,
                                  origin_x + box.w - nw,
                                  by + (box.h - nh) * 0.5f,
                                  ctx.palette.text);
                }
            } else if (is_diagram && is_chart) {
                const auto csz = chartdraw::render_block(nullptr, fonts, draw_code, 0.0f, 0.0f,
                                                         box.w - 28.0f * zc);
                const float cx = origin_x + std::max(0.0f, (box.w - csz.w) * 0.5f);
                chartdraw::render_block(&ctx, fonts, draw_code, cx, by + 4.0f * zc,
                                        box.w - 28.0f * zc);
                draw_centered(f_body, gn.figure_caption,
                              ui::Rect{origin_x, by + box.h - 22.0f * zc, box.w, 20.0f * zc},
                              ctx.palette.text);
            } else if (is_diagram) {
                // ГОСТ Р 2.105 п. 6.9: схема/диаграмма по центру и подрисуночная подпись «Рисунок N».
                const auto dsz = diagdraw::render_block(nullptr, fonts, draw_code, 0.0f, 0.0f,
                                                        box.w - 28.0f * zc);
                const float dx = origin_x + std::max(0.0f, (box.w - dsz.w) * 0.5f);
                diagdraw::render_block(&ctx, fonts, draw_code, dx, by + 4.0f * zc,
                                       box.w - 28.0f * zc);
                draw_centered(f_body, gn.figure_caption,
                              ui::Rect{origin_x, by + box.h - 22.0f * zc, box.w, 20.0f * zc},
                              ctx.palette.text);
            } else {
                const ui::Rect card{origin_x + box.x, by, box.w, box.h};
                ui::stroke_rect(ctx, card, frame_line);
                const float header_h = 30.0f * zc;
                ui::fill_rect(ctx, {origin_x + box.x, by + header_h, box.w, 1.0f}, thin_line);
            }
        } else {
            if (block.type == BlockType::Rule && edit) {
                const float mid_y = by + box.h / 2.0f;
                for (float dx = 0.0f; dx < box.w; dx += 10.0f) {
                    ui::fill_rect(ctx, {origin_x + dx, mid_y, 5.0f, 1.0f}, thin_line);
                }
                const std::string pb_lbl = "— разрыв листа —";
                const float lw = ui::text_width(f_lab, pb_lbl) + 12.0f;
                const ui::Rect lbl_r{origin_x + (box.w - lw) * 0.5f, mid_y - 8.0f, lw, 16.0f};
                ui::fill_rect(ctx, lbl_r, paper_bg);
                draw_centered(f_lab, pb_lbl, lbl_r, ctx.palette.text_dim);
            }
        }
    }

    // ГОСТ Р 2.105 п. 6.8: таблицы с нумерацией «Таблица N» слева над таблицей
    // и двойной горизонтальной линией под головкой таблицы (п. 6.8.5).
    TTF_Font* f_tbl_cap = fonts.get(Family::Serif, false, false, 14.5f);
    for (const TableGrid& g : layout.tables) {
        if (g.block < 0 || static_cast<size_t>(g.block) >= layout.boxes.size()
            || static_cast<size_t>(g.block) >= doc.blocks.size()) continue;
        const BlockBox& box = layout.boxes[g.block];
        const float by = origin_y + box.y;
        if (by + box.h < viewport.y || by > viewport.y + viewport.h) continue;

        const float left = origin_x + g.xs.front();
        const float right = origin_x + g.xs.back();
        const float top = origin_y + g.ys.front();
        float bottom = origin_y + g.ys.back();

        const bool is_fullpage_form = (doc.meta.form_type > 0 && g.block == 0);
        if (is_fullpage_form && !layout.page_tops.empty()) {
            const int first_tp = gost_title_pages(doc.meta);
            const int last_tp = doc.meta.has_lri ? std::max(first_tp, total_pages - 2) : (total_pages - 1);
            const float stamp_h = (last_tp == first_tp) ? 40.0f * mm : 15.0f * mm;
            const float form_bot = origin_y + layout.page_tops[static_cast<size_t>(last_tp)] + 292.0f * mm - stamp_h;
            if (form_bot > bottom) bottom = form_bot;
        } else {
            const std::string tcap = (static_cast<size_t>(g.block) < layout.block_nums.size()
                                      && !layout.block_nums[static_cast<size_t>(g.block)].table_caption.empty())
                ? layout.block_nums[static_cast<size_t>(g.block)].table_caption
                : "Таблица";
            ui::draw_text(ctx, f_tbl_cap, tcap, left, top - ui::text_height(f_tbl_cap) - 4.0f, ctx.palette.text);
        }

        for (float x : g.xs) {
            ui::fill_rect(ctx, {std::round(origin_x + x), std::round(top), 1.0f, std::round(bottom - top)}, frame_line);
        }
        for (size_t ri = 0; ri < g.ys.size(); ++ri) {
            const float y = origin_y + g.ys[ri];
            ui::fill_rect(ctx, {std::round(left), std::round(y), std::round(right - left), 1.0f}, frame_line);
            if (ri == 1 && g.ys.size() > 2) {
                // ГОСТ Р 2.105 п. 6.8.5: головка таблицы отделена двойной линией от остальной части
                ui::fill_rect(ctx, {std::round(left), std::round(y + 2.0f), std::round(right - left), 1.0f}, frame_line);
            }
        }
        if (is_fullpage_form && !g.ys.empty()) {
            for (float ry = origin_y + g.ys.back() + 8.0f * mm; ry < bottom - 2.0f * mm; ry += 8.0f * mm) {
                ui::fill_rect(ctx, {std::round(left), std::round(ry), std::round(right - left), 1.0f}, thin_line);
            }
        }
    }

    if (edit && edit->selecting) {
        for (const Line& line : layout.lines) {
            const float ly = origin_y + line.y;
            if (ly + line.height < viewport.y || ly > viewport.y + viewport.h) continue;
            const int b = line.block;
            if (b < 0 || static_cast<size_t>(b) >= doc.blocks.size()) continue;
            if (b < edit->sel_start.block || b > edit->sel_end.block) continue;
            // A selection never spans two cells, so only its own cell is highlighted.
            if (b == edit->sel_start.block && line.cell != edit->sel_start.cell) continue;
            if (b == edit->sel_end.block && line.cell != edit->sel_end.cell) continue;

            if (line.spans.empty()) {
                ui::fill_rect(ctx, {origin_x + line.x0, ly, 6.0f, line.height}, selection_col);
                continue;
            }
            const Block& block = doc.blocks[b];
            if (line.cell >= 0 && static_cast<size_t>(line.cell) >= block.cells.size()) continue;
            const std::vector<Inline>& inls = inlines_of(block, line.cell);
            for (const Span& span : line.spans) {
                if (span.inl < 0 || static_cast<size_t>(span.inl) >= inls.size()) continue;
                int sb = span.byte_begin, se = span.byte_end;
                if (b == edit->sel_start.block) {
                    if (span.inl < edit->sel_start.inl) continue;
                    if (span.inl == edit->sel_start.inl) sb = std::max(sb, edit->sel_start.offset);
                }
                if (b == edit->sel_end.block) {
                    if (span.inl > edit->sel_end.inl) continue;
                    if (span.inl == edit->sel_end.inl) se = std::min(se, edit->sel_end.offset);
                }
                if (se <= sb) continue;

                const Inline& inl = inls[span.inl];
                TTF_Font* font = font_for(fonts, block, inl.style | line.extra_style);
                const float w0 = span_prefix_width(font, inl.text, span.byte_begin, sb);
                const float w1 = span_prefix_width(font, inl.text, span.byte_begin, se);
                ui::fill_rect(ctx, {origin_x + span.x + w0, ly, w1 - w0, line.height},
                              selection_col);
            }
        }
    }

    int marked_block = -1;
    for (const Line& line : layout.lines) {
        const float ly = origin_y + line.y;
        if (ly + line.height < viewport.y || ly > viewport.y + viewport.h) continue;
        if (line.block < 0 || static_cast<size_t>(line.block) >= doc.blocks.size()) continue;

        const Block& block = doc.blocks[line.block];
        if (line.cell >= 0 && static_cast<size_t>(line.cell) >= block.cells.size()) continue;
        if (line.block != marked_block) {
            marked_block = line.block;
            const size_t bi = static_cast<size_t>(line.block);
            if (bi < layout.markers.size()) {
                const std::string& marker = layout.markers[bi];
                if (!marker.empty()) {
                    const std::string& bl = block.lang;
                    if (block.type == BlockType::CodeBlock && (bl == "math" || bl == "latex" || bl == "tex")) {
                        // skip marker for visual formulas
                    } else {
                        TTF_Font* mf = font_for(fonts, block, 0);
                        const int mlen = static_cast<int>(marker.size());
                        const float mw = span_prefix_width(mf, marker, 0, mlen);
                        const float base = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(mf));
                        draw_span_text(ctx, mf, marker, 0, mlen,
                                       origin_x + line.x0 - 10.0f - mw, base, ctx.palette.text);
                    }
                }
            }
            if (bi < layout.block_nums.size()) {
                const GostBlockNum& gn = layout.block_nums[bi];
                if (!gn.heading_prefix.empty()) {
                    TTF_Font* hf = font_for(fonts, block, 0);
                    const int plen = static_cast<int>(gn.heading_prefix.size());
                    const float pw = span_prefix_width(hf, gn.heading_prefix, 0, plen);
                    const float base = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(hf));
                    draw_span_text(ctx, hf, gn.heading_prefix, 0, plen,
                                   origin_x + line.x0 - pw, base, ctx.palette.text);
                }
                if (!gn.note_prefix.empty()) {
                    TTF_Font* qf = font_for(fonts, block, 0);
                    const int plen = static_cast<int>(gn.note_prefix.size());
                    const float pw = span_prefix_width(qf, gn.note_prefix, 0, plen);
                    const float base = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(qf));
                    if (gn.note_group_header) {
                        const std::string hdr = "Примечания";
                        draw_span_text(ctx, qf, hdr, 0, static_cast<int>(hdr.size()),
                                       origin_x + line.x0 - pw, base - 20.0f * fonts.zoom(), ctx.palette.text);
                    }
                    draw_span_text(ctx, qf, gn.note_prefix, 0, plen,
                                   origin_x + line.x0 - pw, base, ctx.palette.text);
                }
            }
        }

        const std::vector<Inline>& inls = inlines_of(block, line.cell);
        if (block.type == BlockType::CodeBlock) {
            const std::string& bl = block.lang;
            if (bl == "math" || bl == "latex" || bl == "tex" || is_diagram_lang(bl)) {
                continue;
            }
        }

        // Для полностраничных форм ГОСТ Р 2.106: вертикальные заголовки узких граф шапки
        // и подчёркивание заголовков разделов в графе «Наименование» (п. 4.2.3, 4.4.3, 4.8.2).
        bool underline_sec_hdr = false;
        if (doc.meta.form_type > 0 && line.block == 0 && line.cell >= 0 && !layout.tables.empty()) {
            const TableGrid& tg = layout.tables[0];
            if (tg.cols > 0) {
                const int r = line.cell / tg.cols;
                const int c = line.cell % tg.cols;
                const float col_w = tg.xs[static_cast<size_t>(c + 1)] - tg.xs[static_cast<size_t>(c)];
                if (r == 0 && col_w <= 12.5f * mm) {
                    std::string h_txt;
                    for (const Inline& inl : inls) h_txt += inl.text;
                    TTF_Font* hf = fonts.get(Family::Serif, true, false, 11.5f);
                    draw_rotated_text(ctx, hf, h_txt,
                                      ui::Rect{origin_x + tg.xs[static_cast<size_t>(c)],
                                               origin_y + tg.ys[0],
                                               col_w,
                                               tg.ys[1] - tg.ys[0]},
                                      ctx.palette.text);
                    continue;
                }
                if (r > 0) {
                    int non_empty = 0;
                    for (int k = 0; k < tg.cols; ++k) {
                        bool emp = true;
                        for (const Inline& inl : block.cells[static_cast<size_t>(r * tg.cols + k)]) {
                            if (!inl.text.empty()) { emp = false; break; }
                        }
                        if (!emp) ++non_empty;
                    }
                    if (non_empty == 1) underline_sec_hdr = true;
                }
            }
        }
        for (const Span& span : line.spans) {
            if (span.inl < 0 || static_cast<size_t>(span.inl) >= inls.size()) continue;
            const Inline& inl = inls[span.inl];
            TTF_Font* font = font_for(fonts, block, inl.style | line.extra_style);
            const float sx = origin_x + span.x;
            if (span.icon) {
                const float ih = 20.0f * fonts.zoom();
                draw_file_icon(ctx, fonts, inl.href, sx, ly + (line.height - ih) / 2.0f);
                continue;
            }
            if (span.footnote > 0) {
                TTF_Font* f_sup = fonts.get(Family::Serif, false, false, 11.5f);
                const std::string fn_mark = std::to_string(span.footnote);
                const float ty_sup = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(font)) - 2.0f * fonts.zoom();
                draw_span_text(ctx, f_sup, fn_mark, 0, static_cast<int>(fn_mark.size()),
                               sx + 1.0f, ty_sup, ctx.palette.text);
                continue;
            }
            if (span.math) {
                // An inline formula is drawn as a formula; its source shows only while revealed.
                const float fsize = math_size(block, fonts.zoom());
                const std::string tex = inl.text.substr(static_cast<size_t>(span.byte_begin) + 1,
                    static_cast<size_t>(span.byte_end - span.byte_begin - 2));
                const auto msz = mathdraw::render(nullptr, fonts, math_from_latex(tex), 0, 0, fsize,
                                                  nullptr, nullptr);
                mathdraw::render(&ctx, fonts, math_from_latex(tex), sx + 2.0f,
                                 ly + line.baseline - msz.h * 0.8f, fsize, nullptr, nullptr);
                continue;
            }

            const bool inline_code = (inl.style & StyleCode)
                && block.type != BlockType::CodeBlock;
            if (inline_code) {
                ui::fill_rounded_rect(ctx,
                    {sx - 3.0f, ly + 2.0f, span.width + 6.0f, line.height - 4.0f},
                    4.0f, code_bg);
            }

            SDL_Color color = ctx.palette.text;
            if (inl.style & StyleLink) color = ctx.palette.link;
            if (inl.style & StyleCode) color = code_text;

            const float ty = ly + line.baseline - static_cast<float>(TTF_GetFontAscent(font));
            draw_span_text(ctx, font, inl.text, span.byte_begin, span.byte_end,
                           sx, ty, color);
            if (inl.style & StyleLink) {
                ui::fill_rect(ctx, {sx, ly + line.baseline + 3.0f, span.width - 2.0f, 2.0f}, color);
            } else if (underline_sec_hdr) {
                ui::fill_rect(ctx, {sx, ly + line.baseline + 2.0f, span.width, 1.0f}, color);
            }
        }
    }

    // ГОСТ Р 2.105 п. 6.13: отрисовка сносок в конце страницы над основной надписью,
    // отделённых от текста короткой тонкой горизонтальной линией с левой стороны.
    if (!layout.footnotes.empty()) {
        TTF_Font* f_fn_body = fonts.get(Family::Serif, false, false, 13.5f);
        TTF_Font* f_fn_sup  = fonts.get(Family::Serif, false, false, 10.5f);
        for (const PageFootnote& pf : layout.footnotes) {
            const float fy_fn = origin_y + pf.y;
            if (fy_fn + 24.0f < viewport.y || fy_fn - 10.0f > viewport.y + viewport.h) continue;
            if (pf.first_on_page) {
                draw_line_h(origin_x, fy_fn - 2.5f * mm, 35.0f * mm, false);
            }
            const float indent_x = origin_x + 12.5f * mm; // с абзацного отступа (п. 6.13.2)
            const std::string mk = std::to_string(pf.num);
            draw_span_text(ctx, f_fn_sup, mk, 0, static_cast<int>(mk.size()),
                           indent_x, fy_fn - 2.0f * fonts.zoom(), ctx.palette.text);
            const float mw = ui::text_width(f_fn_sup, mk) + 3.0f * fonts.zoom();
            draw_span_text(ctx, f_fn_body, pf.text, 0, static_cast<int>(pf.text.size()),
                           indent_x + mw, fy_fn, ctx.palette.text);
        }
    }

    if (edit && edit->caret_visible && edit->active_stamp_field == StampField::None
        && edit->active_target == nullptr) {
        float cx = 0;
        const int li = locate_caret(doc, layout, fonts, edit->caret, &cx);
        if (li >= 0) {
            const Line& line = layout.lines[li];
            const float ly = origin_y + line.y;
            if (ly + line.height >= viewport.y && ly <= viewport.y + viewport.h) {
                ui::fill_rect(ctx, {origin_x + cx, ly + 2.0f, 2.0f, line.height - 4.0f},
                              ctx.palette.accent);
            }
        }
    }

    SDL_SetRenderClipRect(ctx.renderer, nullptr);
}

} // namespace md
