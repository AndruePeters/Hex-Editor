cmake_minimum_required(VERSION 3.16)
project(qt_project VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)
#set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Qml Quick Widgets Core Gui QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(qt_project src/main.cpp src/HexModel.cpp src/HexController.cpp src/HexMinimapProvider.cpp)

qt_add_qml_module(qt_project
    URI bite
    QML_FILES qml/main.qml
)
target_link_libraries(qt_project PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)


