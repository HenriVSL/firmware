// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "sim_display.h"

#include <QPainter>

SimDisplay::SimDisplay()
    : m_image(Width, Height, QImage::Format_RGB16)
{
    m_image.fill(Qt::black);
}

void SimDisplay::setChangedCallback(std::function<void()> callback)
{
    QMutexLocker guard(&m_lock);
    m_changed = std::move(callback);
}

void SimDisplay::changed()
{
    std::function<void()> callback;
    {
        QMutexLocker guard(&m_lock);
        callback = m_changed;
    }
    if (callback) {
        callback();
    }
}

void SimDisplay::start()
{
    {
        QMutexLocker guard(&m_lock);
        m_image.fill(Qt::black);
        m_backlight = 0;
        m_panelOn = true;
    }
    changed();
}

void SimDisplay::fillRect(int x, int y, int w, int h, dmm::services::Color color)
{
    {
        QMutexLocker guard(&m_lock);
        const QRect r = QRect(x, y, w, h).intersected(m_image.rect());
        for (int py = r.top(); py <= r.bottom(); ++py) {
            auto *line = reinterpret_cast<quint16 *>(m_image.scanLine(py));
            for (int px = r.left(); px <= r.right(); ++px) {
                line[px] = color;
            }
        }
    }
    changed();
}

void SimDisplay::drawBitmap(int x, int y, int w, int h, const std::uint8_t *rows, int stride, dmm::services::Color fg,
    dmm::services::Color bg)
{
    {
        QMutexLocker guard(&m_lock);
        for (int by = 0; by < h; ++by) {
            const int py = y + by;
            if (py < 0 || py >= Height) {
                continue;
            }
            auto *line = reinterpret_cast<quint16 *>(m_image.scanLine(py));
            for (int bx = 0; bx < w; ++bx) {
                const int px = x + bx;
                if (px < 0 || px >= Width) {
                    continue;
                }
                line[px] = (rows[by * stride + bx / 8] & (0x80u >> (bx % 8))) != 0 ? fg : bg;
            }
        }
    }
    changed();
}

void SimDisplay::setBacklight(int level)
{
    {
        QMutexLocker guard(&m_lock);
        m_backlight = level < 0 ? 0 : level > BacklightLevels ? BacklightLevels
                                                              : level;
    }
    changed();
}

void SimDisplay::setPanelOn(bool on)
{
    {
        QMutexLocker guard(&m_lock);
        m_panelOn = on;
    }
    changed();
}

QImage SimDisplay::visibleImage() const
{
    QMutexLocker guard(&m_lock);
    QImage out = m_image.convertToFormat(QImage::Format_RGB32);
    if (!m_panelOn || m_backlight == 0) {
        out.fill(Qt::black);
        return out;
    }
    if (m_backlight < BacklightLevels) {
        QPainter p(&out);
        p.fillRect(out.rect(), QColor(0, 0, 0, 40 * (BacklightLevels - m_backlight)));
    }
    return out;
}
