#include "HexController.h"
#include "HexModel.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtEndian>
#include <algorithm>

HexController::HexController(HexModel* model)
    : QObject(nullptr), m_hexModel(model)
{
    connect(m_hexModel, &HexModel::bufferChanged, this, &HexController::parseCurrentBuffer);
}

uint32_t HexController::calculateCrc32(const char* data, int length) {
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < length; ++i) {
        uint32_t byte = static_cast<uint8_t>(data[i]);
        crc = crc ^ byte;
        for (int j = 0; j < 8; ++j) {
            uint32_t mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

void HexController::loadConfiguration(const QString& configPath) {
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) return;

    QJsonArray packets = doc.object()["packets"].toArray();
    m_packetConfigs.clear();

    for (const QJsonValue& val : packets) {
        QJsonObject pktObj = val.toObject();
        uint32_t type = pktObj["matchType"].toInt();

        PacketConfig config;
        config.name = pktObj["name"].toString();

        QJsonArray fields = pktObj["fields"].toArray();
        for (const QJsonValue& fVal : fields) {
            QJsonObject fieldObj = fVal.toObject();
            PacketField field;
            field.name = fieldObj["name"].toString();
            field.byteOffset = fieldObj["byteOffset"].toInt();
            field.dataType = fieldObj["dataType"].toString("enum");
            // field.bitMask = fieldObj["bitMask"].toInt();

            if (field.dataType == "enum") {
                field.bitMask = fieldObj["bitMask"].toInt(0xFF);
                QJsonObject enums = fieldObj["enums"].toObject();
                for (auto it = enums.begin(); it != enums.end(); ++it) {
                    uint8_t intVal = it.key().toUInt();
                    QString strVal = it.value().toString();
                    field.valToStr[intVal] = strVal;
                    field.strToVal[strVal] = intVal;
                }
            }

            config.fields.append(field);
        }
        m_packetConfigs[type] = config;
    }
}

void HexController::parseCurrentBuffer() {
    if (!m_hexModel) return;

    m_packets.clear();
    const QByteArray& buffer = m_hexModel->buffer();
    const char* data = buffer.constData();
    int size = buffer.size();

    if (size < 36) return;

    m_hexModel->clearErrorBytes();

    ParsedPacket headerPkt;
    headerPkt.startOffset = 0;
    headerPkt.totalLength = 32;
    headerPkt.typeName = "File Header";
    m_packets.append(headerPkt);

    int offset = 32;

    while (offset + 10 <= size - 4) {
        uint32_t type = qFromBigEndian<uint32_t>(data + offset);
        uint16_t payloadLength = qFromBigEndian<uint16_t>(data + offset + 4);

        int totalPacketLength = 10 + payloadLength;
        if (offset + totalPacketLength > size - 4) break;

        ParsedPacket pkt;
        pkt.startOffset = offset;
        pkt.totalLength = totalPacketLength;

        if (m_packetConfigs.contains(type)) {
            const PacketConfig& config = m_packetConfigs[type];
            pkt.typeName = config.name;

            for (const PacketField& field : config.fields) {
                if (field.byteOffset + 4 <= totalPacketLength) {
                    if (field.dataType == "float32") {
                        float val;
                        memcpy(&val, data + offset + field.byteOffset, 4);
                        pkt.properties[field.name] = QString::number(val);
                    } else if (field.dataType == "uint32") {
                        uint32_t val = qFromBigEndian<uint32_t>(data + offset + field.byteOffset);
                        pkt.properties[field.name] = QString::number(val);
                    } else {
                        uint8_t rawByte = data[offset + field.byteOffset];
                        uint8_t maskedVal = rawByte & field.bitMask;
                        pkt.properties[field.name] = field.valToStr.value(maskedVal, "UNKNOWN");
                    }
                }
            }
            // for (const PacketField& field : config.fields) {
            //     if (field.byteOffset < totalPacketLength) {
            //         uint8_t rawByte = data[offset + field.byteOffset];
            //         uint8_t maskedVal = rawByte & field.bitMask;
            //         pkt.properties[field.name] = field.valToStr.value(maskedVal, "UNKNOWN");
            //     }
            // }
        }
        else if (type == 0x00000000) {
            pkt.typeName = "String Packet";
            pkt.properties["Decoded ASCII"] = QString::fromLatin1(data + offset + 6, payloadLength).replace('\0', "");
        }
        // else if (type == 0x00000001 && payloadLength == 12) {
        //     pkt.typeName = "Sensor Array";
        //     float x, y, z;
        //     memcpy(&x, data + offset + 6, 4);
        //     memcpy(&y, data + offset + 10, 4);
        //     memcpy(&z, data + offset + 14, 4);
        //     pkt.properties["X Axis"] = QString::number(x);
        //     pkt.properties["Y Axis"] = QString::number(y);
        //     pkt.properties["Z Axis"] = QString::number(z);
        // }
        else if (type == 0x00000002) {
            pkt.typeName = "Timestamp";
            if (payloadLength == 4) {
                uint32_t epoch = qFromBigEndian<uint32_t>(data + offset + 6);
                pkt.properties["Linux Epoch"] = QString::number(epoch);
            } else if (payloadLength == 8) {
                uint64_t epoch = qFromBigEndian<uint64_t>(data + offset + 6);
                pkt.properties["Linux Epoch"] = QString::number(epoch);
            }
        }
        else {
            pkt.typeName = QString("Unknown (Type 0x%1)").arg(type, 8, 16, QChar('0'));
        }

        uint32_t embeddedCrc = qFromBigEndian<uint32_t>(data + offset + 6 + payloadLength);
        uint32_t calculatedCrc = calculateCrc32(data + offset, 6 + payloadLength);

        pkt.properties["CRC"] = QString("0x%1").arg(embeddedCrc, 8, 16, QChar('0')).toUpper();
        pkt.properties["Calculated CRC"] = QString("0x%1").arg(calculatedCrc, 8, 16, QChar('0')).toUpper();

        if (embeddedCrc != calculatedCrc) {
            pkt.properties["HasError"] = true;
            pkt.properties["ErrorMessage"] = "CRC Mismatch";
            m_hexModel->addErrorRange(pkt.startOffset, pkt.totalLength);
        }

        m_packets.append(pkt);
        offset += totalPacketLength;
    }

    if (offset + 4 <= size) {
        ParsedPacket crcPkt;
        crcPkt.startOffset = offset;
        crcPkt.totalLength = size - offset;
        crcPkt.typeName = "File CRC";

        if (crcPkt.totalLength == 4) {
            uint32_t fileCrc = qFromBigEndian<uint32_t>(data + offset);
            uint32_t calcFileCrc = calculateCrc32(data, offset);

            crcPkt.properties["CRC32"] = QString("0x%1").arg(fileCrc, 8, 16, QChar('0')).toUpper();
            crcPkt.properties["Calculated CRC"] = QString("0x%1").arg(calcFileCrc, 8, 16, QChar('0')).toUpper();

            if (fileCrc != calcFileCrc) {
                crcPkt.properties["HasError"] = true;
                crcPkt.properties["ErrorMessage"] = "File CRC Mismatch";
                m_hexModel->addErrorRange(crcPkt.startOffset, crcPkt.totalLength);
            }
        }

        m_packets.append(crcPkt);
    }
}

void HexController::selectOffset(int offset) {
    m_hexModel->setHighlightRange(-1, 0);

    m_properties.clear();
    m_options.clear();

    m_properties["Absolute Offset"] = QString::number(offset);
    m_properties["Hex Address"] = QString("0x%1").arg(offset, 8, 16, QChar('0')).toUpper();

    bool found = false;
    for (const auto& pkt : m_packets) {
        if (offset >= pkt.startOffset && offset < (pkt.startOffset + pkt.totalLength)) {
            m_properties["Struct"] = pkt.typeName;
            m_properties["PacketStartOffset"] = pkt.startOffset;

            for (auto it = pkt.properties.constBegin(); it != pkt.properties.constEnd(); ++it) {
                m_properties[it.key()] = it.value();
            }

            const QByteArray& buffer = m_hexModel->buffer();
            if (pkt.startOffset + 4 <= buffer.size()) {
                uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + pkt.startOffset);
                if (m_packetConfigs.contains(type)) {
                    for (const PacketField& field : m_packetConfigs[type].fields) {
                        if (field.dataType == "enum") {
                            QVariantList safeOptions;
                            for (const QString& key : field.strToVal.keys()) {
                                safeOptions.append(key);
                            }
                            m_options[field.name] = safeOptions;
                        } else {
                            // Register key with empty options so Loader recognizes it as a valid config field
                            m_options[field.name] = QVariantList();
                        }
                    }
                }
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

void HexController::selectPropertyBytes(const QString& propertyName) {
    if (!m_properties.contains("PacketStartOffset")) return;
    int startOffset = m_properties["PacketStartOffset"].toInt();

    const QByteArray& buffer = m_hexModel->buffer();
    if (startOffset >= buffer.size()) return;

    if (propertyName == "Hex Address" || propertyName == "Absolute Offset" || propertyName == "Struct") {
        m_hexModel->setHighlightRange(startOffset, qMin(6, static_cast<int>(buffer.size()) - startOffset));
        return;
    }

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + startOffset);
    uint16_t length = (startOffset + 6 <= buffer.size()) ? qFromBigEndian<uint16_t>(buffer.constData() + startOffset + 4) : 0;

    if (m_packetConfigs.contains(type)) {
        for (const PacketField& field : m_packetConfigs[type].fields) {
            if (field.name == propertyName) {
                // Flexible size check for 4-byte types (floats, 32-bit ints, etc.)
                int fieldSize = 1;
                QString dt = field.dataType.toLower();
                if (dt.contains("float") || dt.contains("32") || dt == "uint32" || dt == "int32") {
                    fieldSize = 4;
                }
                m_hexModel->setHighlightRange(startOffset + field.byteOffset, fieldSize);
                return;
            }
        }
    }

    if (propertyName == "CRC" || propertyName == "Calculated CRC") {
        m_hexModel->setHighlightRange(startOffset + 6 + length, 4);
        return;
    }

    if (propertyName == "Decoded ASCII" || propertyName == "Linux Epoch") {
        m_hexModel->setHighlightRange(startOffset + 6, length);
        return;
    }

    auto it = std::find_if(m_packets.begin(), m_packets.end(), [&](const ParsedPacket& p) {
        return p.startOffset == startOffset;
    });

    if (it != m_packets.end()) {
        m_hexModel->setHighlightRange(it->startOffset, it->totalLength);
    }
}

void HexController::applyStagedChanges(int packetStartOffset, const QVariantMap& changes) {
    if (packetStartOffset < 32 || changes.isEmpty()) return;

    auto it = std::find_if(m_packets.begin(), m_packets.end(), [&](const ParsedPacket& p) {
        return p.startOffset == packetStartOffset;
    });

    if (it == m_packets.end()) return;

    const QByteArray& buffer = m_hexModel->buffer();
    if (packetStartOffset + 4 > buffer.size()) return;

    uint32_t packetType = qFromBigEndian<uint32_t>(buffer.constData() + packetStartOffset);

    if (m_packetConfigs.contains(packetType)) {
        const PacketConfig& config = m_packetConfigs[packetType];

        for (auto itMap = changes.constBegin(); itMap != changes.constEnd(); ++itMap) {
            QString fieldName = itMap.key();
            QString newValue = itMap.value().toString();

            for (const PacketField& field : config.fields) {
                if (field.name == fieldName) {
                    int absoluteOffset = packetStartOffset + field.byteOffset;

                    if (field.dataType == "float32") {
                        bool ok = false;
                        float fVal = newValue.toFloat(&ok);
                        if (ok && absoluteOffset + 4 <= buffer.size()) {
                            QByteArray replacement(4, 0);
                            memcpy(replacement.data(), &fVal, 4);
                            m_hexModel->replaceBytes(absoluteOffset, 4, replacement);
                        }
                    }
                    else if (field.dataType == "uint32") {
                        bool ok = false;
                        uint32_t uVal = newValue.toUInt(&ok);
                        if (ok && absoluteOffset + 4 <= buffer.size()) {
                            QByteArray replacement(4, 0);
                            qToBigEndian<uint32_t>(uVal, replacement.data());
                            m_hexModel->replaceBytes(absoluteOffset, 4, replacement);
                        }
                    }
                    else if (field.dataType == "enum") {
                        if (field.strToVal.contains(newValue)) {
                            uint8_t newBits = field.strToVal[newValue];
                            if (absoluteOffset < buffer.size()) {
                                uint8_t currentByte = buffer.at(absoluteOffset);
                                currentByte &= ~field.bitMask;
                                currentByte |= (newBits & field.bitMask);

                                QByteArray replacement;
                                replacement.append(currentByte);
                                m_hexModel->replaceBytes(absoluteOffset, 1, replacement);
                            }
                        }
                    }
                }
            }
        }
    }
    else if (packetType == 0x00000000 && changes.contains("Decoded ASCII")) {
        QString newText = changes["Decoded ASCII"].toString();
        QByteArray newPayload = newText.toLatin1();
        if (newPayload.size() <= 65535) {
            QByteArray newPacketData;
            newPacketData.resize(10 + newPayload.size());
            char* data = newPacketData.data();
            qToBigEndian<uint32_t>(packetType, data);
            qToBigEndian<uint16_t>(newPayload.size(), data + 4);
            memcpy(data + 6, newPayload.constData(), newPayload.size());

            uint32_t crc = calculateCrc32(newPacketData.constData(), newPacketData.size() - 4);
            qToBigEndian<uint32_t>(crc, newPacketData.data() + newPacketData.size() - 4);
            m_hexModel->replaceBytes(packetStartOffset, it->totalLength, newPacketData);
        }
    }

    // Recalculate packet-level CRC
    uint16_t pktLen = qFromBigEndian<uint16_t>(m_hexModel->buffer().constData() + packetStartOffset + 4);
    if (packetStartOffset + 6 + pktLen + 4 <= m_hexModel->buffer().size()) {
        uint32_t crc = calculateCrc32(m_hexModel->buffer().constData() + packetStartOffset, 6 + pktLen);
        QByteArray crcBytes(4, 0);
        qToBigEndian<uint32_t>(crc, crcBytes.data());
        m_hexModel->replaceBytes(packetStartOffset + 6 + pktLen, 4, crcBytes);
    }

    // Recalculate global file CRC at the very end of the buffer
    const QByteArray& updatedBuffer = m_hexModel->buffer();
    if (updatedBuffer.size() >= 4) {
        uint32_t fileCrc = calculateCrc32(updatedBuffer.constData(), updatedBuffer.size() - 4);
        QByteArray crcBytes(4, 0);
        qToBigEndian<uint32_t>(fileCrc, crcBytes.data());
        m_hexModel->replaceBytes(updatedBuffer.size() - 4, 4, crcBytes);
    }

    parseCurrentBuffer();
    selectOffset(packetStartOffset);
}

bool HexController::isConfigEditable(const QString &propertyName) const {
    if (!m_properties.contains("PacketStartOffset")) return false;
    int startOffset = m_properties["PacketStartOffset"].toInt();

    const QByteArray& buffer = m_hexModel->buffer();
    if (startOffset + 4 > buffer.size()) return false;

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + startOffset);
    if (m_packetConfigs.contains(type)) {
        for (const PacketField& field : m_packetConfigs[type].fields) {
            if (field.name == propertyName) return true;
        }
    }
    return false;
}
