cmake_minimum_required(VERSION 3.16)
project(ImgViewPro VERSION 1.0.0 LANGUAGES CXX)

if(MSVC)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Network)
find_package(OpenCV CONFIG REQUIRED)

qt_add_executable(ImgViewPro WIN32
    app.rc
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/ImageViewer.cpp
    src/ImageViewer.h
    src/DropMdiArea.cpp
    src/DropMdiArea.h
    src/DropListWidget.cpp
    src/DropListWidget.h
    src/ClickableLabel.cpp
    src/ClickableLabel.h
    src/FileAssociations.cpp
    src/FileAssociations.h
    src/DragDropUtils.h
    src/ViewerState.h
    src/DebugLog.h
    src/app.qrc
)

target_link_libraries(ImgViewPro PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core Qt6::Network opencv_core opencv_imgcodecs opencv_imgproc)

if(MSVC)
    target_compile_options(ImgViewPro PRIVATE /utf-8)
endif()
