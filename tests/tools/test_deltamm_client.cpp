// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tommi Tauriainen

#include "deltamm_client.h"
#include "stock_layout.h"

#include "dmm/services/data_link.h"

#include <doctest/doctest.h>

#include <vector>

using namespace dmm::services;

namespace {

class MemoryStorage final : public Storage
{
public:
    QByteArray bytes = QByteArray(0x40000, '\0');
    StorageResult result = StorageResult::Ok;

    StorageInfo info() override { return {true, 0x98, 0xCB, static_cast<std::uint32_t>(bytes.size()), 2}; }
    StorageResult read(std::uint32_t address, std::span<std::uint8_t> data, bool) override
    {
        if (address + data.size() > static_cast<std::size_t>(bytes.size())) {
            return StorageResult::OutOfRange;
        }
        std::copy_n(bytes.constData() + address, data.size(), reinterpret_cast<char *>(data.data()));
        return result;
    }
};

// The PC client talking straight to the radio's protocol code. Can drop or
// corrupt one reply to test the client's retries.
class LoopbackTransport final : public Transport
{
public:
    explicit LoopbackTransport(Storage &storage)
        : m_link(&storage, "0.9.0")
    {
    }

    bool corruptNext = false;

    bool write(const QByteArray &data) override
    {
        QByteArray reply;
        m_link.feed(std::span(reinterpret_cast<const std::uint8_t *>(data.constData()), static_cast<std::size_t>(data.size())), 0,
            [&](std::span<const std::uint8_t> b) { reply.append(reinterpret_cast<const char *>(b.data()), static_cast<qsizetype>(b.size())); });
        if (corruptNext && reply.size() > 10) {
            reply[6] = static_cast<char>(reply[6] ^ 0x40);
            corruptNext = false;
        }
        m_rx += reply;
        return true;
    }
    QByteArray read(qsizetype count, int) override
    {
        const QByteArray out = m_rx.left(count);
        m_rx.remove(0, out.size());
        return out;
    }
    void discardInput() override { m_rx.clear(); }

private:
    DataLink m_link;
    QByteArray m_rx;
};

} // namespace

TEST_CASE("the PC client reads the radio's information and storage")
{
    MemoryStorage storage;
    for (int i = 0; i < storage.bytes.size(); ++i) {
        storage.bytes[i] = static_cast<char>(i * 13 + i / 4096);
    }
    LoopbackTransport transport(storage);
    DeltaMmClient client(transport);
    QString error;
    const auto info = client.info(&error);
    REQUIRE(info);
    CHECK(info->version == "0.9.0");
    CHECK(info->storageBytes == 0x40000);
    CHECK(info->remappedBlocks == 2);

    QByteArray out;
    DeltaMmClient::ReadReport report;
    REQUIRE(client.read(0x100, 10000, false, out, report, &error));
    CHECK(out == storage.bytes.mid(0x100, 10000));
    CHECK(report.badChunks.isEmpty());

    transport.corruptNext = true;
    REQUIRE(client.read(0, 100, true, out, report, &error));
    CHECK(out == storage.bytes.left(100));
}

TEST_CASE("chunks the chip flags, and chunks it cannot read, are reported apart")
{
    MemoryStorage storage;
    storage.result = StorageResult::Uncorrectable;
    LoopbackTransport transport(storage);
    DeltaMmClient client(transport);
    QByteArray out;
    DeltaMmClient::ReadReport report;
    QString error;
    REQUIRE(client.read(0, 5000, false, out, report, &error));
    CHECK(report.flaggedChunks == QList<quint32>{0, 4096});
    CHECK(report.badChunks.isEmpty());
    CHECK(out == storage.bytes.left(5000));

    storage.result = StorageResult::Timeout;
    report = {};
    REQUIRE(client.read(0, 5000, false, out, report, &error));
    CHECK(report.badChunks == QList<quint32>{0, 4096});
    CHECK(report.flaggedChunks.isEmpty());
}

TEST_CASE("the programming link's view follows each block's valid copy")
{
    // Three blocks: block 0 valid; block 1 not valid, so reads come from
    // block 2.
    QByteArray image(3 * stock_layout::BlockBytes, '\0');
    image[0] = 'A';
    image[stock_layout::BlockBytes] = 'B';
    image[2 * stock_layout::BlockBytes] = 'C';
    image.replace(stock_layout::BlockBytes - 4, 4, QByteArray("\x55\x55\xAA\xAA", 4));
    CHECK(stock_layout::blockValid(image, 0x1000000, 0x1000000));
    CHECK_FALSE(stock_layout::blockValid(image, 0x1000000, 0x1020000));
    const QByteArray view = stock_layout::linkView(image, 0x1000000, 0x1000000, 2 * stock_layout::BlockBytes);
    REQUIRE(view.size() == 2 * stock_layout::BlockBytes);
    CHECK(view.at(0) == 'A');
    CHECK(view.at(stock_layout::BlockBytes) == 'C');
    CHECK(stock_layout::linkView(image, 0x1000000, 0x1040000, 16).isEmpty()); // needs block 3
    image.replace(stock_layout::BlockBytes - 8, 4, QByteArray("\xAA\xAA\x55\x55", 4));
    CHECK_FALSE(stock_layout::blockValid(image, 0x1000000, 0x1000000));
}
