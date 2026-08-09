#include "HexController.h"
#include "HexModel.h"
#include <QString>
#include <QtEndian>

HexController::HexController(HexModel* hexModel, QObject* parent) : QObject(parent), m_hexModel(hexModel) {}

QVariantMap HexController::currentProperties() const {
    return m_properties;
}

uint32_t HexController::calculateCrc32(const char* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (-(crc & 1) & 0xEDB88320);
        }
    }
    return ~crc;
}

void HexController::parseCurrentBuffer() {
m_packets.clear();
    std::vector<PacketError> modelErrors;
    
    const QByteArray& buffer = m_hexModel->buffer();
    if (buffer.size() < 36) return; // Minimum size: 32b header + 4b file CRC

    const char* data = buffer.constData();
    int offset = 0;

    // 1. Parse File Header
    ParsedPacket headerPacket;
    headerPacket.startOffset = 0;
    headerPacket.totalLength = 32;
    headerPacket.typeName = "File Header";
    headerPacket.hasError = false;
    
    for (int i = 0; i < 8; ++i) {
        uint32_t magic = qFromBigEndian<uint32_t>(data + offset);
        if (magic != 0xDEADBEEF) headerPacket.hasError = true;
        offset += 4;
    }
    
    if (headerPacket.hasError) {
        headerPacket.properties["ErrorMessage"] = "Invalid Header Magic";
        modelErrors.push_back({0, 32, "Invalid Header"});
    }
    m_packets.push_back(headerPacket);

    // 2. Parse Data Packets
    int fileCrcOffset = buffer.size() - 4;
    while (offset + 10 <= fileCrcOffset) {
        int packetStart = offset;
        uint32_t type = qFromBigEndian<uint32_t>(data + offset);
        uint16_t length = qFromBigEndian<uint16_t>(data + offset + 4);
        
        if (offset + 10 + length > fileCrcOffset) break; // Bounds check

        uint32_t readCrc = qFromBigEndian<uint32_t>(data + offset + 6 + length);
        uint32_t calcCrc = calculateCrc32(data + offset, 6 + length);
        
        ParsedPacket pkt;
        pkt.startOffset = packetStart;
        pkt.totalLength = 10 + length;
        pkt.hasError = (readCrc != calcCrc);
        pkt.properties["Payload Length"] = QString::number(length) + " bytes";
        pkt.properties["Read CRC"] = QString("0x%1").arg(readCrc, 8, 16, QChar('0')).toUpper();
        pkt.properties["Calculated CRC"] = QString("0x%1").arg(calcCrc, 8, 16, QChar('0')).toUpper();

        if (pkt.hasError) {
            pkt.properties["HasError"] = true;
            pkt.properties["ErrorMessage"] = "Packet CRC mismatch";
            modelErrors.push_back({packetStart, pkt.totalLength, "CRC Error"});
        } else {
            pkt.properties["HasError"] = false;
        }

        // Decode Payload
        if (type == 0x00000000) {
            pkt.typeName = "String Packet";
            pkt.properties["Decoded ASCII"] = QString::fromLatin1(data + offset + 6, length);
        } 
        else if (type == 0x00000001 && length == 12) {
            pkt.typeName = "Sensor Array";
            float x, y, z;
            memcpy(&x, data + offset + 6, 4);
            memcpy(&y, data + offset + 10, 4);
            memcpy(&z, data + offset + 14, 4);
            pkt.properties["X Axis"] = QString::number(x);
            pkt.properties["Y Axis"] = QString::number(y);
            pkt.properties["Z Axis"] = QString::number(z);
        }
        else if (type == 0x00000002 && length == 8) {
            pkt.typeName = "Timestamp Packet";
            uint64_t ts = qFromBigEndian<uint64_t>(data + offset + 6);
            pkt.properties["Unix Epoch"] = QString::number(ts);
        }
        else {
            pkt.typeName = "Unknown Packet";
        }

        m_packets.push_back(pkt);
        offset += pkt.totalLength;
    }

    // 3. Parse File Footer CRC
    ParsedPacket footerPacket;
    footerPacket.startOffset = fileCrcOffset;
    footerPacket.totalLength = 4;
    footerPacket.typeName = "File Footer CRC";
    
    uint32_t readFileCrc = qFromBigEndian<uint32_t>(data + fileCrcOffset);
    uint32_t calcFileCrc = calculateCrc32(data, fileCrcOffset);
    
    footerPacket.properties["Read CRC"] = QString("0x%1").arg(readFileCrc, 8, 16, QChar('0')).toUpper();
    footerPacket.properties["Calculated CRC"] = QString("0x%1").arg(calcFileCrc, 8, 16, QChar('0')).toUpper();
    footerPacket.hasError = (readFileCrc != calcFileCrc);

    if (footerPacket.hasError) {
        footerPacket.properties["HasError"] = true;
        footerPacket.properties["ErrorMessage"] = "File CRC mismatch";
        modelErrors.push_back({fileCrcOffset, 4, "File CRC Error"});
    } else {
        footerPacket.properties["HasError"] = false;
    }
    
    m_packets.push_back(footerPacket);
   
    // Create bounds vector for the model
    std::vector<PacketBounds> bounds;
    for (const auto& pkt : m_packets) {
        bounds.push_back({pkt.startOffset, pkt.totalLength});
    }
    
    m_hexModel->setPacketBounds(bounds);

    m_hexModel->setErrorRanges(modelErrors);
}

void HexController::updateSensorPayload(int packetStartOffset, float x, float y, float z) {
    if (packetStartOffset < 32) return;

    auto it = std::find_if(m_packets.begin(), m_packets.end(), [&](const ParsedPacket& p) {
        return p.startOffset == packetStartOffset;
    });

    if (it == m_packets.end() || it->typeName != "Sensor Array") return;

    int oldTotalLength = it->totalLength;
    int targetOffset = it->startOffset;

    const QByteArray& buffer = m_hexModel->buffer();
    uint32_t packetType = qFromBigEndian<uint32_t>(buffer.constData() + targetOffset);

    // Construct new packet buffer (4b type + 2b length + 12b payload + 4b CRC)
    QByteArray newPacketData;
    newPacketData.resize(22);
    char* data = newPacketData.data();

    qToBigEndian<uint32_t>(packetType, data);
    qToBigEndian<uint16_t>(12, data + 4);

    // Write the 3 floats using native endianness (matches memcpy parsing)
    memcpy(data + 6, &x, 4);
    memcpy(data + 10, &y, 4);
    memcpy(data + 14, &z, 4);

    uint32_t crc = calculateCrc32(data, 18);
    qToBigEndian<uint32_t>(crc, data + 18);

    m_hexModel->replaceBytes(targetOffset, oldTotalLength, newPacketData);

    QByteArray& updatedBuffer = m_hexModel->getBufferRef();
    uint32_t fileCrc = calculateCrc32(updatedBuffer.constData(), updatedBuffer.size() - 4);
    qToBigEndian<uint32_t>(fileCrc, updatedBuffer.data() + updatedBuffer.size() - 4);

    parseCurrentBuffer();
    selectOffset(targetOffset);
}

void HexController::selectOffset(int offset) {
    m_properties.clear();
    m_properties["Absolute Offset"] = QString::number(offset);
    m_properties["Hex Address"] = QString("0x%1").arg(offset, 8, 16, QChar('0')).toUpper();

    bool found = false;
    for (const auto& pkt : m_packets) {
        if (offset >= pkt.startOffset && offset < (pkt.startOffset + pkt.totalLength)) {
            m_properties["Struct"] = pkt.typeName;
            
            // Explicitly inject the start offset to prevent QML 'undefined' conversion
            m_properties["PacketStartOffset"] = pkt.startOffset; 
            
            for (auto it = pkt.properties.constBegin(); it != pkt.properties.constEnd(); ++it) {
                m_properties[it.key()] = it.value();
            }
            m_hexModel->setSelectionRange(pkt.startOffset, pkt.totalLength);
            found = true;
            break;
        }
    }

    if (!found) {
        m_properties["Struct"] = "Unmapped Data";
        m_properties["HasError"] = false;
        m_hexModel->setSelectionRange(offset, 1);
    }

    emit propertiesChanged();
}


void HexController::updateStringPayload(int packetStartOffset, const QString& newText) {
    QByteArray newPayload = newText.toLatin1();
    if (newPayload.size() > 65535) return;

    // 1. Find the exact packet using the current offset
    auto it = std::find_if(m_packets.begin(), m_packets.end(), [&](const ParsedPacket& p) {
        return p.startOffset == packetStartOffset;
    });
    
    if (it == m_packets.end()) return;

    int oldTotalLength = it->totalLength;
    int targetOffset = it->startOffset; // Capture absolute offset before any state changes

    const QByteArray& buffer = m_hexModel->buffer();
    uint32_t packetType = qFromBigEndian<uint32_t>(buffer.constData() + targetOffset);

    // 2. Construct new packet buffer (4b type + 2b length + payload + 4b CRC)
    QByteArray newPacketData;
    newPacketData.resize(10 + newPayload.size());
    char* data = newPacketData.data();

    qToBigEndian<uint32_t>(packetType, data);
    qToBigEndian<uint16_t>(newPayload.size(), data + 4);
    memcpy(data + 6, newPayload.constData(), newPayload.size());

    uint32_t crc = calculateCrc32(data, 6 + newPayload.size());
    qToBigEndian<uint32_t>(crc, data + 6 + newPayload.size());

    // 3. Perform safe in-place replacement using the verified target offset
    m_hexModel->replaceBytes(targetOffset, oldTotalLength, newPacketData);

    // 4. Recalculate File Footer CRC
    QByteArray& updatedBuffer = m_hexModel->getBufferRef();
    uint32_t fileCrc = calculateCrc32(updatedBuffer.constData(), updatedBuffer.size() - 4);
    qToBigEndian<uint32_t>(fileCrc, updatedBuffer.data() + updatedBuffer.size() - 4);

    // 5. Refresh parser state and maintain selection focus
    parseCurrentBuffer();
    selectOffset(targetOffset); 
}