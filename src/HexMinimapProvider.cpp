#include "HexMinimapProvider.h"
#include <QImage>
#include <QColor>

HexMinimapProvider::HexMinimapProvider(HexModel* model) 
    : QQuickImageProvider(QQuickImageProvider::Image), m_model(model) {}

// Relative luminance formula (WCAG standard)
double getLuminance(int r, int g, int b) {
    auto normalize = [](int value) {
        double v = value / 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * normalize(r) + 0.7152 * normalize(g) + 0.0722 * normalize(b);
}

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

    QImage result = image.convertToFormat(QImage::Format_ARGB32);

    for (int y = 0; y < result.height(); ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb pixel = scanLine[x];
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            int a = qAlpha(pixel);

            if (getLuminance(r, g, b) < 0.15 && a > 0) {
                r = std::min(255, r + 80);
                g = std::min(255, g + 80);
                b = std::min(255, b + 80);
                scanLine[x] = qRgba(r, g, b, a);
            }
        }
    }

    return result;
}
