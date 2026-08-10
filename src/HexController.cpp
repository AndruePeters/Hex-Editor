#include "HexController.h"
#include "HexModel.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtEndian>
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>

int getBitShift(uint32_t mask) {
    if (mask == 0) return 0;
    int shift = 0;
    while ((mask & 1) == 0) {
        mask >>= 1;
        shift++;
    }
    return shift;
}

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

            if (field.dataType == "enum") {

                if (fieldObj.contains("bitMask")) {
                    QJsonValue maskVal = fieldObj["bitMask"];
                    if (maskVal.isString()) {
                        field.bitMask = maskVal.toString().toUInt(nullptr, 0);
                    } else {
                        field.bitMask = maskVal.toInt();
                    }
                } else {
                    field.bitMask = 0xFF; // Default full byte mask
                }
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
    m_baseColorMap.clear();

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

    for (int i = 0; i < 32; ++i) {
        m_baseColorMap.insert(i, "#455A64"); // Dark boundary for header
    }

    int offset = 32;
    bool alternatePacketColor = false;

    while (offset + 10 <= size - 4) {
        uint32_t type = qFromBigEndian<uint32_t>(data + offset);
        uint16_t payloadLength = qFromBigEndian<uint16_t>(data + offset + 4);

        int totalPacketLength = 10 + payloadLength;
        if (offset + totalPacketLength > size - 4) break;

        ParsedPacket pkt;
        pkt.startOffset = offset;
        pkt.totalLength = totalPacketLength;

        // Alternate packet background colors for visual differentiation
        QString basePktColor = alternatePacketColor ? "#424242" : "#212121";
        alternatePacketColor = !alternatePacketColor;

        for (int i = 0; i < totalPacketLength; ++i) {
            if (offset + i < size) {
                m_baseColorMap.insert(offset + i, basePktColor);
            }
        }

        if (m_packetConfigs.contains(type)) {
            const PacketConfig& config = m_packetConfigs[type];
            pkt.typeName = config.name;

            for (const PacketField& field : config.fields) {
                if (field.dataType == "float32" && field.byteOffset + 4 <= totalPacketLength) {
                    float val;
                    memcpy(&val, data + offset + field.byteOffset, 4);
                    pkt.properties[field.name] = QString::number(val);
                }
                else if (field.dataType == "uint32" && field.byteOffset + 4 <= totalPacketLength) {
                    uint32_t val = qFromBigEndian<uint32_t>(data + offset + field.byteOffset);
                    pkt.properties[field.name] = QString::number(val);
                }
                else if (field.dataType == "epoch32" && field.byteOffset + 4 <= totalPacketLength) {
                    uint32_t epoch = qFromBigEndian<uint32_t>(data + offset + field.byteOffset);
                    QDateTime dt = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
                    pkt.properties[field.name] = dt.toString("yyyy-MM-dd HH:mm:ss UTC");
                }
                else if (field.dataType == "epoch64" && field.byteOffset + 8 <= totalPacketLength) {
                    uint64_t epoch = qFromBigEndian<uint64_t>(data + offset + field.byteOffset);
                    QDateTime dt = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::UTC);
                    pkt.properties[field.name] = dt.toString("yyyy-MM-dd HH:mm:ss UTC");
                }
                else if (field.dataType == "enum" && field.byteOffset < totalPacketLength) {
                    uint8_t rawByte = data[offset + field.byteOffset];
                    uint32_t mask = field.bitMask;
                    int shift = getBitShift(mask);
                    uint32_t maskedVal = (rawByte & mask) >> shift;
                    pkt.properties[field.name] = field.valToStr.value(maskedVal, "UNKNOWN");
                }
                else if (field.dataType == "string") {
                    int stringLen = payloadLength - (field.byteOffset - 6);
                    if (stringLen > 0 && field.byteOffset <= payloadLength) {
                        pkt.properties[field.name] = QString::fromLatin1(data + offset + field.byteOffset, stringLen).replace('\0', "");
                    }
                }
            }
        } else {
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

        for (int i = offset; i < size; ++i) {
            m_baseColorMap.insert(i, "#151515");
        }

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

    // Apply the unselected packet grid colors
    m_hexModel->setSectionColors(m_baseColorMap);
}

void HexController::selectOffset(int offset) {
    m_hexModel->setHighlightRange(-1, 0);

    m_properties.clear();
    m_options.clear();

    m_properties["Absolute Offset"] = QString::number(offset);
    m_properties["Hex Address"] = QString("0x%1").arg(offset, 8, 16, QChar('0')).toUpper();

    // Start with the base grid map
    QHash<int, QString> activeColorMap = m_baseColorMap;
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
                uint16_t payloadLength = (pkt.startOffset + 6 <= buffer.size()) ? qFromBigEndian<uint16_t>(buffer.constData() + pkt.startOffset + 4) : 0;

                if (m_packetConfigs.contains(type)) {
                    const QStringList colors = {"#1A4A76", "#256296", "#307BB5", "#4793C4", "#61ABD1"};
                    int colorIdx = 0;

                    // Overlay field structure colors exclusively for the selected packet
                    for (const PacketField& field : m_packetConfigs[type].fields) {
                        int fieldSize = 1;
                        QString dt = field.dataType.toLower();
                        if (dt == "string") {
                            fieldSize = payloadLength - (field.byteOffset - 6);
                            if (fieldSize < 1) fieldSize = 1;
                        } else if (dt.contains("float") || dt.contains("32") || dt == "uint32" || dt == "int32" || dt == "epoch32") {
                            fieldSize = 4;
                        } else if (dt.contains("64") || dt == "epoch64") {
                            fieldSize = 8;
                        }

                        QString color = colors[colorIdx % colors.size()];
                        for (int i = 0; i < fieldSize; ++i) {
                            if (pkt.startOffset + field.byteOffset + i < buffer.size()) {
                                activeColorMap.insert(pkt.startOffset + field.byteOffset + i, color);
                            }
                        }
                        colorIdx++;
                    }

                    // Overlay CRC distinct color for the selected packet
                    for (int i = 0; i < 4; ++i) {
                        if (pkt.startOffset + 6 + payloadLength + i < buffer.size()) {
                            activeColorMap.insert(pkt.startOffset + 6 + payloadLength + i, "#3A1A1A");
                        }
                    }

                    for (const PacketField& field : m_packetConfigs[type].fields) {
                        if (field.dataType == "enum") {
                            QVariantList safeOptions;
                            for (const QString& key : field.strToVal.keys()) {
                                safeOptions.append(key);
                            }
                            m_options[field.name] = safeOptions;
                        } else {
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

    // Apply the updated map containing the base grid and the active packet overlay
    m_hexModel->setSectionColors(activeColorMap);

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
                int fieldSize = 1;
                QString dt = field.dataType.toLower();
                if (dt == "string") {
                    fieldSize = length - (field.byteOffset - 6);
                } else if (dt.contains("float") || dt.contains("32") || dt == "uint32" || dt == "int32" || dt == "epoch32") {
                    fieldSize = 4;
                } else if (dt.contains("64") || dt == "epoch64") {
                    fieldSize = 8;
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

    auto it = std::find_if(m_packets.begin(), m_packets.end(), [&](const ParsedPacket& p) {
        return p.startOffset == startOffset;
    });

    if (it != m_packets.end()) {
        m_hexModel->setHighlightRange(it->startOffset, it->totalLength);
    }
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

void HexController::applyStagedChanges(int packetStartOffset, const QVariantMap& changes) {
    if (packetStartOffset < 32 || changes.isEmpty()) return;

    auto it = std::find_if(m_packets.begin(), m_packets.end(), [&](const ParsedPacket& p) {
        return p.startOffset == packetStartOffset;
    });

    if (it == m_packets.end()) return;

    int oldTotalLength = it->totalLength;
    const QByteArray& buffer = m_hexModel->buffer();
    if (packetStartOffset + 4 > buffer.size()) return;

    uint32_t packetType = qFromBigEndian<uint32_t>(buffer.constData() + packetStartOffset);
    bool sizeChanged = false;
    QByteArray newPacketData;

    if (m_packetConfigs.contains(packetType)) {
        const PacketConfig& config = m_packetConfigs[packetType];

        for (auto itMap = changes.constBegin(); itMap != changes.constEnd(); ++itMap) {
            QString fieldName = itMap.key();
            QString newValue = itMap.value().toString();

            for (const PacketField& field : config.fields) {
                if (field.name == fieldName) {
                    int absoluteOffset = packetStartOffset + field.byteOffset;

                    if (field.dataType == "string") {
                        QByteArray newPayload = newValue.toLatin1();
                        if (newPayload.size() <= 65535) {
                            newPacketData.resize(10 + newPayload.size());
                            char* pData = newPacketData.data();
                            qToBigEndian<uint32_t>(packetType, pData);
                            qToBigEndian<uint16_t>(newPayload.size(), pData + 4);
                            memcpy(pData + 6, newPayload.constData(), newPayload.size());
                            sizeChanged = true;
                        }
                    }
                    else if (field.dataType == "float32") {
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
                    else if (field.dataType == "epoch32") {
                        bool ok = false;
                        uint32_t uVal = newValue.toUInt(&ok);
                        if (!ok) {
                            QDateTime dt = QDateTime::fromString(newValue, "yyyy-MM-dd HH:mm:ss UTC");
                            if (!dt.isValid()) dt = QDateTime::fromString(newValue, Qt::ISODate);
                            if (dt.isValid()) {
                                uVal = dt.toSecsSinceEpoch();
                                ok = true;
                            }
                        }
                        if (ok && absoluteOffset + 4 <= buffer.size()) {
                            QByteArray replacement(4, 0);
                            qToBigEndian<uint32_t>(uVal, replacement.data());
                            m_hexModel->replaceBytes(absoluteOffset, 4, replacement);
                        }
                    }
                    else if (field.dataType == "epoch64") {
                        bool ok = false;
                        uint64_t uVal = newValue.toULongLong(&ok);
                        if (!ok) {
                            QDateTime dt = QDateTime::fromString(newValue, "yyyy-MM-dd HH:mm:ss UTC");
                            if (!dt.isValid()) dt = QDateTime::fromString(newValue, Qt::ISODate);
                            if (dt.isValid()) {
                                uVal = dt.toSecsSinceEpoch();
                                ok = true;
                            }
                        }
                        if (ok && absoluteOffset + 8 <= buffer.size()) {
                            QByteArray replacement(8, 0);
                            qToBigEndian<uint64_t>(uVal, replacement.data());
                            m_hexModel->replaceBytes(absoluteOffset, 8, replacement);
                        }
                    }
                    else if (field.dataType == "enum") {
                        if (field.strToVal.contains(newValue)) {
                            uint32_t rawVal = field.strToVal[newValue];
                            uint32_t mask = field.bitMask;
                            int shift = getBitShift(mask);
                            uint32_t shiftedVal = rawVal << shift;

                            if (absoluteOffset < buffer.size()) {
                                uint8_t currentByte = buffer.at(absoluteOffset);
                                currentByte &= ~mask;                           // Clear only the bits for this field
                                currentByte |= (shiftedVal & mask);             // Merge the new shifted value

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

    if (sizeChanged) {
        uint32_t crc = calculateCrc32(newPacketData.constData(), newPacketData.size() - 4);
        qToBigEndian<uint32_t>(crc, newPacketData.data() + newPacketData.size() - 4);
        m_hexModel->replaceBytes(packetStartOffset, oldTotalLength, newPacketData);
    } else {
        uint16_t pktLen = qFromBigEndian<uint16_t>(m_hexModel->buffer().constData() + packetStartOffset + 4);
        if (packetStartOffset + 6 + pktLen + 4 <= m_hexModel->buffer().size()) {
            uint32_t crc = calculateCrc32(m_hexModel->buffer().constData() + packetStartOffset, 6 + pktLen);
            QByteArray crcBytes(4, 0);
            qToBigEndian<uint32_t>(crc, crcBytes.data());
            m_hexModel->replaceBytes(packetStartOffset + 6 + pktLen, 4, crcBytes);
        }
    }

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

bool HexController::isStringField(const QString& propertyName) const {
    if (!m_properties.contains("PacketStartOffset")) return false;
    int startOffset = m_properties["PacketStartOffset"].toInt();
    const QByteArray& buffer = m_hexModel->buffer();
    if (startOffset + 4 > buffer.size()) return false;

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + startOffset);
    if (m_packetConfigs.contains(type)) {
        for (const PacketField& field : m_packetConfigs[type].fields) {
            if (field.name == propertyName && field.dataType == "string") return true;
        }
    }
    return false;
}

bool HexController::currentPacketHasString() const {
    if (!m_properties.contains("PacketStartOffset")) return false;
    int startOffset = m_properties["PacketStartOffset"].toInt();
    const QByteArray& buffer = m_hexModel->buffer();
    if (startOffset + 4 > buffer.size()) return false;

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + startOffset);
    if (m_packetConfigs.contains(type)) {
        for (const PacketField& field : m_packetConfigs[type].fields) {
            if (field.dataType == "string") return true;
        }
    }
    return false;
}

void HexController::selectStringCharacter(int charIndex) {
    if (!m_properties.contains("PacketStartOffset")) return;
    int startOffset = m_properties["PacketStartOffset"].toInt();
    const QByteArray& buffer = m_hexModel->buffer();
    if (startOffset + 4 > buffer.size()) return;

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + startOffset);
    if (m_packetConfigs.contains(type)) {
        for (const PacketField& field : m_packetConfigs[type].fields) {
            if (field.dataType == "string") {
                QString currentVal = m_properties.value(field.name).toString();
                if (charIndex >= currentVal.length() && currentVal.length() > 0) {
                    charIndex = currentVal.length() - 1;
                }
                int targetOffset = startOffset + field.byteOffset + charIndex;
                if (targetOffset < buffer.size()) {
                    m_hexModel->setHighlightRange(targetOffset, 1);
                }
                return;
            }
        }
    }
}

void HexController::selectStringRange(int startCharIndex, int endCharIndex) {
    if (!m_properties.contains("PacketStartOffset")) return;
    int startOffset = m_properties["PacketStartOffset"].toInt();
    const QByteArray& buffer = m_hexModel->buffer();
    if (startOffset + 4 > buffer.size()) return;

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + startOffset);
    if (m_packetConfigs.contains(type)) {
        for (const PacketField& field : m_packetConfigs[type].fields) {
            if (field.dataType == "string") {
                QString currentVal = m_properties.value(field.name).toString();

                if (startCharIndex < 0) startCharIndex = 0;
                if (endCharIndex > currentVal.length()) endCharIndex = currentVal.length();
                if (startCharIndex > endCharIndex) std::swap(startCharIndex, endCharIndex);

                int length = endCharIndex - startCharIndex;
                if (length <= 0) length = 1;

                int targetOffset = startOffset + field.byteOffset + startCharIndex;
                if (targetOffset < buffer.size()) {
                    m_hexModel->setHighlightRange(targetOffset, qMin(length, buffer.size() - targetOffset));
                }
                return;
            }
        }
    }
}

void HexController::updatePacketStructuralColors(int packetStartOffset) {
    if (!m_hexModel) return;

    QHash<int, QString> colorMap;
    const QByteArray& buffer = m_hexModel->buffer();
    if (packetStartOffset + 4 > buffer.size()) {
        m_hexModel->clearSectionColors();
        return;
    }

    uint32_t type = qFromBigEndian<uint32_t>(buffer.constData() + packetStartOffset);
    uint16_t payloadLength = (packetStartOffset + 6 <= buffer.size()) ? qFromBigEndian<uint16_t>(buffer.constData() + packetStartOffset + 4) : 0;

    if (m_packetConfigs.contains(type)) {
        const PacketConfig& config = m_packetConfigs[type];

        // Define a distinct structure palette (semi-transparent or dark-theme compatible)
        const QStringList paletteColors = {"#1A3644", "#204659", "#26576E", "#2C6784", "#327899"};
        int colorIdx = 0;

        for (const PacketField& field : config.fields) {
            int fieldSize = 1;
            QString dt = field.dataType.toLower();
            if (dt == "string") {
                fieldSize = payloadLength - (field.byteOffset - 6);
                if (fieldSize < 0) fieldSize = 1;
            } else if (dt.contains("float") || dt.contains("32") || dt == "uint32" || dt == "int32" || dt == "epoch32") {
                fieldSize = 4;
            } else if (dt.contains("64") || dt == "epoch64") {
                fieldSize = 8;
            }

            QString assignedColor = paletteColors[colorIdx % paletteColors.size()];
            for (int i = 0; i < fieldSize; ++i) {
                colorMap.insert(packetStartOffset + field.byteOffset + i, assignedColor);
            }
            colorIdx++;
        }
    }

    m_hexModel->setSectionColors(colorMap);
}

