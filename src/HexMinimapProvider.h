#pragma once
#include <QQuickImageProvider>
#include "HexModel.h"

class HexMinimapProvider : public QQuickImageProvider {
public:
    explicit HexMinimapProvider(HexModel* model);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    HexModel* m_model;
};
