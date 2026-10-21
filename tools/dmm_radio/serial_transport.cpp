// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "serial_transport.h"

#include <QDeadlineTimer>

namespace {

// USB IDs of the GD32-based AnyTone radios' programming port.
constexpr quint16 RadioVendorId = 0x28E9;
constexpr quint16 RadioProductId = 0x018A;
// DeltaMM in development: the pid.codes test IDs.
constexpr quint16 DeltaMmVendorId = 0x1209;
constexpr quint16 DeltaMmProductId = 0x0001;

} // namespace

QList<QSerialPortInfo> SerialTransport::findRadios()
{
    QList<QSerialPortInfo> radios;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        if (info.hasVendorIdentifier() && info.hasProductIdentifier() &&
            info.vendorIdentifier() == RadioVendorId && info.productIdentifier() == RadioProductId) {
            radios.append(info);
        }
    }
    return radios;
}

QList<QSerialPortInfo> SerialTransport::findDeltaMmPorts()
{
    QList<QSerialPortInfo> ports;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (info.hasVendorIdentifier() && info.hasProductIdentifier() && info.vendorIdentifier() == DeltaMmVendorId &&
            info.productIdentifier() == DeltaMmProductId) {
            ports.append(info);
        }
    }
    return ports;
}

bool SerialTransport::open(const QString &portName, QString *error)
{
    m_port.setPortName(portName);
    m_port.setBaudRate(QSerialPort::Baud115200);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port.open(QIODevice::ReadWrite)) {
        if (error) {
            *error = m_port.errorString();
        }
        return false;
    }
    m_port.clear();
    return true;
}

bool SerialTransport::write(const QByteArray &data)
{
    if (m_port.write(data) != data.size()) {
        return false;
    }
    return m_port.waitForBytesWritten(1000);
}

QByteArray SerialTransport::read(qsizetype count, int timeoutMs)
{
    QByteArray out;
    QDeadlineTimer deadline(timeoutMs);
    while (out.size() < count) {
        if (m_port.bytesAvailable() == 0 && !m_port.waitForReadyRead(static_cast<int>(deadline.remainingTime()))) {
            break;
        }
        out.append(m_port.read(count - out.size()));
        if (deadline.hasExpired()) {
            break;
        }
    }
    return out;
}

void SerialTransport::discardInput()
{
    m_port.clear(QSerialPort::Input);
}
