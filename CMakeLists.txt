cmake_minimum_required(VERSION 4.0)
project(StickyNotesC CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64"
    ${CMAKE_PREFIX_PATH}
)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Test Concurrent Sql Network)

if (COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

set(APP_SOURCES
    src/note_store.cpp
    src/note_view.cpp
    src/project_tree_model.cpp
    src/config.cpp
    src/ai_client.cpp
    src/backup_service.cpp
)

# Active desktop implementation: Qt/QML shell plus C++ data/config/AI modules.
qt_add_executable(StickyNotesC WIN32
    src/qt_main.cpp
    src/qt_note_app.cpp
    src/qt_note_app.h
    src/note_view.h
    src/project_tree_model.h
    src/backup_service.h
    resources.qrc
    ${APP_SOURCES}
)

qt_add_qml_module(StickyNotesC
    URI StickyNotes
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppTitleBar.qml
        qml/StageTabs.qml
        qml/SearchBar.qml
        qml/EventComposer.qml
        qml/ProgressStats.qml
        qml/NoteListPanel.qml
        qml/NoteRow.qml
        qml/ProjectTreePanel.qml
        qml/OverlayPanel.qml
        qml/AiSummaryPanel.qml
        qml/DetailPanel.qml
        qml/SettingsPanel.qml
        qml/TinyMeta.qml
        qml/ToolPill.qml
        qml/WindowButton.qml
)

target_include_directories(StickyNotesC PRIVATE
    src
)
target_compile_definitions(StickyNotesC PRIVATE UNICODE _UNICODE)
target_link_libraries(StickyNotesC PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent Qt6::Sql Qt6::Network user32)

if (WIN32)
    add_custom_command(TARGET StickyNotesC POST_BUILD
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin/windeployqt.exe"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:StickyNotesC>"
        COMMENT "Deploying Qt runtime"
    )
endif()

enable_testing()

add_executable(note_store_tests
    tests/note_store_tests.cpp
    src/note_store.cpp
)

target_include_directories(note_store_tests PRIVATE src)
target_compile_definitions(note_store_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(note_store_tests PRIVATE Qt6::Sql)

add_test(NAME note_store_tests COMMAND note_store_tests)

add_executable(note_view_tests
    tests/note_view_tests.cpp
    src/note_view.h
    src/note_view.cpp
    src/note_store.cpp
)

target_include_directories(note_view_tests PRIVATE src)
target_compile_definitions(note_view_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(note_view_tests PRIVATE Qt6::Test Qt6::Sql)

add_test(NAME note_view_tests COMMAND note_view_tests)
set_tests_properties(note_view_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin;PATH=path_list_prepend:C:/Program Files/JetBrains/CLion 2025.2/bin/mingw/bin"
)

add_executable(project_tree_model_tests
    tests/project_tree_model_tests.cpp
    src/project_tree_model.h
    src/project_tree_model.cpp
)

target_include_directories(project_tree_model_tests PRIVATE src)
target_compile_definitions(project_tree_model_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(project_tree_model_tests PRIVATE Qt6::Test)

add_test(NAME project_tree_model_tests COMMAND project_tree_model_tests)
set_tests_properties(project_tree_model_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin;PATH=path_list_prepend:C:/Program Files/JetBrains/CLion 2025.2/bin/mingw/bin"
)

add_executable(config_tests
    tests/config_tests.cpp
    src/config.cpp
)

target_include_directories(config_tests PRIVATE src)
target_compile_definitions(config_tests PRIVATE UNICODE _UNICODE)

add_test(NAME config_tests COMMAND config_tests)

add_executable(qt_note_app_tests
    tests/qt_note_app_tests.cpp
    src/qt_note_app.cpp
    src/qt_note_app.h
    ${APP_SOURCES}
)

target_include_directories(qt_note_app_tests PRIVATE src)
target_compile_definitions(qt_note_app_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(qt_note_app_tests PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Test Qt6::Concurrent Qt6::Sql Qt6::Network user32)

add_test(NAME qt_note_app_tests COMMAND qt_note_app_tests)
set_tests_properties(qt_note_app_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin;PATH=path_list_prepend:C:/Program Files/JetBrains/CLion 2025.2/bin/mingw/bin"
)

add_test(NAME qml_interaction_tests
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin/qmltestrunner.exe"
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
            -import "${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
set_tests_properties(qml_interaction_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin;PATH=path_list_prepend:C:/Program Files/JetBrains/CLion 2025.2/bin/mingw/bin;QT_QUICK_CONTROLS_STYLE=set:Basic"
)
