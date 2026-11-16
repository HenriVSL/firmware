// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "plug_screens.h"

#include "draw.h"
#include "menu_screen.h"
#include "standby.h"

#include "dmm/services/font.h"

#include <algorithm>
#include <cstring>

namespace dmm::app::ui {

namespace {

using namespace services;
using plug::FieldInfo;
using plug::FieldKind;
namespace p = palette;

const Font &Body = fonts::Spleen6x12;
const Font &Medium = fonts::Spleen8x16;
const Font &Large = fonts::Spleen12x24;

// The letters on each digit key, and the characters the knob steps through.
constexpr const char *KeyLetters[] = {" 0", ".,-_/1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8",
    "wxyz9"};
constexpr std::string_view KnobChars =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,-_/()*#+:@!?";
// A digit key pressed again within this time takes its next letter.
constexpr os::Ticks TapMs = 1200;
// A number with more steps than this is typed rather than chosen.
constexpr std::int32_t MaxChosenSteps = 1000;
// The knob and Up and Down move a frequency this much.
constexpr std::int32_t FrequencyStepHz = 12'500;

constexpr std::string_view ValueHint = "Menu: set   Exit: back";

std::string_view bookName(PlugScreens::Book book)
{
    return menu::info(menu::itemOf(book)).label;
}

bool isDigit(Key key)
{
    return key >= Key::Digit0 && key <= Key::Digit9;
}

int digitOf(Key key)
{
    return static_cast<int>(key) - static_cast<int>(Key::Digit0);
}

// The value screen's choices are made on demand from the field chosen.
board::PlugEditor *valueEditor = nullptr;
std::size_t valueField = 0;
std::int32_t valueMin = 0;
std::int32_t valueStep = 1;

std::int32_t choiceValue(std::size_t i)
{
    return valueMin + static_cast<std::int32_t>(i) * valueStep;
}

menu::ValueText choiceText(std::size_t i)
{
    char text[plug::TextBytes];
    valueEditor->showValue(valueField, choiceValue(i), plug::Text(text, plug::TextBytes));
    menu::ValueText t;
    t << std::string_view(text);
    return t;
}

// How many values a field steps through.
std::size_t stepsOf(board::PlugEditor &editor, std::size_t field, const FieldInfo &f)
{
    return f.kind == FieldKind::Choice ? editor.choices(field)
                                       : static_cast<std::size_t>((f.max - f.min) / f.step + 1);
}

} // namespace

PlugScreens::PlugScreens(board::PlugEditor &editor, menu::Choices &choices, std::string_view pageKey)
    : m_editor(editor), m_choices(choices)
{
    m_editHint << "Menu: edit " << pageKey << ": page Exit: done";
}

plug::Text PlugScreens::text(std::size_t k)
{
    return plug::Text(m_texts[k], plug::TextBytes);
}

std::string_view PlugScreens::textView(std::size_t k) const
{
    return m_texts[k];
}

PlugScreens::Result PlugScreens::show(Screen screen)
{
    m_screen = screen;
    return {Do::Show};
}

std::size_t PlugScreens::placeOf(Book book, std::ptrdiff_t record)
{
    std::size_t place = 0;
    for (std::ptrdiff_t i = m_editor.nearest(book, 0, 1); i >= 0 && i < record; i = m_editor.nearest(book, i + 1, 1)) {
        ++place;
    }
    return place;
}

PlugScreens::Result PlugScreens::openBook(Book book)
{
    m_picking = false;
    if (m_editor.capacity(book) == 1) {
        return openRecord(book, 0);
    }
    return openList(book, -1);
}

// The list of @p book's records, from @p record or the first in use.
PlugScreens::Result PlugScreens::openList(Book book, std::ptrdiff_t record)
{
    const std::ptrdiff_t first = record >= 0 && m_editor.inUse(book, static_cast<std::size_t>(record))
        ? record
        : m_editor.nearest(book, 0, 1);
    if (first < 0) {
        m_picking = false;
        return {Do::Nothing, "None in the codeplug"};
    }
    m_book = book;
    m_count = m_editor.count(book);
    m_record = first;
    m_place = placeOf(book, first);
    return show(Screen::List);
}

PlugScreens::Result PlugScreens::openRecord(Book book, std::size_t record)
{
    if (!m_editor.open(book, record)) {
        return {Do::Nothing, "Cannot read it"};
    }
    m_book = book;
    m_field = 0;
    return show(Screen::Edit);
}

PlugScreens::Result PlugScreens::closeRecord()
{
    const Book book = m_editor.book();
    const std::size_t record = m_editor.index();
    m_editor.close();
    m_picking = false;
    if (m_editor.capacity(book) == 1) {
        return {Do::Leave};
    }
    const Result r = openList(book, static_cast<std::ptrdiff_t>(record));
    return r.what == Do::Show ? r : Result{Do::Leave};
}

// Menu on a field: its value screen, the keypad, the letters, or the list
// of the records it may point to.
PlugScreens::Result PlugScreens::editField()
{
    const FieldInfo f = m_editor.field(m_field);
    if (f.readOnly) {
        return {Do::Nothing, "Read only"};
    }
    m_value = m_editor.value(m_field);
    m_typedLength = 0;
    switch (f.kind) {
    case FieldKind::Number:
        if ((f.max - f.min) / f.step > MaxChosenSteps) {
            return show(Screen::Digits);
        }
        [[fallthrough]];
    case FieldKind::Choice:
    case FieldKind::Byte: {
        valueEditor = &m_editor;
        valueField = m_field;
        valueMin = f.min;
        valueStep = f.step;
        m_choices.clear();
        m_choices.make(stepsOf(m_editor, m_field, f), choiceValue, choiceText);
        m_inUse = m_value >= f.min && m_value <= f.max ? static_cast<std::size_t>((m_value - f.min) / f.step)
                                                       : m_choices.size();
        m_cursor = m_inUse < m_choices.size() ? m_inUse : 0;
        return show(Screen::Value);
    }
    case FieldKind::Frequency:
        return show(Screen::Digits);
    case FieldKind::Text:
        m_editor.show(m_field, plug::Text(m_typed, plug::TextBytes));
        m_typedLength = std::strlen(m_typed);
        m_textCursor = m_typedLength;
        m_lastKey = Key::Count;
        m_tap = 0;
        m_upper = true;
        return show(Screen::Text);
    case FieldKind::Link:
        m_picking = true;
        return openList(f.link, m_value);
    }
    return {};
}

// A page further on (+1) or back (-1): the first field of the next page,
// or of this one and then of the one before.
PlugScreens::Result PlugScreens::page(int direction)
{
    const std::size_t count = m_editor.fields();
    const std::string_view here = m_editor.field(m_field).page;
    std::size_t i = m_field;
    if (direction > 0) {
        while (i + 1 < count && m_editor.field(i).page == here) {
            ++i;
        }
    } else {
        while (i > 0 && m_editor.field(i - 1).page == here) {
            --i;
        }
        if (i == m_field && i > 0) {
            const std::string_view before = m_editor.field(i - 1).page;
            while (i > 0 && m_editor.field(i - 1).page == before) {
                --i;
            }
        }
    }
    m_field = i;
    return {Do::Redraw};
}

PlugScreens::Result PlugScreens::key(Key key, bool press, bool step)
{
    // A release ends nothing: tapping on a key goes on across it.
    if (!step) {
        return {};
    }
    const int up = key == Key::Up ? 1 : key == Key::Down ? -1
                                                         : 0;
    switch (m_screen) {
    case Screen::List:
        return listKey(key, press, up);
    case Screen::Edit:
        return editKey(key, press, up);
    case Screen::Value:
        return valueKey(key, press, up);
    case Screen::Digits:
        return digitsKey(key, press, up);
    case Screen::Text:
        return textKey(key, press, up);
    case Screen::Save:
        return saveKey(key, press, up);
    }
    return {};
}

PlugScreens::Result PlugScreens::listKey(Key key, bool press, int up)
{
    if (up != 0) {
        const std::ptrdiff_t next = m_editor.nearest(m_book, m_record - up, -up);
        if (next < 0) {
            return {};
        }
        m_record = next;
        m_place = up < 0 ? m_place + 1 : m_place - 1;
        return {Do::Redraw};
    }
    if (!press) {
        return {};
    }
    if (key == Key::Menu && m_picking) {
        m_picking = false;
        m_editor.setValue(m_field, static_cast<std::int32_t>(m_record));
        m_book = m_editor.book();
        return show(Screen::Edit);
    }
    if (key == Key::Hash && m_picking && m_editor.field(m_field).min < 0) {
        m_picking = false;
        m_editor.setValue(m_field, -1);
        m_book = m_editor.book();
        return show(Screen::Edit);
    }
    if (key == Key::Menu) {
        return openRecord(m_book, static_cast<std::size_t>(m_record));
    }
    if (key == Key::Exit && m_picking) {
        m_picking = false;
        m_book = m_editor.book();
        return show(Screen::Edit);
    }
    if (key == Key::Exit) {
        return {Do::Leave};
    }
    return {};
}

PlugScreens::Result PlugScreens::editKey(Key key, bool press, int up)
{
    if (up != 0) {
        const std::size_t count = m_editor.fields();
        const std::size_t was = m_field;
        m_field = up < 0 ? std::min(m_field + 1, count - 1) : (m_field > 0 ? m_field - 1 : 0);
        return {m_field != was ? Do::Redraw : Do::Nothing};
    }
    if (!press) {
        return {};
    }
    switch (key) {
    case Key::Front1:
    case Key::Front2:
        return page(key == Key::Front2 ? 1 : -1);
    case Key::Menu:
        return editField();
    case Key::Exit:
        if (m_editor.changes() > 0) {
            m_saveRow = 0;
            return show(Screen::Save);
        }
        return closeRecord();
    default:
        return {};
    }
}

void PlugScreens::moveValue(int steps)
{
    const int n = static_cast<int>(m_choices.size());
    if (n > 0) {
        m_cursor = static_cast<std::size_t>(((static_cast<int>(m_cursor) + steps) % n + n) % n);
    }
}

PlugScreens::Result PlugScreens::valueKey(Key key, bool press, int up)
{
    if (up != 0) {
        // A large value goes up with Up; buttons move as drawn.
        moveValue(menu::styleOf(m_choices) == menu::Style::Spinner ? up : -up);
        return {Do::Redraw};
    }
    if (press && key == Key::Menu) {
        m_editor.setValue(m_field, m_choices.value(m_cursor));
        return show(Screen::Edit);
    }
    if (press && key == Key::Exit) {
        return show(Screen::Edit);
    }
    return {};
}

// A frequency as MHz with five decimals, a number as its digits; returns
// how many of the characters are typed.
std::size_t PlugScreens::digitsText(lib::StaticString<16> &out)
{
    if (m_editor.field(m_field).kind != FieldKind::Frequency) {
        if (m_typedLength > 0) {
            out << std::string_view(m_typed, m_typedLength);
        } else {
            out << m_value;
        }
        return m_typedLength;
    }
    lib::StaticString<12> digits;
    digits << lib::zeroPadded(static_cast<std::uint32_t>(std::max<std::int32_t>(m_value, 0) / 10), MaxDigits);
    for (std::size_t k = 0; k < MaxDigits; ++k) {
        if (k == 3) {
            out << '.';
        }
        out << (k < m_typedLength ? m_typed[k] : digits.view()[k]);
    }
    return m_typedLength + (m_typedLength > 3 ? 1 : 0);
}

PlugScreens::Result PlugScreens::digitsKey(Key key, bool press, int up)
{
    const FieldInfo f = m_editor.field(m_field);
    const bool frequency = f.kind == FieldKind::Frequency;
    if (press && isDigit(key) && m_typedLength < MaxDigits) {
        m_typed[m_typedLength++] = static_cast<char>('0' + digitOf(key));
        return {Do::Redraw};
    }
    if (press && key == Key::Star && m_typedLength > 0) {
        --m_typedLength;
        return {Do::Redraw};
    }
    if (up != 0) {
        m_typedLength = 0;
        m_value = std::clamp(m_value + up * (frequency ? FrequencyStepHz : f.step), f.min, f.max);
        return {Do::Redraw};
    }
    if (press && key == Key::Menu) {
        lib::StaticString<16> text;
        digitsText(text);
        std::int32_t v = 0;
        for (const char c : text.view()) {
            if (c >= '0' && c <= '9') {
                v = v * 10 + (c - '0');
            }
        }
        m_editor.setValue(m_field, std::clamp(frequency ? v * 10 : v, f.min, f.max));
        return show(Screen::Edit);
    }
    if (press && key == Key::Exit) {
        return show(Screen::Edit);
    }
    return {};
}

PlugScreens::Result PlugScreens::textKey(Key key, bool press, int up)
{
    const std::size_t max = m_editor.field(m_field).maxChars;
    if (press && isDigit(key)) {
        const os::Ticks now = os::nowMs();
        const std::string_view set = KeyLetters[digitOf(key)];
        const bool again = key == m_lastKey && now - m_lastTap < TapMs && m_textCursor > 0;
        m_tap = again ? (m_tap + 1) % set.size() : 0;
        char c = set[m_tap];
        if (m_upper && c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        if (again) {
            m_typed[m_textCursor - 1] = c;
        } else if (m_textCursor < max) {
            m_typed[m_textCursor++] = c;
            m_typedLength = std::max(m_typedLength, m_textCursor);
        }
        m_lastKey = key;
        m_lastTap = now;
        return {Do::Redraw};
    }
    m_lastKey = Key::Count;
    if (press && key == Key::Star && m_textCursor > 0) {
        std::memmove(&m_typed[m_textCursor - 1], &m_typed[m_textCursor], m_typedLength - m_textCursor);
        --m_textCursor;
        --m_typedLength;
        return {Do::Redraw};
    }
    if (press && key == Key::Hash) {
        m_upper = !m_upper;
        return {Do::Redraw};
    }
    if (up != 0) {
        if (up > 0 && m_textCursor > 0) {
            --m_textCursor;
        } else if (up < 0 && m_textCursor < m_typedLength && m_textCursor + 1 < max) {
            ++m_textCursor;
        }
        return {Do::Redraw};
    }
    if (press && key == Key::Menu) {
        m_editor.setText(m_field, std::string_view(m_typed, m_typedLength));
        return show(Screen::Edit);
    }
    if (press && key == Key::Exit) {
        return show(Screen::Edit);
    }
    return {};
}

PlugScreens::Result PlugScreens::saveKey(Key key, bool press, int up)
{
    if (up != 0) {
        const std::size_t count = m_editor.changes();
        const std::size_t was = m_saveRow;
        m_saveRow = up < 0 ? (m_saveRow + 1 < count ? m_saveRow + 1 : m_saveRow) : (m_saveRow > 0 ? m_saveRow - 1 : 0);
        return {m_saveRow != was ? Do::Redraw : Do::Nothing};
    }
    if (!press) {
        return {};
    }
    switch (key) {
    case Key::Menu:
        return {Do::Save};
    case Key::Hash: {
        m_editor.revert();
        Result r = closeRecord();
        r.notice = "Changes dropped";
        return r;
    }
    case Key::Exit:
        return show(Screen::Edit);
    default:
        return {};
    }
}

PlugScreens::Result PlugScreens::knob(int moved)
{
    if (moved == 0) {
        return {};
    }
    if (m_screen == Screen::Text) {
        const std::size_t max = m_editor.field(m_field).maxChars;
        m_lastKey = Key::Count;
        if (m_textCursor >= max) {
            return {};
        }
        if (m_textCursor == m_typedLength) {
            m_typed[m_typedLength++] = ' ';
        }
        const std::size_t at = KnobChars.find(m_typed[m_textCursor]);
        const int n = static_cast<int>(KnobChars.size());
        const int to = ((at == std::string_view::npos ? 0 : static_cast<int>(at)) + moved % n + n) % n;
        m_typed[m_textCursor] = KnobChars[static_cast<std::size_t>(to)];
        return {Do::Redraw};
    }
    if (m_screen == Screen::Value) {
        moveValue(moved);
        return {Do::Redraw};
    }
    // Clockwise goes down a list and raises a typed value.
    const Key key = (moved > 0) == (m_screen == Screen::Digits) ? Key::Up : Key::Down;
    Result r;
    for (int k = 0; k < (moved > 0 ? moved : -moved); ++k) {
        const Result step = this->key(key, false, true);
        if (step.what != Do::Nothing) {
            r = step;
        }
    }
    return r;
}

void PlugScreens::draw(Display &d)
{
    switch (m_screen) {
    case Screen::List:
        drawList(d);
        break;
    case Screen::Edit:
        drawEdit(d);
        break;
    case Screen::Value:
        drawValue(d);
        break;
    case Screen::Digits:
        drawDigitsEntry(d);
        break;
    case Screen::Text:
        drawTextEntry(d);
        break;
    case Screen::Save:
        drawSave(d);
        break;
    }
}

// A book's records around the chosen one, or the records a link field may
// point to: each one's name and number.
void PlugScreens::drawList(Display &d)
{
    const std::size_t first = firstShown(m_place, m_count, ListRows);
    std::ptrdiff_t from = m_record;
    for (std::size_t k = first; k < m_place; ++k) {
        const std::ptrdiff_t before = from > 0 ? m_editor.nearest(m_book, from - 1, -1) : -1;
        if (before < 0) {
            break;
        }
        from = before;
    }
    ListRow rows[ListRows];
    std::size_t n = 0;
    for (std::ptrdiff_t i = from; i >= 0 && n < std::size(rows); i = m_editor.nearest(m_book, i + 1, 1)) {
        m_editor.title(m_book, static_cast<std::size_t>(i), text(n));
        lib::TextWriter w(std::span(m_texts[ListRows + n], plug::TextBytes - 1));
        w << '#' << i + 1;
        m_texts[ListRows + n][w.size()] = '\0';
        rows[n] = {textView(n), textView(ListRows + n)};
        ++n;
    }
    lib::StaticString<menu::TitleChars> title;
    if (m_picking) {
        m_editor.label(m_field, text(Texts - 1));
        title << textView(Texts - 1);
    } else {
        title << bookName(m_book);
    }
    drawListScreen(d, title.view(), std::span(rows, n), first, m_count, m_place,
        m_picking ? "Menu: pick  #: none  Exit: back" : "Menu: open   Exit: back");
}

// The open record's fields around the chosen one, with a panel saying what
// the chosen one takes, or what it was before a change.
void PlugScreens::drawEdit(Display &d)
{
    const std::size_t count = m_editor.fields();
    const std::size_t first = firstShown(m_field, count, MenuRows);
    ListRow rows[MenuRows];
    std::size_t n = 0;
    for (; n < std::size(rows) && first + n < count; ++n) {
        const std::size_t i = first + n;
        m_editor.label(i, text(n));
        m_editor.show(i, text(MenuRows + n));
        rows[n] = {textView(n), textView(MenuRows + n), false, m_editor.changed(i)};
    }
    const FieldInfo f = m_editor.field(m_field);
    lib::StaticString<menu::AboutChars> about1;
    lib::StaticString<menu::AboutChars> about2;
    about1 << f.page;
    if (f.readOnly) {
        about1 << ", read only";
    }
    if (m_editor.changed(m_field)) {
        m_editor.show(m_field, text(12), true);
        about2 << "Was " << textView(12);
    } else {
        switch (f.kind) {
        case FieldKind::Choice:
            about2 << m_editor.choices(m_field) << " choices";
            break;
        case FieldKind::Number:
            m_editor.showValue(m_field, f.min, text(12));
            m_editor.showValue(m_field, f.max, text(13));
            about2 << textView(12) << " to " << textView(13);
            break;
        case FieldKind::Frequency:
            about2 << "MHz, from the keypad";
            break;
        case FieldKind::Text:
            about2 << "Up to " << f.maxChars << " letters";
            break;
        case FieldKind::Link:
            about2 << "One of the " << bookName(f.link);
            break;
        case FieldKind::Byte:
            about2 << "A byte of the record";
            break;
        }
    }
    m_editor.title(m_editor.book(), m_editor.index(), text(14));
    lib::StaticString<menu::TitleChars> title;
    title << textView(14);
    if (m_editor.changes() > 0) {
        title << '*';
    }
    drawMenuScreen(d, title.view(), std::span(rows, n), first, count, m_field, about1.view(), about2.view(),
        m_editHint.view());
}

void PlugScreens::drawValue(Display &d)
{
    m_editor.label(m_field, text(Texts - 1));
    drawValueScreen(d, textView(Texts - 1), m_choices, m_cursor, m_inUse, ValueHint);
}

// Digits typed over a value: the typed part bright, the rest dim, the unit
// after it, a cursor under the next digit and two lines of notes.
void PlugScreens::drawDigitsEntry(Display &d)
{
    const bool frequency = m_editor.field(m_field).kind == FieldKind::Frequency;
    lib::StaticString<16> value;
    const std::size_t typed = std::min(digitsText(value), value.size());
    m_editor.label(m_field, text(Texts - 1));
    drawTitleBand(d, textView(Texts - 1), 0, 0);
    const int w = d.width();
    const int top = bodyTop();
    const int bodyEnd = d.height() - StandbyScreen::HintH;
    const std::string_view all = value.view();
    const std::string_view done(all.data(), typed);
    const std::string_view rest(all.data() + typed, all.size() - typed);
    const std::string_view unit = frequency ? "MHz" : "";
    constexpr int X = 8;
    const int y = top + 10;
    const int restX = X + textWidth(Large, done);
    const int unitX = X + textWidth(Large, all) + 4;
    band(d, top, 36,
        {{X, y, done, &Large, p::Text}, {restX, y, rest, &Large, p::Rule},
            {unitX, y + Large.height - Body.height - 2, unit, &Body, p::Dim}});
    const int cursorY = top + 36;
    const int cursorW = rest.empty() ? 0 : Large.width;
    fillArea(d, 0, cursorY, restX, 3, p::Ground);
    fillArea(d, restX, cursorY, cursorW, 3, p::Warn);
    fillArea(d, restX + cursorW, cursorY, w - restX - cursorW, 3, p::Ground);
    const int notesY = cursorY + 11;
    fillArea(d, 0, cursorY + 3, w, notesY - cursorY - 3, p::Ground);
    band(d, notesY, Body.height + 2,
        {{X, notesY, frequency ? "Knob: 12.5 kHz steps" : "Knob: one up or down", &Body, p::Dim}});
    band(d, notesY + Body.height + 2, Body.height + 2, {{X, notesY + Body.height + 2, "*: delete a digit", &Body, p::Rule}});
    const int end = notesY + 2 * (Body.height + 2);
    fillArea(d, 0, end, w, bodyEnd - end, p::Ground);
    drawHintBar(d, "0-9  *: delete  Menu: set");
}

// A text typed letter by letter, with a cursor on one character, and the
// letters of the key last pressed with the one taken lifted.
void PlugScreens::drawTextEntry(Display &d)
{
    const std::size_t maxChars = std::max<std::size_t>(m_editor.field(m_field).maxChars, 1);
    const std::size_t cursor = std::min<std::size_t>(m_textCursor, maxChars - 1);
    const std::string_view typed(m_typed, m_typedLength);
    m_editor.label(m_field, text(Texts - 2));
    drawTitleBand(d, textView(Texts - 2), cursor, maxChars);
    const int w = d.width();
    const int top = bodyTop();
    const int bodyEnd = d.height() - StandbyScreen::HintH;
    // As large as the longest text fits; a long text wraps over lines.
    constexpr int X = 6;
    const int room = w - 2 * X;
    const Font &f = static_cast<int>(maxChars) * Large.width <= room ? Large
        : static_cast<int>(maxChars) * Medium.width <= room          ? Medium
                                                                     : Body;
    const std::size_t perLine = static_cast<std::size_t>(room / f.width);
    const std::size_t lines = (maxChars + perLine - 1) / perLine;
    const int lineH = f.height + 2;
    int y = top + 6;
    fillArea(d, 0, top, w, y - top, p::Ground);
    for (std::size_t l = 0; l < lines; ++l, y += lineH) {
        const std::size_t from = l * perLine;
        const std::size_t to = std::min(from + perLine, maxChars);
        int x = X;
        fillArea(d, 0, y, X, lineH, p::Ground);
        for (std::size_t c = from; c < to; ++c) {
            const char ch = c < typed.size() ? typed[c] : ' ';
            const bool here = c == cursor;
            services::drawText(d, f, x, y, std::string_view(&ch, 1), here ? p::Ground : p::Text,
                here ? p::Warn : p::Ground);
            fillArea(d, x, y + f.height, f.width, lineH - f.height, p::Ground);
            x += f.width;
        }
        fillArea(d, x, y, w - x, lineH, p::Ground);
    }
    std::string_view letters;
    if (isDigit(m_lastKey)) {
        const char *set = KeyLetters[digitOf(m_lastKey)];
        std::size_t k = 0;
        for (; set[k] != '\0'; ++k) {
            const char c = set[k];
            m_texts[Texts - 1][k] = m_upper && c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
        }
        m_texts[Texts - 1][k] = '\0';
        letters = textView(Texts - 1);
    }
    const int keysY = y + 6;
    fillArea(d, 0, y, w, keysY - y, p::Ground);
    constexpr int BoxW = 22;
    constexpr int BoxH = 20;
    const int n = static_cast<int>(std::min<std::size_t>(letters.size(), 6));
    const int x0 = (w - n * (BoxW + 4) + 4) / 2;
    fillArea(d, 0, keysY, x0, BoxH, p::Ground);
    int x = x0;
    for (int i = 0; i < n; ++i) {
        const bool on = static_cast<std::size_t>(i) == m_tap;
        const std::string_view letter(letters.data() + i, 1);
        band(d, keysY, BoxH,
            {{x + (BoxW - Medium.width) / 2, keysY + (BoxH - Medium.height) / 2, letter, &Medium,
                on ? p::Ground : p::Text}},
            on ? p::Text : p::Panel, x, x + BoxW);
        fillArea(d, x + BoxW, keysY, i + 1 < n ? 4 : 0, BoxH, p::Ground);
        x += BoxW + (i + 1 < n ? 4 : 0);
    }
    fillArea(d, x, keysY, w - x, BoxH, p::Ground);
    fillArea(d, 0, keysY + BoxH, w, bodyEnd - keysY - BoxH, p::Ground);
    drawHintBar(d, m_upper ? "#: ABC *: del Menu: set" : "#: abc *: del Menu: set");
}

// The @p k-th change listed, as a field number, or -1.
std::ptrdiff_t PlugScreens::changedField(std::size_t k)
{
    for (std::size_t i = 0; i < m_editor.fields(); ++i) {
        if (m_editor.listed(i) && k-- == 0) {
            return static_cast<std::ptrdiff_t>(i);
        }
    }
    return -1;
}

// The changed fields with their new values; the panel shows the chosen
// one's old value.
void PlugScreens::drawSave(Display &d)
{
    const std::size_t count = m_editor.changes();
    m_saveRow = std::min(m_saveRow, count > 0 ? count - 1 : 0);
    const std::size_t first = firstShown(m_saveRow, count, MenuRows);
    ListRow rows[MenuRows];
    std::size_t n = 0;
    for (; n < std::size(rows) && first + n < count; ++n) {
        const std::ptrdiff_t i = changedField(first + n);
        if (i < 0) {
            break;
        }
        m_editor.label(static_cast<std::size_t>(i), text(n));
        m_editor.show(static_cast<std::size_t>(i), text(MenuRows + n));
        rows[n] = {textView(n), textView(MenuRows + n), false, true};
    }
    lib::StaticString<menu::AboutChars> was;
    if (const std::ptrdiff_t i = changedField(m_saveRow); i >= 0) {
        m_editor.show(static_cast<std::size_t>(i), text(12), true);
        was << "Was " << textView(12);
    }
    lib::StaticString<menu::AboutChars> changes;
    changes << count << (count == 1 ? " change" : " changes");
    m_editor.title(m_editor.book(), m_editor.index(), text(14));
    lib::StaticString<menu::TitleChars> title;
    title << "Save " << textView(14) << '?';
    drawMenuScreen(d, title.view(), std::span(rows, n), first, count, m_saveRow, was.view(), changes.view(),
        "Menu: save  Exit: edit  #: drop");
}

} // namespace dmm::app::ui
