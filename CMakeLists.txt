cmake_minimum_required(VERSION 3.16)
project(Hex-Editor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Qml Quick Widgets Core Gui QuickControls2)
find_package(ZLIB REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(Hex-Editor src/main.cpp src/HexModel.cpp src/HexController.cpp src/HexMinimapProvider.cpp)

qt_add_qml_module(Hex-Editor
    URI bite
    QML_FILES
        qml/HexTableView.qml
        qml/MinimapView.qml
        qml/PropertiesList.qml
        qml/SensorArrayEditor.qml
        qml/StringPayloadEditor.qml
        qml/main.qml
)

qt_add_resources(
        Hex-Editor "app_resources"
        PREFIX "/"
        FILES "resources/protocols.json"
)

target_link_libraries(Hex-Editor PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 ZLIB::ZLIB)


