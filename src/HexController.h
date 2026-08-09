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

struct EnumField {
    QString name;
    int byteOffset;
    uint8_t bitMask;
    QMap<uint8_t, QString> valToStr;
    QMap<QString, uint8_t> strToVal;
};

struct PacketConfig {
    QString name;
    QList<EnumField> fields;
};

class HexController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap currentProperties READ currentProperties NOTIFY propertiesChanged)
    Q_PROPERTY(QVariantMap propertyOptions READ propertyOptions NOTIFY propertiesChanged)

public:
    explicit HexController(HexModel* model);

    QVariantMap currentProperties() const { return m_properties; }
    QVariantMap propertyOptions() const { return m_options; }

    Q_INVOKABLE void loadConfiguration(const QString& configPath);
    Q_INVOKABLE void parseCurrentBuffer();
    Q_INVOKABLE void selectOffset(int offset);
    Q_INVOKABLE void selectPropertyBytes(const QString& propertyName);
    Q_INVOKABLE void applyStagedChanges(int packetStartOffset, const QVariantMap& changes);

    signals:
        void propertiesChanged();

private:
    uint32_t calculateCrc32(const char* data, int length);

    HexModel* m_hexModel;
    QVector<ParsedPacket> m_packets;
    QMap<uint32_t, PacketConfig> m_packetConfigs;

    QVariantMap m_properties;
    QVariantMap m_options;
};