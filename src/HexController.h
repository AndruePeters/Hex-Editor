#pragma once
#include "HexModel.h"

#include <QObject>
#include <QVariantMap>

#include <vector>


struct ParsedPacket {
    int startOffset;
    int totalLength;
    QString typeName;
    QVariantMap properties;
    bool hasError;
};

class HexController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap currentProperties READ currentProperties NOTIFY propertiesChanged)

public:
    explicit HexController(HexModel* hexModel, QObject* parent = nullptr);

    QVariantMap currentProperties() const;
    
    Q_INVOKABLE void updateStringPayload(int packetStartOffset, const QString& newText); 
    Q_INVOKABLE void selectOffset(int offset);
    Q_INVOKABLE void parseCurrentBuffer();

signals:
    void propertiesChanged();

private:
    uint32_t calculateCrc32(const char* data, size_t length);

    QVariantMap m_properties;
    HexModel* m_hexModel;
    std::vector<ParsedPacket> m_packets;
};
