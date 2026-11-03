// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "menu.h"

#include "dmm/board/parts.h"
#include "dmm/lib/format.h"
#include "dmm/os/os.h"
#include "dmm/services/display.h"
#include "dmm/services/input.h"
#include "dmm/services/plug_editor.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dmm::app::ui {

/// The codeplug editor's screens, under the status bar: a book's records,
/// one record's fields, a field's value chosen, typed as digits or as text,
/// and the changes before they are saved. The ui task draws the status bar,
/// carries out what a key asks for and shows the notices.
class PlugScreens
{
public:
    using Book = services::plug::Book;

    enum class Screen : std::uint8_t
    {
        List,   // a book's records, or the records a link may point to
        Edit,   // one record's fields
        Value,  // one field's value, chosen
        Digits, // a frequency or a number, typed
        Text,   // a name or a text, typed
        Save,   // what changed, before saving
    };

    /// What a key or the knob asks the ui task to do.
    enum class Do : std::uint8_t
    {
        Nothing,
        Redraw, // the screen changed in place: draw()
        Show,   // another screen is up: the status bar, then draw()
        Leave,  // back to the menu row the editor opened from
        Save,   // the open record is to be saved: save()
    };
    struct Result
    {
        Do what = Do::Nothing;
        /// A notice to show over the screen, or nullptr.
        const char *notice = nullptr;
    };

    /// @p choices holds the value screen's choices while it is up; the
    /// menu's own serve, as the menu has none up then. @p pageKey names the
    /// key that turns to the next page.
    PlugScreens(board::PlugEditor &editor, menu::Choices &choices, std::string_view pageKey);

    board::PlugEditor &editor() { return m_editor; }
    Screen screen() const { return m_screen; }
    /// The book listed, or of the open record.
    Book book() const { return m_book; }

    /// Opens @p book: its list, or its only record.
    Result openBook(Book book);
    /// Ends editing the open record: back to its book's list, or out of
    /// the editor for a record that stands alone.
    Result closeRecord();
    /// Saves the open record (the ui task first lets the settings store
    /// write what it keeps).
    services::SaveResult save() { return m_editor.save(); }

    /// The screen up, drawn in place.
    void draw(services::Display &d);

    /// A key: @p press on its press, @p step on its press and its repeats.
    Result key(services::Key key, bool press, bool step);
    /// The knob, clockwise positive.
    Result knob(int moved);

private:
    Result openList(Book book, std::ptrdiff_t record);
    Result openRecord(Book book, std::size_t record);
    Result editField();
    Result page(int direction);
    Result listKey(services::Key key, bool press, int up);
    Result editKey(services::Key key, bool press, int up);
    Result valueKey(services::Key key, bool press, int up);
    Result digitsKey(services::Key key, bool press, int up);
    Result textKey(services::Key key, bool press, int up);
    Result saveKey(services::Key key, bool press, int up);
    Result show(Screen screen);
    std::size_t placeOf(Book book, std::ptrdiff_t record);
    std::ptrdiff_t changedField(std::size_t k);
    void moveValue(int steps);
    std::size_t digitsText(lib::StaticString<16> &out);

    void drawList(services::Display &d);
    void drawEdit(services::Display &d);
    void drawValue(services::Display &d);
    void drawDigitsEntry(services::Display &d);
    void drawTextEntry(services::Display &d);
    void drawSave(services::Display &d);

    // A text of the rows and panels, kept off the ui task's stack.
    services::plug::Text text(std::size_t k);
    std::string_view textView(std::size_t k) const;

    static constexpr std::size_t Texts = 16;
    static constexpr std::size_t MaxDigits = 8;

    board::PlugEditor &m_editor;
    menu::Choices &m_choices;
    lib::StaticString<40> m_editHint;
    Screen m_screen = Screen::List;
    // The book listed, the record chosen in it and its place among those
    // in use.
    Book m_book = Book::Channels;
    std::ptrdiff_t m_record = -1;
    std::size_t m_place = 0;
    std::size_t m_count = 0;
    // The field chosen in the open record, and the row of the changes.
    std::size_t m_field = 0;
    std::size_t m_saveRow = 0;
    // Choosing a record for a link field, rather than opening one.
    bool m_picking = false;
    // The value screen's choice and the one in use.
    std::size_t m_cursor = 0;
    std::size_t m_inUse = 0;
    // What is typed for a number or a text, and the text's cursor.
    std::int32_t m_value = 0;
    char m_typed[services::plug::TextBytes] = {};
    std::size_t m_typedLength = 0;
    std::size_t m_textCursor = 0;
    // Letters typed by pressing a digit key again and again.
    services::Key m_lastKey = services::Key::Count;
    os::Ticks m_lastTap = 0;
    std::size_t m_tap = 0;
    bool m_upper = true;
    char m_texts[Texts][services::plug::TextBytes] = {};
};

} // namespace dmm::app::ui
