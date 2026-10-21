// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/lib/crc32.h"

#include <QByteArray>

/// CRC-32 (as zlib) of @p data, as the radio computes it.
inline quint32 dmmCrc32(const QByteArray &data)
{
    return dmm::lib::crc32(reinterpret_cast<const std::uint8_t *>(data.constData()), static_cast<std::size_t>(data.size()));
}
