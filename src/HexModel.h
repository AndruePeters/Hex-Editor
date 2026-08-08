#pragma once
#include <QAbstractTableModel>
#include <QByteArray>
#include <QUrl>
#include <vector>

struct PacketError {
    int startOffset;
    int length;
    QString message;
};

struct PacketBounds {
    int startOffset;
    int length;
};

class HexModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Roles {
        DisplayRole = Qt::DisplayRole,
        IsErrorRole = Qt::UserRole + 1,
        IsSelectedRole = Qt::UserRole + 2,
        PacketIdRole = Qt::UserRole + 3,
        EdgeTopRole = Qt::UserRole + 4,
        EdgeBottomRole = Qt::UserRole + 5,
        EdgeLeftRole = Qt::UserRole + 6,
        EdgeRightRole = Qt::UserRole + 7,
        IsEditCursorRole = Qt::UserRole + 8
    };

    explicit HexModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool loadFile(const QString &filePath);
    Q_INVOKABLE bool saveFile(const QString &filePath);
    
    void setErrorRanges(const std::vector<PacketError> &errors);
    void setPacketBounds(const std::vector<PacketBounds> &bounds);
    Q_INVOKABLE void setSelectionRange(int startOffset, int length);
    Q_INVOKABLE void setEditSelection(int startOffset, int length);
    
    void replaceBytes(int offset, int removeLength, const QByteArray& insertBytes);
    QByteArray& getBufferRef();
    
    bool isErrorByte(int offset) const;
    const QByteArray& buffer() const { return m_buffer; }

private:
    int getPacketId(int offset) const;

    QByteArray m_buffer;
    std::vector<PacketError> m_errors;
    std::vector<PacketBounds> m_packetBounds;
    int m_selectedStart = -1;
    int m_selectedLength = 0;
    int m_editStart = -1;
    int m_editLength = 0;
};