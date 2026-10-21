// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "sim_screen.h"

#include "sim_bridge.h"

#include <QPainter>

SimScreen::SimScreen(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    connect(SimBridge::instance(), &SimBridge::displayChanged, this, [this] { update(); });
}

void SimScreen::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(boundingRect(), SimBridge::instance()->display().visibleImage());
}
