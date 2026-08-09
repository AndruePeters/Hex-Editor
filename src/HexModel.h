#pragma once
#include <QAbstractListModel>
#include <QByteArray>
#include <QVector>
#include <QString>

class HexModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int size READ size NOTIFY bufferChanged)
    Q_PROPERTY(int selectionOffset READ selectionOffset NOTIFY selectionChanged)
    Q_PROPERTY(int selectionLength READ selectionLength NOTIFY selectionChanged)
    Q_PROPERTY(int highlightOffset READ highlightOffset NOTIFY highlightChanged)
    Q_PROPERTY(int highlightLength READ highlightLength NOTIFY highlightChanged)

public:
    enum HexRoles {
        HexRole = Qt::UserRole + 1,
        AsciiRole,
        IsErrorRole
    };

    explicit HexModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int size() const { return m_buffer.size(); }
    const QByteArray& buffer() const { return m_buffer; }
    // QByteArray& getBufferRef() { return m_buffer; }

    int selectionOffset() const { return m_selectionOffset; }
    int selectionLength() const { return m_selectionLength; }
    void setSelectionRange(int offset, int length);

    int highlightOffset() const { return m_highlightOffset; }
    int highlightLength() const { return m_highlightLength; }
    void setHighlightRange(int offset, int length);

    void replaceBytes(int offset, int length, const QByteArray& newBytes);

    bool isErrorByte(int index) const;
    void clearErrorBytes();
    void addErrorRange(int offset, int length);

    Q_INVOKABLE void loadFile(const QString& filePath);
    Q_INVOKABLE void saveFile(const QString& filePath);
    Q_INVOKABLE QString getHexByte(int index) const;
    Q_INVOKABLE QString getAsciiChar(int index) const;
    Q_INVOKABLE void setEditSelection(int offset, int length);

signals:
    void selectionChanged();
    void highlightChanged();
    void bufferChanged();

private:
    QByteArray m_buffer;
    QVector<bool> m_errorMap;
    int m_selectionOffset = -1;
    int m_selectionLength = 0;
    int m_highlightOffset = -1;
    int m_highlightLength = 0;
};