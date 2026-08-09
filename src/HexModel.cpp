#include "HexModel.h"
#include <QFile>
#include <QUrl>

HexModel::HexModel(QObject* parent) : QAbstractListModel(parent) {}

int HexModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_buffer.size();
}

QVariant HexModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_buffer.size() || index.row() < 0) {
        return QVariant();
    }

    int row = index.row();

    switch (role) {
        case Qt::DisplayRole:
        case HexRole:
            return getHexByte(row);
        case AsciiRole:
            return getAsciiChar(row);
        case IsErrorRole:
            return isErrorByte(row);
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> HexModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[HexRole] = "hex";
    roles[AsciiRole] = "ascii";
    roles[IsErrorRole] = "isError";
    return roles;
}

void HexModel::loadFile(const QString& filePath) {
    QString path = filePath;
    if (path.startsWith("file://")) {
        path = QUrl(path).toLocalFile();
    }

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        beginResetModel();
        m_buffer = file.readAll();
        m_errorMap.resize(m_buffer.size());
        m_errorMap.fill(false);
        endResetModel();

        emit bufferChanged();
    }
}

void HexModel::saveFile(const QString& filePath) {
    QString path = filePath;
    if (path.startsWith("file://")) {
        path = QUrl(path).toLocalFile();
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(m_buffer);
    }
}

QString HexModel::getHexByte(int index) const {
    if (index < 0 || index >= m_buffer.size()) return "";
    return QString("%1").arg(static_cast<uint8_t>(m_buffer.at(index)), 2, 16, QChar('0')).toUpper();
}

QString HexModel::getAsciiChar(int index) const {
    if (index < 0 || index >= m_buffer.size()) return "";
    char c = m_buffer.at(index);
    return (c >= 32 && c <= 126) ? QString(c) : ".";
}

void HexModel::setSelectionRange(int offset, int length) {
    if (m_selectionOffset != offset || m_selectionLength != length) {
        m_selectionOffset = offset;
        m_selectionLength = length;
        emit selectionChanged();
    }
}

void HexModel::setHighlightRange(int offset, int length) {
    if (m_highlightOffset != offset || m_highlightLength != length) {
        m_highlightOffset = offset;
        m_highlightLength = length;
        emit highlightChanged();
    }
}

void HexModel::replaceBytes(int offset, int length, const QByteArray& newBytes) {
    m_buffer.replace(offset, length, newBytes);
    m_errorMap.resize(m_buffer.size());

    emit dataChanged(createIndex(offset, 0), createIndex(offset + length - 1, 0));
    emit bufferChanged();
}

bool HexModel::isErrorByte(int index) const {
    if (index >= 0 && index < m_errorMap.size()) {
        return m_errorMap.at(index);
    }
    return false;
}

void HexModel::clearErrorBytes() {
    m_errorMap.fill(false, m_buffer.size());
    if (m_buffer.size() > 0) {
        emit dataChanged(createIndex(0, 0), createIndex(m_buffer.size() - 1, 0), {IsErrorRole});
    }
}

void HexModel::addErrorRange(int offset, int length) {
    if (m_errorMap.size() != m_buffer.size()) {
        m_errorMap.resize(m_buffer.size());
    }
    for (int i = offset; i < offset + length && i < m_errorMap.size(); ++i) {
        m_errorMap[i] = true;
    }
    emit dataChanged(createIndex(offset, 0), createIndex(offset + length - 1, 0), {IsErrorRole});
}

void HexModel::setEditSelection(int offset, int length) {
    setHighlightRange(offset, length);
}