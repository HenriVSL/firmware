// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <QByteArray>

/// The radio keeps each 128 KB block in two copies, the block itself and
/// the one after it. Its programming link answers a read from the block
/// when that is marked valid (last four bytes 55 55 AA AA, the four before
/// them not AA AA 55 55), else from the next block.
namespace stock_layout {

inline constexpr quint32 BlockBytes = 0x20000;

bool blockValid(const QByteArray &image, quint32 imageBase, quint32 block);

/// What the programming link would return for @p length bytes from
/// @p address, given a raw @p image that starts at @p imageBase and covers
/// every block involved and the one after each. Empty if not covered.
QByteArray linkView(const QByteArray &image, quint32 imageBase, quint32 address, quint32 length);

} // namespace stock_layout
