// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <QByteArray>
#include <QString>

/// The three files the vendor update tool sends to a radio in update mode:
/// the image (.CDD) and two small headers (.spi, .CDI) that carry its
/// length and load address.
struct UpdateFiles
{
    QByteArray image;
    QByteArray spi;
    QByteArray cdi;
};

/// Where a firmware image starts in MCU flash, and where the radio data
/// above it begins.
inline constexpr quint32 ImageLoadAddress = 0x08004000;
inline constexpr quint32 ImageLimitAddress = 0x080F5800;

/// Checks that @p stock is a firmware set for the D878UVII loading at
/// ImageLoadAddress. Returns an empty string when it is, else the reason.
QString checkStockFiles(const UpdateFiles &stock);

/// Checks that @p image looks like a firmware image for that address: a
/// stack pointer in RAM, a reset handler inside the image, and a size that
/// stays below the radio data. Returns an empty string or the reason.
QString checkImage(const QByteArray &image);

/// The factory headers with their length fields set to @p image's length.
/// Both inputs must have passed their checks.
UpdateFiles makeUpdateFiles(const UpdateFiles &stock, const QByteArray &image);
