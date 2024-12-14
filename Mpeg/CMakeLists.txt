cmake_minimum_required(VERSION 3.5)

project(MpegEncoder LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_C_COMPILER /usr/bin/gcc)
#set(QT_QMAKE_EXECUTABLE /usr/lib/qt5/bin/qmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Find the Qt5 packages

find_package(Qt5 COMPONENTS Core Widgets Multimedia MultimediaWidgets REQUIRED)
#set(Qt5MultimediaWidgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5MultimediaWidgetsConfig")

# List the sources
set(SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.ui
)

set(UI_FILES
    mainwindow.ui # Ensure this path is correct
)

set(HEADERS
    mainwindow.h
)
# Create the executable
add_executable(Mpeg
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# Link Qt libraries
target_link_libraries(Mpeg
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::MultimediaWidgets
)
