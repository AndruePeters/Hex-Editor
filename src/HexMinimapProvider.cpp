#include "HexMinimapProvider.h"
#include <QImage>
#include <QColor>

HexMinimapProvider::HexMinimapProvider(HexModel* model) 
    : QQuickImageProvider(QQuickImageProvider::Image), m_model(model) {}

QImage HexMinimapProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    if (!m_model || m_model->buffer().isEmpty()) {
        QImage emptyImage(64, 1, QImage::Format_RGBA8888);
        emptyImage.fill(Qt::transparent);
        if (size) *size = emptyImage.size();
        return emptyImage;
    }

    const QByteArray& buffer = m_model->buffer();
    int totalBytes = buffer.size();
    int totalRows = (totalBytes + 15) / 16;
    int width = 64; 
    
    QImage image(width, totalRows, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    for (int row = 0; row < totalRows; ++row) {
    for (int col = 0; col < 16; ++col) {
        int byteOffset = (row * 16) + col;
        
        if (byteOffset >= totalBytes) break;

        quint8 byteVal = static_cast<quint8>(buffer.at(byteOffset));
        QColor pixelColor;
        
        // 1. Error state override
        if (m_model->isErrorByte(byteOffset)) {
            pixelColor = Qt::red;
        } 
        // 2. Null bytes (darkest)
        else if (byteVal == 0x00) {
            pixelColor = QColor(25, 25, 25);
        } 
        // 3. ASCII Printable text (Blue)
        else if (byteVal >= 0x20 && byteVal <= 0x7E) {
            pixelColor = QColor(50, 100, 150);
        } 
        // 4. Everything else (Grayscale based on byte value)
        else {
            int intensity = 40 + (byteVal * 120 / 255);
            pixelColor = QColor(intensity, intensity, intensity);
        }

        int xStart = col * (width / 16);
        for (int px = 0; px < (width / 16); ++px) {
            image.setPixelColor(xStart + px, row, pixelColor);
        }
    }
}

    if (size) *size = image.size();
    return image;
}
