// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/services/settings_store.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace dmm::services::plug {

/// The kinds of records a codeplug holds, as the editor lists them.
enum class Book : std::uint8_t
{
    Channels,
    Zones,
    Contacts,
    GroupLists,
    Repeaters, // the roaming zones' repeaters
    RoamZones,
    RadioIds,
    Settings, // one record each from here on
    MoreSettings,
    Aprs,
    AprsText,
    Count,
};

/// How a field's value is shown and changed.
enum class FieldKind : std::uint8_t
{
    Choice,    // one of choices(): the value is its number
    Number,    // from min to max in steps of step
    Frequency, // in Hz, typed on the keypad
    Text,      // up to maxChars characters
    Link,      // a record of another book by its index; -1 for none
    Byte,      // one raw byte, 0 to 255
};

struct FieldInfo
{
    /// The page the field is listed on: "Basic", "DMR", "FM", "Bytes".
    std::string_view page;
    FieldKind kind = FieldKind::Number;
    std::int32_t min = 0;
    std::int32_t max = 0;
    std::int32_t step = 1;
    std::uint8_t maxChars = 0;
    /// For a link: the book it points into.
    Book link = Book::Count;
    bool readOnly = false;
};

/// Room for a field's label or value as text, with its terminator.
inline constexpr std::size_t TextBytes = 64;
using Text = std::span<char, TextBytes>;

/// A codeplug editor, which the board names (dmm/board/parts.h). It lists
/// the records of each book and edits one record at a time: changes stay
/// in the editor until save() writes the record back.
///
/// - capacity(book), inUse(book, i), count(book): the records.
/// - nearest(book, from, direction): the first record in use from @p from
///   on, going up (+1) or down (-1), or -1.
/// - title(book, i, out): a record's name for a list.
/// - open(book, i): starts editing a record; close() ends it.
/// - fields(), field(i), label(i, out): the open record's fields.
/// - value(i), original(i), setValue(i, v): numbers of the kinds that have
///   them; show(i, out, original) and showValue(i, v, out): as text.
/// - setText(i, text): a text field.
/// - choices(i): how many values a choice has.
/// - changed(i): the field differs from the record as read; listed(i): a
///   change to list when saving; changes(): how many are listed.
/// - revert(), save(): Busy while the PC programs the radio.
/// - debugCommand(text): the editor's console commands, answered in the
///   log; false for one it does not know.
template <typename T>
concept Editor = requires(T &e, const T &c, Book book, std::size_t i, std::ptrdiff_t from, int direction,
    std::int32_t v, std::string_view text, Text out, bool original) {
    { c.capacity(book) } -> std::same_as<std::size_t>;
    { e.inUse(book, i) } -> std::same_as<bool>;
    { e.count(book) } -> std::same_as<std::size_t>;
    { e.nearest(book, from, direction) } -> std::same_as<std::ptrdiff_t>;
    { e.title(book, i, out) } -> std::same_as<bool>;
    { e.open(book, i) } -> std::same_as<bool>;
    e.close();
    { c.isOpen() } -> std::same_as<bool>;
    { c.book() } -> std::same_as<Book>;
    { c.index() } -> std::same_as<std::size_t>;
    { c.fields() } -> std::same_as<std::size_t>;
    { c.field(i) } -> std::same_as<FieldInfo>;
    c.label(i, out);
    { c.value(i) } -> std::same_as<std::int32_t>;
    { c.original(i) } -> std::same_as<std::int32_t>;
    { e.setValue(i, v) } -> std::same_as<bool>;
    { e.setText(i, text) } -> std::same_as<bool>;
    e.show(i, out, original);
    e.showValue(i, v, out);
    { c.choices(i) } -> std::same_as<std::size_t>;
    { c.changed(i) } -> std::same_as<bool>;
    { c.listed(i) } -> std::same_as<bool>;
    { c.changes() } -> std::same_as<std::size_t>;
    e.revert();
    { e.save() } -> std::same_as<SaveResult>;
    { e.debugCommand(text) } -> std::same_as<bool>;
};

} // namespace dmm::services::plug
