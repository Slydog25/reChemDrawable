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
    src/core/EditHistory.cpp
    src/core/GeometryEngine.cpp
    src/core/Molecule.cpp
    src/core/MolfileReader.cpp
    src/core/MolfileWriter.cpp
    src/core/ReactionLayer.cpp
    src/core/SvgWriter.cpp
)

set(PLATFORM_SOURCES
    src/platform/InputSettings.cpp
    src/platform/InputSettings.h
    src/platform/QtfbClient.cpp
    src/platform/QtfbClient.h
)

set(CORE_HEADERS
    src/core/Atom.h
    src/core/Bond.h
    src/core/EditHistory.h
    src/core/GeometryEngine.h
    src/core/Molecule.h
    src/core/MolfileReader.h
    src/core/MolfileWriter.h
    src/core/ReactionLayer.h
    src/core/SvgWriter.h
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
        ${CORE_SOURCES}
        ${CORE_HEADERS}
    )
    target_include_directories(core_tests PRIVATE src)
    target_link_libraries(core_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME core_tests COMMAND core_tests)
    set_tests_properties(core_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
