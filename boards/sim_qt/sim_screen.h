// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#pragma once

#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

/// Shows the simulated display, scaled up with square pixels.
class SimScreen : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SimScreen(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
};
