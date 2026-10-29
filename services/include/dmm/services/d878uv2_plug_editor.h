// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#pragma once

#include "dmm/lib/fn_ref.h"
#include "dmm/services/d878uv2_codeplug.h"
#include "dmm/services/plug_editor.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace dmm::services {

/// Edits the D878UVII's codeplug one record at a time (see
/// plug_editor.h). Every record lists its known fields by name and then
/// each of its bytes, so a value without a named field can still be
/// changed. The record and its copy as read take twice MaxRecordBytes.
class D878Uv2PlugEditor
{
public:
    using Book = plug::Book;
    using FieldInfo = plug::FieldInfo;
    using SaveResult = plug::SaveResult;
    /// Writes a whole record at its codeplug address.
    using Writer = lib::FnRef<SaveResult(std::uint32_t, std::span<const std::uint8_t>)>;

    static constexpr std::size_t MaxRecordBytes = 0x200;

    D878Uv2PlugEditor(D878Uv2Codeplug &codeplug, Writer write)
        : m_codeplug(codeplug), m_write(write)
    {
    }

    std::size_t capacity(Book book) const;
    bool inUse(Book book, std::size_t index);
    std::size_t count(Book book);
    std::ptrdiff_t nearest(Book book, std::ptrdiff_t from, int direction);
    bool title(Book book, std::size_t index, plug::Text out);

    bool open(Book book, std::size_t index);
    void close() { m_open = false; }
    bool isOpen() const { return m_open; }
    Book book() const { return m_book; }
    std::size_t index() const { return m_index; }

    std::size_t fields() const;
    FieldInfo field(std::size_t i) const;
    void label(std::size_t i, plug::Text out) const;
    std::int32_t value(std::size_t i) const { return valueIn(i, m_record); }
    std::int32_t original(std::size_t i) const { return valueIn(i, m_read); }
    bool setValue(std::size_t i, std::int32_t v);
    bool setText(std::size_t i, std::string_view text);
    void show(std::size_t i, plug::Text out, bool original = false);
    void showValue(std::size_t i, std::int32_t v, plug::Text out);
    std::size_t choices(std::size_t i) const;
    bool changed(std::size_t i) const;
    std::size_t changes() const;
    void revert();
    SaveResult save();

    /// The console's codeplug commands, answered in the log:
    ///   peek <address> [count]   up to 256 bytes from a codeplug address
    ///   poke <address> <byte>... up to 32 bytes written there
    /// Addresses and bytes are hexadecimal. A poke goes through the board's
    /// writer like a save, so it reaches only the codeplug's own blocks.
    bool debugCommand(std::string_view text);

    /// One field of a record as the tables describe it.
    struct Field;

private:
    std::uint32_t address(Book book, std::size_t index) const;
    const Field *named(std::size_t i) const;
    std::size_t namedCount() const;
    std::int32_t valueIn(std::size_t i, const std::uint8_t *record) const;
    void textIn(std::size_t i, const std::uint8_t *record, plug::Text out) const;
    void showIn(std::size_t i, std::int32_t v, const std::uint8_t *record, plug::Text out);

    D878Uv2Codeplug &m_codeplug;
    Writer m_write;
    bool m_open = false;
    Book m_book = Book::Channels;
    std::size_t m_index = 0;
    std::size_t m_bytes = 0;
    std::uint8_t m_record[MaxRecordBytes] = {};
    std::uint8_t m_read[MaxRecordBytes] = {};
};

} // namespace dmm::services
