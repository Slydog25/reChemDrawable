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
    src/core/Abbreviations.cpp
    src/core/CdxmlWriter.cpp
    src/core/SceneRenderer.cpp
    src/core/Document.cpp
    src/core/EditHistory.cpp
    src/core/Elements.cpp
    src/core/GeometryEngine.cpp
    src/core/Layout2D.cpp
    src/core/Molecule.cpp
    src/core/MolfileReader.cpp
    src/core/MolfileWriter.cpp
    src/core/Properties.cpp
    src/core/ReactionLayer.cpp
    src/core/SmilesReader.cpp
    src/core/SmilesWriter.cpp
    src/core/Stereo.cpp
    src/core/SvgWriter.cpp
    src/core/ThumbnailWriter.cpp
)

set(CORE_HEADERS
    src/core/Abbreviations.h
    src/core/CdxmlWriter.h
    src/core/SceneRenderer.h
    src/core/Atom.h
    src/core/Bond.h
    src/core/Document.h
    src/core/EditHistory.h
    src/core/Elements.h
    src/core/GeometryEngine.h
    src/core/Layout2D.h
    src/core/Molecule.h
    src/core/MolfileReader.h
    src/core/MolfileWriter.h
    src/core/Properties.h
    src/core/ReactionLayer.h
    src/core/SmilesReader.h
    src/core/SmilesWriter.h
    src/core/Stereo.h
    src/core/SvgWriter.h
    src/core/ThumbnailWriter.h
)

# The chemistry core and the testable input helpers are compiled once and
# shared by the app and every test program.
qt_add_library(rechemdraw_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(rechemdraw_core PUBLIC src)
target_link_libraries(rechemdraw_core PUBLIC Qt6::Core Qt6::Gui)

qt_add_library(rechemdraw_platform STATIC
    src/platform/DirtyRect.cpp
    src/platform/DirtyRect.h
    src/platform/DisplayController.h
    src/platform/DisplayProtocol.cpp
    src/platform/DisplayProtocol.h
    src/platform/RefreshPolicy.cpp
    src/platform/RefreshPolicy.h
    src/platform/InputRouter.cpp
    src/platform/InputRouter.h
    src/platform/PanZoomGesture.cpp
    src/platform/PanZoomGesture.h
    src/platform/PenTool.cpp
    src/platform/PenTool.h
    src/platform/QmlJitWorkaround.h
    src/platform/QuitSignals.cpp
    src/platform/QuitSignals.h
    src/platform/TwoFingerTap.cpp
    src/platform/TwoFingerTap.h
)
target_include_directories(rechemdraw_platform PUBLIC src)
target_link_libraries(rechemdraw_platform PUBLIC Qt6::Core Qt6::Gui)

qt_add_executable(reChemDrawable
    src/main.cpp
    src/platform/QtfbClient.cpp
    src/platform/QtfbClient.h
)

if (RECHEMDRAW_EXTERNAL_QML)
    target_compile_definitions(reChemDrawable PRIVATE RECHEMDRAW_EXTERNAL_QML)
else()
    target_sources(reChemDrawable PRIVATE resources.qrc)
endif()

target_link_libraries(reChemDrawable PRIVATE rechemdraw_core rechemdraw_platform Qt6::Qml Qt6::Quick)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test QuickTest)

    # rechemdraw_add_test(<name> <source>): a QtTest program run by ctest.
    function(rechemdraw_add_test name source)
        qt_add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE rechemdraw_core rechemdraw_platform Qt6::Test)
        add_test(NAME ${name} COMMAND ${name})
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endfunction()

    rechemdraw_add_test(core_tests tests/test_molecule.cpp)
    rechemdraw_add_test(undo_tests tests/test_undo.cpp)          # every edit is undoable
    rechemdraw_add_test(files_tests tests/test_files.cpp)        # autosave, thumbnails, Open list
    rechemdraw_add_test(arrow_tests tests/test_arrows.cpp)
    rechemdraw_add_test(interaction_tests tests/test_interaction.cpp)
    rechemdraw_add_test(test_templates tests/test_templates.cpp)
    rechemdraw_add_test(stereo_tests tests/test_stereo.cpp)      # CIP R/S and E/Z
    rechemdraw_add_test(label_tests tests/test_labels.cpp)
    rechemdraw_add_test(export_tests tests/test_exports.cpp)     # PNG, PDF, CDXML, reMarkable library
    rechemdraw_add_test(smiles_import_tests tests/test_smiles_import.cpp)  # parser, 2D layout, .smi
    rechemdraw_add_test(properties_tests tests/test_properties.cpp)  # DoU, Lipinski, TPSA, isotope pattern
    rechemdraw_add_test(stereo_smiles_tests tests/test_stereo_smiles.cpp)  # SMILES stereo, invert

    rechemdraw_add_test(display_tests tests/test_display.cpp)  # e-ink updates, refresh modes, keyboard, rotation

    # QML interaction tests: drive the real QML with simulated taps and drags.
    qt_add_executable(qml_tests tests/qml_tests.cpp)
    target_link_libraries(qml_tests PRIVATE rechemdraw_core rechemdraw_platform Qt6::Qml Qt6::Quick Qt6::QuickTest)
    add_test(NAME qml_tests COMMAND qml_tests -input ${CMAKE_CURRENT_SOURCE_DIR}/tests/qml)
    set_tests_properties(qml_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
