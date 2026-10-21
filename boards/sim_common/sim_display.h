// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include "dmm/services/display.h"

#include <QImage>
#include <QMutex>

#include <functional>

/// The simulated panel: a 160 x 128 RGB565 picture plus backlight level and
/// panel state. Drawn from the app's ui task, read from the GUI thread.
class SimDisplay final : public dmm::services::Display
{
public:
    SimDisplay();

    /// Called after every change, from the drawing thread.
    void setChangedCallback(std::function<void()> callback);

    /// The picture as the user would see it: dimmed by the backlight level,
    /// black while the panel sleeps or the backlight is off.
    QImage visibleImage() const;

    void start() override;
    int width() const override { return Width; }
    int height() const override { return Height; }
    void fillRect(int x, int y, int w, int h, dmm::services::Color color) override;
    void drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, dmm::services::Color fg,
        dmm::services::Color bg) override;
    void setBacklight(int level) override;
    void setPanelOn(bool on) override;

private:
    static constexpr int Width = 160;
    static constexpr int Height = 128;

    void changed();

    mutable QMutex m_lock;
    QImage m_image;
    int m_backlight = 0;
    bool m_panelOn = true;
    std::function<void()> m_changed;
};
