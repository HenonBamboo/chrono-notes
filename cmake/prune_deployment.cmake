if(NOT DEFINED ROOT OR ROOT STREQUAL "")
    set(ROOT "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
endif()
file(TO_CMAKE_PATH "${ROOT}" ROOT)

set(_unused_directories
    "generic"
    "qmltooling"
    "translations"
    "qml/QtQuick/Controls/FluentWinUI3"
    "qml/QtQuick/Controls/Fusion"
    "qml/QtQuick/Controls/Imagine"
    "qml/QtQuick/Controls/Material"
    "qml/QtQuick/Controls/Universal"
    "qml/QtQuick/Controls/Windows"
    "qml/QtQuick/NativeStyle"
    "qml/QtQuick/tooling"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Fusion"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Imagine"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Material"
    "qml/QtQuick/Dialogs/quickimpl/qml/+Universal"
)
foreach(_relative IN LISTS _unused_directories)
    file(REMOVE_RECURSE "${ROOT}/${_relative}")
endforeach()

set(_unused_files
    "D3Dcompiler_47.dll"
    "opengl32sw.dll"
    "imageformats/qgif.dll"
    "imageformats/qico.dll"
    "imageformats/qjpeg.dll"
    "sqldrivers/qsqlmimer.dll"
    "sqldrivers/qsqlodbc.dll"
    "sqldrivers/qsqlpsql.dll"
    "Qt6QuickControls2FluentWinUI3StyleImpl.dll"
    "Qt6QuickControls2Fusion.dll"
    "Qt6QuickControls2FusionStyleImpl.dll"
    "Qt6QuickControls2Imagine.dll"
    "Qt6QuickControls2ImagineStyleImpl.dll"
    "Qt6QuickControls2Material.dll"
    "Qt6QuickControls2MaterialStyleImpl.dll"
    "Qt6QuickControls2Universal.dll"
    "Qt6QuickControls2UniversalStyleImpl.dll"
    "Qt6QuickControls2WindowsStyleImpl.dll"
)
foreach(_relative IN LISTS _unused_files)
    file(REMOVE "${ROOT}/${_relative}")
endforeach()
