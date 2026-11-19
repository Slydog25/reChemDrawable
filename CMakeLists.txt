cmake_minimum_required(VERSION 3.21)

project(reChemDrawable VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_standard_project_setup()

option(RECHEMDRAW_EXTERNAL_QML "Load QML files from beside the application binary" OFF)

set(CORE_SOURCES
    src/core/Document.cpp
    src/core/EditHistory.cpp
    src/core/Elements.cpp
    src/core/GeometryEngine.cpp
    src/core/Molecule.cpp
    src/core/MolfileReader.cpp
    src/core/MolfileWriter.cpp
    src/core/ReactionLayer.cpp
    src/core/SmilesWriter.cpp
    src/core/SvgWriter.cpp
    src/core/ThumbnailWriter.cpp
)

set(PLATFORM_SOURCES
    src/platform/InputRouter.cpp
    src/platform/InputRouter.h
    src/platform/PenTool.cpp
    src/platform/PenTool.h
    src/platform/QmlJitWorkaround.h
    src/platform/QtfbClient.cpp
    src/platform/QtfbClient.h
    src/platform/QuitSignals.cpp
    src/platform/QuitSignals.h
    src/platform/TwoFingerTap.cpp
    src/platform/TwoFingerTap.h
)

set(CORE_HEADERS
    src/core/Atom.h
    src/core/Bond.h
    src/core/Document.h
    src/core/EditHistory.h
    src/core/Elements.h
    src/core/GeometryEngine.h
    src/core/Molecule.h
    src/core/MolfileReader.h
    src/core/MolfileWriter.h
    src/core/ReactionLayer.h
    src/core/SmilesWriter.h
    src/core/SvgWriter.h
    src/core/ThumbnailWriter.h
)

qt_add_executable(reChemDrawable
    src/main.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${PLATFORM_SOURCES}
)

if (RECHEMDRAW_EXTERNAL_QML)
    target_compile_definitions(reChemDrawable PRIVATE RECHEMDRAW_EXTERNAL_QML)
else()
    target_sources(reChemDrawable PRIVATE resources.qrc)
endif()

target_include_directories(reChemDrawable PRIVATE src)
target_link_libraries(reChemDrawable PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    qt_add_executable(core_tests
        tests/test_molecule.cpp
        src/platform/InputRouter.cpp
        src/platform/InputRouter.h
        src/platform/PenTool.cpp
        src/platform/PenTool.h
        src/platform/TwoFingerTap.cpp
        src/platform/TwoFingerTap.h
        ${CORE_SOURCES}
        ${CORE_HEADERS}
    )
    target_include_directories(core_tests PRIVATE src)
    target_link_libraries(core_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME core_tests COMMAND core_tests)
    set_tests_properties(core_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # Autosave, thumbnails and Open-list file handling.
    qt_add_executable(files_tests
        tests/test_files.cpp
        ${CORE_SOURCES}
        ${CORE_HEADERS}
    )
    target_include_directories(files_tests PRIVATE src)
    target_link_libraries(files_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME files_tests COMMAND files_tests)
    set_tests_properties(files_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    qt_add_executable(arrow_tests
        tests/test_arrows.cpp
        ${CORE_SOURCES}
        ${CORE_HEADERS}
    )
    target_include_directories(arrow_tests PRIVATE src)
    target_link_libraries(arrow_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME arrow_tests COMMAND arrow_tests)
    set_tests_properties(arrow_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    # QML interaction tests: drive MoleculeCanvas with simulated taps and drags.
    find_package(Qt6 6.4 REQUIRED COMPONENTS QuickTest)
    qt_add_executable(qml_tests
        tests/qml_tests.cpp
        ${CORE_SOURCES}
        ${CORE_HEADERS}
    )
    target_include_directories(qml_tests PRIVATE src)
    target_link_libraries(qml_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickTest)
    add_test(NAME qml_tests COMMAND qml_tests -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/qml)
    set_tests_properties(qml_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
