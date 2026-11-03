#let f = (
  name: "Наименование документа",
  code: "АБВГ.123456.001 ПЗ",
  org: "Организация",
  dev: "Иванов",
  check: "Петров",
  tcheck: "",
  ncheck: "Сидоров",
  approve: "Смирнов",
)

#let s-thick = 0.7pt
#let s-thin = 0.25pt

#let lab(t) = text(size: 6pt, style: "italic", t)
#let val(t, size: 8pt) = text(size: size, t)

#let cell(x, y, w, h, body, stroke: s-thick, align-to: horizon + left, inset-x: 0.8mm) = place(
  top + left,
  dx: x, dy: y,
  box(width: w, height: h, stroke: stroke, inset: (x: inset-x, y: 0.3mm))[
    #align(align-to)[#body]
  ],
)

// Боковой штамп (графы 19–23): 12 x 145 мм, примыкает к левой стороне рамки внизу
#let side-stamp() = {
  let hs = (35mm, 25mm, 25mm, 35mm, 25mm) // сверху вниз: (23), (22), (21), (20), (19)
  let labels = (
    [Подп. и дата],
    [Инв. № дубл.],
    [Взам. инв. №],
    [Подп. и дата],
    [Инв. № подл.],
  )
  let y = 0mm
  for i in range(5) {
    let h = hs.at(i)
    cell(0mm, y, 5mm, h, rotate(-90deg, reflow: true)[#lab(labels.at(i))], align-to: horizon + center, inset-x: 0mm)
    cell(5mm, y, 7mm, h, [])
    y += h
  }
}

// Форма 2 (первый или заглавный лист ТД): 185 x 40 мм
#let stamp-first(f, sheet, sheets) = {
  let cx = (0mm, 7mm, 17mm, 40mm, 55mm, 65mm)

  // Верхние 2 пустые строки изменений (y = 0..10mm): тонкие горизонтальные линии
  for r in range(2) {
    let y = 5mm * r
    for i in range(5) {
      cell(cx.at(i), y, cx.at(i + 1) - cx.at(i), 5mm, [],
        stroke: (left: s-thick, right: s-thick, top: if r == 0 { s-thick } else { s-thin }, bottom: s-thin))
    }
  }
  // Строка заголовков изменений (y = 10..15mm)
  for (i, t) in ([Изм.], [Лист], [№ докум.], [Подп.], [Дата]).enumerate() {
    cell(cx.at(i), 10mm, cx.at(i + 1) - cx.at(i), 5mm, lab(t),
      stroke: (left: s-thick, right: s-thick, top: s-thin, bottom: s-thick),
      align-to: horizon + center, inset-x: 0.2mm)
  }

  // Таблица подписей (графы 10–13), y = 15..40mm
  let roles = ([Разраб.], [Пров.], [], [Н.контр.], [Утв.])
  let values = (f.dev, f.check, f.tcheck, f.ncheck, f.approve)
  for r in range(5) {
    let y = 15mm + 5mm * r
    let st = (
      left: s-thick,
      right: s-thick,
      top: if r == 0 { s-thick } else { s-thin },
      bottom: if r == 4 { s-thick } else { s-thin },
    )
    let role-txt = if r == 2 and f.tcheck != "" { [Т.контр.] } else { roles.at(r) }
    cell(0mm, y, 17mm, 5mm, lab(role-txt), stroke: st)
    cell(17mm, y, 23mm, 5mm, val(values.at(r), size: 7pt), stroke: st)
    cell(40mm, y, 15mm, 5mm, [], stroke: st)
    cell(55mm, y, 10mm, 5mm, [], stroke: st)
  }

  // Графа (2) Обозначение документа: 120 x 15 мм
  cell(65mm, 0mm, 120mm, 15mm, val(f.code, size: 11pt), align-to: horizon + center)

  // Графа (1) Наименование: 67 x 25 мм
  cell(65mm, 15mm, 67mm, 25mm, val(f.name, size: 10pt), align-to: horizon + center)

  // Правый блок: Лит. (15 мм = 3x5), Лист (15 мм), Листов (23 мм) -> всего 53 мм
  cell(132mm, 15mm, 15mm, 5mm, lab[Лит.], align-to: horizon + center)
  cell(147mm, 15mm, 15mm, 5mm, lab[Лист], align-to: horizon + center)
  cell(162mm, 15mm, 23mm, 5mm, lab[Листов], align-to: horizon + center)

  // 3 клеточки под литеру по 5 мм (разделены тонкими линиями)
  for i in range(3) {
    cell(132mm + 5mm * i, 20mm, 5mm, 5mm, [],
      stroke: (
        top: s-thick,
        bottom: s-thick,
        left: if i == 0 { s-thick } else { s-thin },
        right: if i == 2 { s-thick } else { s-thin },
      ))
  }
  cell(147mm, 20mm, 15mm, 5mm, val(size: 8pt)[#sheet], align-to: horizon + center)
  cell(162mm, 20mm, 23mm, 5mm, val(size: 8pt)[#sheets], align-to: horizon + center)

  // Графа (9) Организация: 53 x 15 мм
  cell(132mm, 25mm, 53mm, 15mm, val(f.org, size: 9pt), align-to: horizon + center)
}

// Форма 2а (последующие листы): 185 x 15 мм
#let stamp-next(f, sheet) = {
  let cx = (0mm, 7mm, 17mm, 40mm, 55mm, 65mm)
  for r in range(2) {
    let y = 5mm * r
    for i in range(5) {
      cell(cx.at(i), y, cx.at(i + 1) - cx.at(i), 5mm, [],
        stroke: (left: s-thick, right: s-thick, top: if r == 0 { s-thick } else { s-thin }, bottom: s-thin))
    }
  }
  for (i, t) in ([Изм.], [Лист], [№ докум.], [Подп.], [Дата]).enumerate() {
    cell(cx.at(i), 10mm, cx.at(i + 1) - cx.at(i), 5mm, lab(t),
      stroke: (left: s-thick, right: s-thick, top: s-thin, bottom: s-thick),
      align-to: horizon + center, inset-x: 0.2mm)
  }

  // Графа (2) Обозначение: 110 x 15 мм
  cell(65mm, 0mm, 110mm, 15mm, val(f.code, size: 10pt), align-to: horizon + center)

  // Графа (7) Лист: 10 мм шириной, 7 мм шапка + 8 мм номер
  cell(175mm, 0mm, 10mm, 7mm, lab[Лист], align-to: horizon + center, inset-x: 0.2mm)
  cell(175mm, 7mm, 10mm, 8mm, val(size: 8pt)[#sheet], align-to: horizon + center, inset-x: 0.2mm)
}

#let frame-bg = context {
  let p = counter(page).get().first()
  let total = counter(page).final().first()

  // Основная рамка листа А4: отступ 20 мм слева, 5 мм с остальных сторон (185 x 287 мм)
  place(top + left, dx: 20mm, dy: 5mm, rect(width: 185mm, height: 287mm, stroke: s-thick))

  // Боковой штамп слева внизу (примыкает к x = 20mm, y = 292mm - 145mm = 147mm)
  place(top + left, dx: 8mm, dy: 147mm)[
    #side-stamp()
  ]

  // Основная надпись в правом нижнем углу рамки
  let h = if p == 1 { 40mm } else { 15mm }
  place(top + left, dx: 20mm, dy: 292mm - h)[
    #if p == 1 {
      stamp-first(f, p, total)
    } else {
      stamp-next(f, p)
    }
  ]

  // Под рамкой (в нижнем поле 5 мм): Копировал и Формат А4
  place(top + left, dx: 20mm, dy: 292mm)[
    #box(width: 185mm, height: 5mm)[
      #place(top + left, dx: 66mm, dy: 0.8mm)[#lab[Копировал]]
      #place(top + left, dx: 135mm, dy: 0.8mm)[#lab[Формат А4]]
    ]
  ]
}

#set page(
  paper: "a4",
  margin: (left: 25mm, right: 10mm, top: 15mm, bottom: 30mm),
  background: frame-bg,
)
#set text(font: "PT Serif", size: 14pt, lang: "ru")
#set par(justify: true, leading: 0.95em, first-line-indent: 1.27cm)

// Резервируем дополнительные 25 мм внизу первого листа под штамп Формы 2 (40 мм вместо 15 мм)
#place(bottom, float: true, clearance: 0mm, block(width: 100%, height: 25mm))

= 1 Введение

Настоящий документ выполнен в соответствии с требованиями ГОСТ Р 2.105–2019 и ГОСТ Р 2.106–2019. Абзацный отступ равен пяти знакам, межстрочный интервал — полуторный.

#lorem(260)

= 2 Техническая характеристика

#lorem(380)
