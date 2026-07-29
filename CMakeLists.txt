cmake_minimum_required(VERSION 4.0)
project(ChronoNotes VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CHRONONOTES_APP_ID "com.chrononotes.desktop")
set(CHRONONOTES_RELEASE_CHANNEL "candidate" CACHE STRING
    "Release channel label embedded in unsigned packages.")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/windows_app.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/windows_app.rc"
    @ONLY
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/portable.flag" "ChronoNotes portable profile\n")

set(CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64"
    ${CMAKE_PREFIX_PATH}
)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Test Concurrent Sql Network)

get_filename_component(CHRONONOTES_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(CHRONONOTES_COMPILER_RUNTIME_DLLS
    "${CHRONONOTES_COMPILER_BIN_DIR}/libgcc_s_seh-1.dll"
    "${CHRONONOTES_COMPILER_BIN_DIR}/libstdc++-6.dll"
    "${CHRONONOTES_COMPILER_BIN_DIR}/libwinpthread-1.dll"
)
foreach(runtime_dll IN LISTS CHRONONOTES_COMPILER_RUNTIME_DLLS)
    if(NOT EXISTS "${runtime_dll}")
        message(FATAL_ERROR "Required MinGW runtime is missing: ${runtime_dll}")
    endif()
endforeach()

set(CHRONONOTES_QT_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin")
set(CHRONONOTES_QT_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/plugins")
set(CHRONONOTES_MINGW_RUNTIME_BIN "" CACHE PATH "Optional MinGW runtime bin directory for running tests.")
set(CHRONONOTES_TEST_ENV
    "PATH=path_list_prepend:${CHRONONOTES_QT_BIN_DIR}"
    "QT_PLUGIN_PATH=set:${CHRONONOTES_QT_PLUGIN_DIR}"
)
if (CHRONONOTES_MINGW_RUNTIME_BIN)
    list(APPEND CHRONONOTES_TEST_ENV
        "PATH=path_list_prepend:${CHRONONOTES_MINGW_RUNTIME_BIN}"
    )
endif()

if (COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

set(APP_SOURCES
    src/note_store.cpp
    src/notebook.cpp
    src/note_view.cpp
    src/project_tree_model.cpp
    src/config.cpp
    src/local_profile.cpp
    src/credential_store.cpp
    src/clock.cpp
    src/ai_client.cpp
    src/backup_service.cpp
    src/workspace_recovery.cpp
)

# Active desktop implementation: Qt/QML shell plus C++ data/config/AI modules.
qt_add_executable(ChronoNotes WIN32
    src/qt_main.cpp
    src/qt_note_app.cpp
    src/qt_note_app.h
    src/note_view.h
    src/notebook.h
    src/project_tree_model.h
    src/backup_service.h
    src/workspace_recovery.h
    src/local_profile.h
    src/credential_store.h
    src/clock.h
    resources.qrc
    "${CMAKE_CURRENT_BINARY_DIR}/generated/windows_app.rc"
    ${APP_SOURCES}
)

qt_add_qml_module(ChronoNotes
    URI ChronoNotes
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        qml/ChronoTokens.qml
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

target_include_directories(ChronoNotes PRIVATE
    src
)
target_compile_definitions(ChronoNotes PRIVATE UNICODE _UNICODE)
target_compile_definitions(ChronoNotes PRIVATE
    CHRONONOTES_VERSION="${PROJECT_VERSION}"
    CHRONONOTES_APP_ID="${CHRONONOTES_APP_ID}"
    CHRONONOTES_RELEASE_CHANNEL="${CHRONONOTES_RELEASE_CHANNEL}"
)
target_link_libraries(ChronoNotes PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent Qt6::Sql Qt6::Network user32 shell32 Advapi32)

if (WIN32)
    add_custom_command(TARGET ChronoNotes POST_BUILD
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin/windeployqt.exe"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                --no-translations
                --no-compiler-runtime
                --no-opengl-sw
                --no-system-d3d-compiler
                --skip-plugin-types qmltooling,generic
                --exclude-plugins qgif,qico,qjpeg,qsqlmimer,qsqlodbc,qsqlpsql
                "$<TARGET_FILE:ChronoNotes>"
        COMMENT "Deploying Qt runtime"
    )
    foreach(runtime_dll IN LISTS CHRONONOTES_COMPILER_RUNTIME_DLLS)
        add_custom_command(TARGET ChronoNotes POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${runtime_dll}"
                    "$<TARGET_FILE_DIR:ChronoNotes>"
            COMMENT "Staging matching MinGW runtime: ${runtime_dll}"
        )
    endforeach()
    add_custom_command(TARGET ChronoNotes POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                "-DROOT=$<TARGET_FILE_DIR:ChronoNotes>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prune_deployment.cmake"
        COMMENT "Pruning unused Qt styles and development plugins"
    )
endif()

install(TARGETS ChronoNotes
    RUNTIME DESTINATION .
    COMPONENT Runtime
)
set(CHRONONOTES_WINDEPLOYQT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin/windeployqt.exe"
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_runtime.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/deploy_runtime.cmake"
    @ONLY
)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/generated/deploy_runtime.cmake"
    COMPONENT Runtime
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION .
    COMPONENT Runtime
)
install(FILES ${CHRONONOTES_COMPILER_RUNTIME_DLLS}
    DESTINATION .
    COMPONENT Runtime
)
install(SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prune_deployment.cmake"
    COMPONENT Runtime
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/portable.flag"
    DESTINATION .
    COMPONENT Portable
)

enable_testing()

add_executable(note_store_tests
    tests/note_store_tests.cpp
    src/note_store.cpp
)

target_include_directories(note_store_tests PRIVATE src)
target_compile_definitions(note_store_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(note_store_tests PRIVATE Qt6::Sql)

add_test(NAME note_store_tests COMMAND note_store_tests)

add_executable(notebook_tests
    tests/notebook_tests.cpp
    src/notebook.cpp
    src/notebook.h
    src/note_store.cpp
)
target_include_directories(notebook_tests PRIVATE src)
target_compile_definitions(notebook_tests PRIVATE
    UNICODE
    _UNICODE
    CHRONONOTES_TESTING
)
target_link_libraries(notebook_tests PRIVATE Qt6::Test Qt6::Sql)
add_test(NAME notebook_tests COMMAND notebook_tests)
set_tests_properties(notebook_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "${CHRONONOTES_TEST_ENV}"
)

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
    ENVIRONMENT_MODIFICATION "${CHRONONOTES_TEST_ENV}"
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
    ENVIRONMENT_MODIFICATION "${CHRONONOTES_TEST_ENV}"
)

add_executable(config_tests
    tests/config_tests.cpp
    src/config.cpp
)

target_include_directories(config_tests PRIVATE src)
target_compile_definitions(config_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(config_tests PRIVATE Qt6::Core)
set_target_properties(config_tests PROPERTIES AUTOMOC OFF)

add_test(NAME config_tests COMMAND config_tests)

add_executable(ai_client_tests
    tests/ai_client_tests.cpp
    src/ai_client.cpp
    src/ai_client.h
)
target_include_directories(ai_client_tests PRIVATE src)
target_compile_definitions(ai_client_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(ai_client_tests PRIVATE Qt6::Test Qt6::Concurrent Qt6::Network)
add_test(NAME ai_client_tests COMMAND ai_client_tests)
set_tests_properties(ai_client_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "${CHRONONOTES_TEST_ENV}"
)

add_executable(backup_service_tests
    tests/backup_service_tests.cpp
    src/backup_service.cpp
    src/backup_service.h
    src/note_store.cpp
)
target_include_directories(backup_service_tests PRIVATE src)
target_compile_definitions(backup_service_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(backup_service_tests PRIVATE Qt6::Test Qt6::Sql)
add_test(NAME backup_service_tests COMMAND backup_service_tests)
set_tests_properties(backup_service_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "${CHRONONOTES_TEST_ENV}"
)

add_executable(workspace_recovery_tests
    tests/workspace_recovery_tests.cpp
    src/workspace_recovery.cpp
    src/workspace_recovery.h
    src/backup_service.cpp
    src/note_store.cpp
)
target_include_directories(workspace_recovery_tests PRIVATE src)
target_compile_definitions(workspace_recovery_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(workspace_recovery_tests PRIVATE Qt6::Test Qt6::Sql)
add_test(NAME workspace_recovery_tests COMMAND workspace_recovery_tests)
set_tests_properties(workspace_recovery_tests PROPERTIES
    ENVIRONMENT_MODIFICATION "${CHRONONOTES_TEST_ENV}"
)

add_executable(qt_note_app_tests
    tests/qt_note_app_tests.cpp
    src/qt_note_app.cpp
    src/qt_note_app.h
    ${APP_SOURCES}
)

target_include_directories(qt_note_app_tests PRIVATE src)
target_compile_definitions(qt_note_app_tests PRIVATE UNICODE _UNICODE)
target_link_libraries(qt_note_app_tests PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Test Qt6::Concurrent Qt6::Sql Qt6::Network user32 shell32 Advapi32)

add_test(NAME qt_note_app_tests COMMAND qt_note_app_tests)
set_tests_properties(qt_note_app_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "${CHRONONOTES_TEST_ENV};QT_QPA_PLATFORM=set:offscreen"
)

add_test(NAME qml_interaction_tests
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Qt/6.8.3/mingw_64/bin/qmltestrunner.exe"
            -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
            -import "${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
set_tests_properties(qml_interaction_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "${CHRONONOTES_TEST_ENV};QT_QPA_PLATFORM=set:offscreen;QT_QUICK_CONTROLS_STYLE=set:Basic"
)

set(CPACK_PACKAGE_NAME "ChronoNotes")
set(CPACK_PACKAGE_VENDOR "ChronoNotes")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A lightweight, private Windows planning notebook")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/chrononotes/chrononotes")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "ChronoNotes")
set(CPACK_PACKAGE_FILE_NAME "ChronoNotes-${PROJECT_VERSION}-${CHRONONOTES_RELEASE_CHANNEL}-windows-x64")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(CPACK_MONOLITHIC_INSTALL OFF)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChronoNotesCPackOptions.cmake")

set(CPACK_NSIS_DISPLAY_NAME "ChronoNotes")
set(CPACK_NSIS_PACKAGE_NAME "ChronoNotes")
set(CPACK_NSIS_INSTALLED_ICON_NAME "ChronoNotes.exe")
set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/chrono_notes.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/chrono_notes.ico")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
set(CPACK_PACKAGE_EXECUTABLES "ChronoNotes" "ChronoNotes")
set(CPACK_CREATE_DESKTOP_LINKS "ChronoNotes")

include(CPack)
