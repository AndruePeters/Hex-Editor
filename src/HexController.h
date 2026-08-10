#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVector>
#include <QString>
#include <QMap>

class HexModel;

struct ParsedPacket {
    int startOffset;
    int totalLength;
    QString typeName;
    QVariantMap properties;
};

struct PacketField {
    QString name;
    int byteOffset;
    QString  dataType; // "enum", "float32", "uint32", etc
    uint8_t bitMask = 0xFF;
    QMap<uint8_t, QString> valToStr;
    QMap<QString, uint8_t> strToVal;
};

struct PacketConfig {
    QString name;
    QList<PacketField> fields;
};

class HexController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap currentProperties READ currentProperties NOTIFY propertiesChanged)
    Q_PROPERTY(QVariantMap propertyOptions READ propertyOptions NOTIFY propertiesChanged)
    Q_PROPERTY(bool currentPacketHasString READ currentPacketHasString NOTIFY propertiesChanged) // Add this property
public:
    explicit HexController(HexModel* model);

    QVariantMap currentProperties() const { return m_properties; }
    QVariantMap propertyOptions() const { return m_options; }

    Q_INVOKABLE void loadConfiguration(const QString& configPath);
    Q_INVOKABLE void parseCurrentBuffer();
    Q_INVOKABLE void selectOffset(int offset);
    Q_INVOKABLE void selectPropertyBytes(const QString& propertyName);
    Q_INVOKABLE void applyStagedChanges(int packetStartOffset, const QVariantMap& changes);
    Q_INVOKABLE bool isConfigEditable(const QString& propertyName) const;
    bool currentPacketHasString() const;
    Q_INVOKABLE void selectStringCharacter(int charIndex);
    Q_INVOKABLE bool isStringField(const QString& propertyName) const;
    Q_INVOKABLE void selectStringRange(int startCharIndex, int endCharIndex);

    void updatePacketStructuralColors(int packetStartOffset);

    signals:
        void propertiesChanged();

private:
    uint32_t calculateCrc32(const char* data, int length);

    HexModel* m_hexModel;
    QVector<ParsedPacket> m_packets;
    QMap<uint32_t, PacketConfig> m_packetConfigs;

    QVariantMap m_properties;
    QVariantMap m_options;
    QHash<int, QString> m_baseColorMap;
};