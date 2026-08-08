#include "HexModel.h"
#include <QFile>
#include <QFileInfo>

HexModel::HexModel(QObject *parent) : QAbstractTableModel(parent) {}

int HexModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || m_buffer.isEmpty()) return 0;
    return (m_buffer.size() + 15) / 16;
}

int HexModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return 16;
}

int HexModel::getPacketId(int offset) const {
    if (offset < 0 || offset >= m_buffer.size()) return -1;
    for (size_t i = 0; i < m_packetBounds.size(); ++i) {
        if (offset >= m_packetBounds[i].startOffset && offset < (m_packetBounds[i].startOffset + m_packetBounds[i].length)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}


QVariant HexModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    int offset = (index.row() * 16) + index.column();

    // Handle out-of-bounds cells (padding in the TableView)
    if (offset >= m_buffer.size()) {
        if (role == DisplayRole) return QString("");
        if (role == PacketIdRole) return -1;
        if (role == IsErrorRole || role == IsSelectedRole || role == IsEditCursorRole ||
            role == EdgeTopRole || role == EdgeBottomRole || role == EdgeLeftRole || role == EdgeRightRole) {
            return false;
        }
        return QVariant();
    }

    if (role == DisplayRole) {
        return QString("%1").arg(static_cast<quint8>(m_buffer.at(offset)), 2, 16, QChar('0')).toUpper();
    }
    if (role == IsErrorRole) return isErrorByte(offset);
    if (role == IsSelectedRole) return (offset >= m_selectedStart && offset < (m_selectedStart + m_selectedLength));
    if (role == IsEditCursorRole) return (offset >= m_editStart && offset < (m_editStart + m_editLength));
    
    int pktId = getPacketId(offset);
    if (role == PacketIdRole) return pktId;

    if (role == EdgeTopRole) return pktId != -1 ? (getPacketId(offset - 16) != pktId) : false;
    if (role == EdgeBottomRole) return pktId != -1 ? (getPacketId(offset + 16) != pktId) : false;
    if (role == EdgeLeftRole) return pktId != -1 ? (offset % 16 == 0 || getPacketId(offset - 1) != pktId) : false;
    if (role == EdgeRightRole) return pktId != -1 ? (offset % 16 == 15 || getPacketId(offset + 1) != pktId) : false;

    return QVariant();
}

QHash<int, QByteArray> HexModel::roleNames() const {
    return {
        {DisplayRole, "display"},
        {IsErrorRole, "isError"},
        {IsSelectedRole, "isSelected"},
        {PacketIdRole, "packetId"},
        {EdgeTopRole, "edgeTop"},
        {EdgeBottomRole, "edgeBottom"},
        {EdgeLeftRole, "edgeLeft"},
        {EdgeRightRole, "edgeRight"},
        {IsEditCursorRole, "isEditCursor"}
    };
}

bool HexModel::loadFile(const QString &filePath) {
    QString cleanPath = QUrl(filePath).toLocalFile();
    if (cleanPath.isEmpty()) cleanPath = filePath;

    QFile file(cleanPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    beginResetModel();
    m_buffer = file.readAll();
    m_packetBounds.clear();
    m_errors.clear();
    m_selectedStart = -1;
    m_selectedLength = 0;
    m_editStart = -1;
    m_editLength = 0;
    file.close();
    endResetModel();

    return true;
}

bool HexModel::saveFile(const QString &filePath) {
    QString cleanPath = QUrl(filePath).toLocalFile();
    if (cleanPath.isEmpty()) cleanPath = filePath;

    QFile file(cleanPath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    file.write(m_buffer);
    return true;
}

void HexModel::replaceBytes(int offset, int removeLength, const QByteArray& insertBytes) {
    if (offset < 0 || offset > m_buffer.size()) return;
    
    beginResetModel();
    // Safely remove the exact old packet bytes and insert the new packet bytes
    m_buffer.remove(offset, removeLength);
    m_buffer.insert(offset, insertBytes);
    endResetModel();
}

QByteArray& HexModel::getBufferRef() {
    return m_buffer;
}

void HexModel::setErrorRanges(const std::vector<PacketError> &errors) {
    beginResetModel();
    m_errors = errors;
    endResetModel();
}

void HexModel::setPacketBounds(const std::vector<PacketBounds> &bounds) {
    beginResetModel();
    m_packetBounds = bounds;
    endResetModel();
}

void HexModel::setSelectionRange(int startOffset, int length) {
    m_selectedStart = startOffset;
    m_selectedLength = length;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {IsSelectedRole});
}

void HexModel::setEditSelection(int startOffset, int length) {
    if (m_editStart == startOffset && m_editLength == length) return;

    int oldStart = m_editStart;
    int oldLength = m_editLength;

    m_editStart = startOffset;
    m_editLength = length;

    // Refresh old bounds (spanning full rows 0 to 15)
    if (oldStart != -1 && oldLength > 0 && oldStart < m_buffer.size()) {
        int oldEnd = std::min(oldStart + oldLength - 1, static_cast<int>(m_buffer.size()) - 1);
        QModelIndex topLeft = index(oldStart / 16, 0);
        QModelIndex bottomRight = index(oldEnd / 16, 15);
        emit dataChanged(topLeft, bottomRight, {IsEditCursorRole});
    }

    // Refresh new bounds (spanning full rows 0 to 15)
    if (m_editStart != -1 && m_editLength > 0 && m_editStart < m_buffer.size()) {
        int newEnd = std::min(m_editStart + m_editLength - 1, static_cast<int>(m_buffer.size()) - 1);
        QModelIndex topLeft = index(m_editStart / 16, 0);
        QModelIndex bottomRight = index(newEnd / 16, 15);
        emit dataChanged(topLeft, bottomRight, {IsEditCursorRole});
    }
}

bool HexModel::isErrorByte(int offset) const {
    for (const auto &err : m_errors) {
        if (offset >= err.startOffset && offset < (err.startOffset + err.length)) return true;
    }
    return false;
}