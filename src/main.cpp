#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle> // Required for QQuickStyle
#include "HexController.h"
#include "HexModel.h"
#include "HexMinimapProvider.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Force a standard style to bypass the Breeze theme bug
    QQuickStyle::setStyle("Fusion"); // Valid options: "Basic", "Fusion", "Material", "Universal"

    qmlRegisterUncreatableType<HexModel>("AppBackend", 1, 0, "HexModel", "Backend model is a singleton");

    HexModel hexModel;
    HexController hexController(&hexModel);

    hexController.loadConfiguration(":/resources/protocols.json");

    QQmlApplicationEngine engine;

    qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexModelInst", &hexModel);
    qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexControllerInst", &hexController);

    engine.addImageProvider("hexminimap", new HexMinimapProvider(&hexModel));

    const QUrl url(QStringLiteral("qrc:/qt/qml/bite/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}

// #include <QGuiApplication>
// #include <QQmlApplicationEngine>
// #include "HexController.h"
// #include "HexModel.h"
//
// int main(int argc, char *argv[]) {
//     QGuiApplication app(argc, argv);
//
//     // Stack allocation
//     HexModel hexModel;
//     HexController hexController(&hexModel);
//
//     hexController.loadConfiguration(":/resources/protocols.json");
//
//     QQmlApplicationEngine engine;
//
//     // Pass the addresses of the stack-allocated instances to QML
//     qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexModelInst", &hexModel);
//     qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexControllerInst", &hexController);
//
//     engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
//     if (engine.rootObjects().isEmpty())
//         return -1;
//
//     return app.exec();
// }

// #include <QGuiApplication>
// #include <QQmlApplicationEngine>
// #include <QQmlEngine>
// #include <QQuickStyle>
// #include <memory>
//
// #include "HexModel.h"
// #include "HexController.h"
// #include "HexMinimapProvider.h"
//
// int main(int argc, char *argv[]) {
//     using Qt::StringLiterals::operator ""_s;
//     QGuiApplication app(argc, argv);
//
//     // Force the Fusion style to prevent Breeze theme engine crashes
//     QQuickStyle::setStyle("Fusion");
//
//     QQmlApplicationEngine engine;
//
//     HexModel hexModel;
//     HexController hexController(&hexModel);
//     hexController.loadConfiguration(":/resources/protocols.json");
//
//     auto minimapProvider = std::make_unique<HexMinimapProvider>(&hexModel);
//     engine.addImageProvider(QLatin1String("hexminimap"), minimapProvider.release());
//
//     qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexModelInst", &hexModel);
//     qmlRegisterSingletonInstance("AppBackend", 1, 0, "HexControllerInst", &hexController);
//
//     const QUrl url(u"qrc:/qt/qml/bite/qml/main.qml"_s);
//     QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
//         &app, [url](QObject *obj, const QUrl &objUrl) {
//             if (!obj && url == objUrl)
//                 QCoreApplication::exit(-1);
//         }, Qt::QueuedConnection);
//
//     engine.load(url);
//
//     return app.exec();
// }