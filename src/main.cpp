#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickStyle>
#include <memory>

#include "HexModel.h"
#include "HexController.h"
#include "HexMinimapProvider.h"

int main(int argc, char *argv[]) {
    using Qt::StringLiterals::operator ""_s;
    QGuiApplication app(argc, argv);
    
    // Force the Fusion style to prevent Breeze theme engine crashes
    QQuickStyle::setStyle("Fusion");

    QQmlApplicationEngine engine;

    HexModel hexModel;
    HexController hexController(&hexModel);
    
    auto minimapProvider = std::make_unique<HexMinimapProvider>(&hexModel);
    engine.addImageProvider(QLatin1String("hexminimap"), minimapProvider.release());

    qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexModelInst", &hexModel);
    qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexControllerInst", &hexController);

    const QUrl url(u"qrc:/qt/qml/bite/qml/main.qml"_s);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    
    engine.load(url);

    return app.exec();
}