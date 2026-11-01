// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Henri Lavikainen

#include "dmm/services/frequency_offset.h"

namespace dmm::services {

FmChannel FrequencyOffset::apply(FmChannel channel) const
{
    const std::int32_t shift = hz();
    const auto shifted = [shift](std::uint32_t frequencyHz) -> std::uint32_t {
        if (frequencyHz == 0) {
            return 0;
        }
        const std::int64_t f = static_cast<std::int64_t>(frequencyHz) + shift;
        return f <= 0 ? 0 : static_cast<std::uint32_t>(f);
    };
    channel.receiveHz = shifted(channel.receiveHz);
    channel.transmitHz = shifted(channel.transmitHz);
    return channel;
}

lib::StaticString<16> FrequencyOffset::text() const
{
    const std::int32_t m = milliHz();
    const std::uint32_t tenths = (static_cast<std::uint32_t>(m < 0 ? -m : m) + 50) / 100;
    lib::StaticString<16> s;
    if (tenths != 0) {
        s << (m < 0 ? '-' : '+');
    }
    s << tenths / 10;
    if (tenths % 10 != 0) {
        s << '.' << tenths % 10;
    }
    s << " Hz";
    return s;
}

} // namespace dmm::services
