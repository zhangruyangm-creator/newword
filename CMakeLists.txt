cmake_minimum_required(VERSION 3.16)

project(NewWord VERSION 0.5 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Concurrent Test)

# Third-party dependencies (brew install minizip-ng webp)
find_package(minizip-ng REQUIRED)

# libwebp — Qt's bundled image formats lack WebP.
find_path(WEBP_INCLUDE_DIR NAMES webp/decode.h)
find_library(WEBP_LIBRARY NAMES webp)
if(NOT WEBP_INCLUDE_DIR OR NOT WEBP_LIBRARY)
    message(FATAL_ERROR "libwebp not found — install with: brew install webp")
endif()
add_library(NewWordWebP UNKNOWN IMPORTED)
set_target_properties(NewWordWebP PROPERTIES
    IMPORTED_LOCATION "${WEBP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIR}"
)

# Hunspell — spell checking (system NSSpellChecker was macOS-only).
find_path(HUNSPELL_INCLUDE_DIR NAMES hunspell/hunspell.hxx)
find_library(HUNSPELL_LIBRARY NAMES hunspell-1.7 hunspell)
if(NOT HUNSPELL_INCLUDE_DIR OR NOT HUNSPELL_LIBRARY)
    message(FATAL_ERROR "Hunspell not found — install with: brew install hunspell")
endif()
add_library(NewWordHunspell UNKNOWN IMPORTED)
set_target_properties(NewWordHunspell PROPERTIES
    IMPORTED_LOCATION "${HUNSPELL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${HUNSPELL_INCLUDE_DIR}"
)
set(NEWWORD_DICT_DIR "${CMAKE_SOURCE_DIR}/resources/dictionaries")

set(NEWWORD_ENGINE_SOURCES
    engine/documentmodel.h
    engine/documentmodel.cpp
    engine/pagelist.h
    engine/qtextadapter.h
    engine/qtextadapter.cpp
    engine/layoutengine.h
    engine/layoutengine.cpp
    engine/docxexporter.h
    engine/docxexporter.cpp
    engine/docximporter.h
    engine/docximporter.cpp
)

set(NEWWORD_LAYOUT_PAINTER_SOURCES
    engine/layoutpainter.h
    engine/layoutpainter.cpp
)

set(NEWWORD_CORE_SOURCES
    appstyle.h
    appstyle.cpp
    pagelayout.h
    pagelayout.cpp
    pagegeometry.h
    pagegeometry.cpp
    styleutils.h
    styleutils.cpp
    textstats.h
    textstats.cpp
    imageprops.h
    imageprops.cpp
    imagepropertiesdialog.h
    imagepropertiesdialog.cpp
    docxmeta.h
    docxpackage.h
    docxpackage.cpp
    ziparchive.h
    ziparchive.cpp
    docxcommon.h
    docxcommon.cpp
    docxio.h
    docxreader.cpp
    docxwriter.cpp
    paginationmetrics.h
    paginationmetrics.cpp
    reviewnotes.h
    reviewnotes.cpp
    editorviewlayout.h
    editorviewlayout.cpp
    rulerwidget.h
    rulerwidget.cpp
    tablegeometry.h
    tablegeometry.cpp
    floatingtextbox.h
    floatingtextbox.cpp
    ${NEWWORD_ENGINE_SOURCES}
)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow_setup.cpp
    mainwindow_menu.cpp
    mainwindow_tabs.cpp
    mainwindow_stats.cpp
    mainwindow_file.cpp
    mainwindow_format.cpp
    mainwindow_insert.cpp
    mainwindow_table.cpp
    mainwindow_view.cpp
    webpimage.h
    webpimage.cpp
    pagededitorwidget.h
    pagededitorwidget.cpp
    pagedocumentlayout.h
    pagedocumentlayout.cpp
    pagedimageobject.h
    pagedimageobject.cpp
    pagedfloatingboxes.h
    pagedfloatingboxes.cpp
    editorwidget.h
    editorwidget.cpp
    documenttab.h
    documenttab.cpp
    documenttab_io.cpp
    ribbonbar.h
    ribbonbar.cpp
    lazyfontcombobox.h
    lazyfontcombobox.cpp
    outlinepane.h
    outlinepane.cpp
    outlineviewwidget.h
    outlineviewwidget.cpp
    rulerwidget.h
    rulerwidget.cpp
    pagesetupdialog.h
    pagesetupdialog.cpp
    paragraphdialog.h
    paragraphdialog.cpp
    pagepreview.h
    pagepreview.cpp
    findreplacedialog.h
    findreplacedialog.cpp
    headerfooterdialog.h
    headerfooterdialog.cpp
    tabledialogs.h
    tabledialogs.cpp
    formularenderer.h
    formularenderer.cpp
    formuladialog.h
    formuladialog.cpp
    formulaio.h
    formulaio.cpp
    docxconverter.h
    docxconverter.cpp
    headerfootersettings.h
    headerfootersettings.cpp
    pagedocumentpainter.h
    pagedocumentpainter.cpp
    ${NEWWORD_LAYOUT_PAINTER_SOURCES}
    documentsections.h
    documentsections.cpp
    documentviewmode.h
    editorviewlayout.h
    editorviewlayout.cpp
    spellchecker.h
    spellchecker.cpp
    spellhighlighter.h
    spellhighlighter.cpp
    pdfviewtab.h
    pdfviewtab.mm
    documentrecovery.h
    documentrecovery.cpp
    ${NEWWORD_CORE_SOURCES}
)

qt_add_executable(NewWord
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    resources.qrc
)

target_include_directories(NewWord PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(NewWord PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Concurrent
    MINIZIP::minizip-ng
    NewWordWebP
    NewWordHunspell
    "-framework ImageIO"
    z
    "-framework AppKit"
    "-framework PDFKit"
)

target_compile_definitions(NewWord PRIVATE
    NEWWORD_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    NEWWORD_DICT_DIR="${NEWWORD_DICT_DIR}"
)

add_custom_command(TARGET NewWord POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:NewWord>/../Resources/docx_bridge"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/tools/docx_bridge/docx_bridge.py"
            "$<TARGET_FILE_DIR:NewWord>/../Resources/docx_bridge/docx_bridge.py"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/tools/docx_bridge/requirements.txt"
            "$<TARGET_FILE_DIR:NewWord>/../Resources/docx_bridge/requirements.txt"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:NewWord>/../Resources/dictionaries"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NEWWORD_DICT_DIR}/en_US.aff"
            "$<TARGET_FILE_DIR:NewWord>/../Resources/dictionaries/en_US.aff"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NEWWORD_DICT_DIR}/en_US.dic"
            "$<TARGET_FILE_DIR:NewWord>/../Resources/dictionaries/en_US.dic"
    COMMENT "Bundle DOCX bridge script and dictionary into app Resources"
)

# Optional: keep a project-local venv for the DOCX bridge (mammoth/html2docx).
set(NEWWORD_DOCX_VENV "${CMAKE_SOURCE_DIR}/.venv")
if(EXISTS "${NEWWORD_DOCX_VENV}/bin/python3")
    message(STATUS "DOCX bridge venv: ${NEWWORD_DOCX_VENV}")
else()
    message(STATUS "DOCX bridge venv missing — run tools/docx_bridge/setup.sh for enhanced DOCX")
endif()

set_target_properties(NewWord PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.NewWord
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS NewWord
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(NewWord)

# --- Unit / smoke tests (PageGeometry + builtin DOCX round-trip) ---
enable_testing()
qt_add_executable(NewWordCoreTests
    tests/coresmoke.h
    tests/tst_coresmoke_main.cpp
    tests/tst_coresmoke_docx.cpp
    tests/tst_coresmoke_engine.cpp
    tests/tst_coresmoke_editor.cpp
    appstyle.h
    appstyle.cpp
    pagededitorwidget.h
    pagededitorwidget.cpp
    pagedocumentlayout.h
    pagedocumentlayout.cpp
    pagedimageobject.h
    pagedimageobject.cpp
    pagedfloatingboxes.h
    pagedfloatingboxes.cpp
    ${NEWWORD_CORE_SOURCES}
    ${NEWWORD_LAYOUT_PAINTER_SOURCES}
    headerfootersettings.h
    headerfootersettings.cpp
    pagedocumentpainter.h
    pagedocumentpainter.cpp
    documentsections.h
    documentsections.cpp
)
target_include_directories(NewWordCoreTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)
target_compile_definitions(NewWordCoreTests PRIVATE
    NEWWORD_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(NewWordCoreTests PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    MINIZIP::minizip-ng
    z
)
add_test(NAME NewWordCoreTests COMMAND NewWordCoreTests)
set_tests_properties(NewWordCoreTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# DocumentTab smoke: the self-drawn editor inside the real tab (view switching,
# typing, rendering) without launching the whole app.
qt_add_executable(NewWordDocTabSmokeTests
    tests/tst_documenttab_smoke.cpp
    documenttab.h
    documenttab.cpp
    documenttab_io.cpp
    pagededitorwidget.h
    pagededitorwidget.cpp
    pagedocumentlayout.h
    pagedocumentlayout.cpp
    pagedimageobject.h
    pagedimageobject.cpp
    pagedfloatingboxes.h
    pagedfloatingboxes.cpp
    appstyle.h
    appstyle.cpp
    documentrecovery.h
    documentrecovery.cpp
    formularenderer.h
    formularenderer.cpp
    formulaio.h
    formulaio.cpp
    outlineviewwidget.h
    outlineviewwidget.cpp
    spellchecker.h
    spellchecker.cpp
    spellhighlighter.h
    spellhighlighter.cpp
    docxconverter.h
    docxconverter.cpp
    headerfootersettings.h
    headerfootersettings.cpp
    webpimage.h
    webpimage.cpp
    ${NEWWORD_CORE_SOURCES}
)
target_include_directories(NewWordDocTabSmokeTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)
target_compile_definitions(NewWordDocTabSmokeTests PRIVATE
    NEWWORD_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    NEWWORD_DICT_DIR="${NEWWORD_DICT_DIR}"
)
target_link_libraries(NewWordDocTabSmokeTests PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Concurrent
    MINIZIP::minizip-ng
    NewWordWebP
    NewWordHunspell
    "-framework ImageIO"
    z
    "-framework AppKit"
)
add_test(NAME NewWordDocTabSmokeTests COMMAND NewWordDocTabSmokeTests)
set_tests_properties(NewWordDocTabSmokeTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Editor performance micro-benchmark: open / per-keystroke / pagination cost.
qt_add_executable(NewWordBenchEditorPerf
    tools/bench_editor_perf.cpp
    appstyle.h
    appstyle.cpp
    pagededitorwidget.h
    pagededitorwidget.cpp
    pagedocumentlayout.h
    pagedocumentlayout.cpp
    pagedimageobject.h
    pagedimageobject.cpp
    pagedfloatingboxes.h
    pagedfloatingboxes.cpp
    ${NEWWORD_CORE_SOURCES}
    headerfootersettings.h
    headerfootersettings.cpp
)
target_include_directories(NewWordBenchEditorPerf PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)
target_link_libraries(NewWordBenchEditorPerf PRIVATE Qt6::Gui Qt6::Widgets MINIZIP::minizip-ng z)

# Visual QA tool for the self-drawn paged editor (renders to a PNG offscreen).
qt_add_executable(NewWordRenderPagedPreview
    tools/render_paged_editor.cpp
    appstyle.h
    appstyle.cpp
    pagededitorwidget.h
    pagededitorwidget.cpp
    pagedocumentlayout.h
    pagedocumentlayout.cpp
    pagedimageobject.h
    pagedimageobject.cpp
    pagedfloatingboxes.h
    pagedfloatingboxes.cpp
    ${NEWWORD_CORE_SOURCES}
    headerfootersettings.h
    headerfootersettings.cpp
)
target_include_directories(NewWordRenderPagedPreview PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)
target_link_libraries(NewWordRenderPagedPreview PRIVATE Qt6::Gui Qt6::Widgets MINIZIP::minizip-ng z)

# Layout performance micro-benchmark (Adapter / Engine / Fast path)
qt_add_executable(NewWordBenchLayoutPerf tools/bench_layout_perf.cpp ${NEWWORD_CORE_SOURCES})
target_include_directories(NewWordBenchLayoutPerf PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)
target_link_libraries(NewWordBenchLayoutPerf PRIVATE Qt6::Gui Qt6::Widgets MINIZIP::minizip-ng z)

# Mixed-document micro-benchmark: QTextDocument with images + tables +
# fragmented formatting (real-world paper/report load). Independent of
# NewWord sources; measures the raw editing cost of QTextDocument.
qt_add_executable(NewWordBenchMixedDoc tools/bench_mixed_doc.cpp)
target_link_libraries(NewWordBenchMixedDoc PRIVATE Qt6::Gui Qt6::Widgets)

# Generate examples/demo.docx (acceptance sample)
qt_add_executable(NewWordMakeDemo tools/make_demo.cpp ${NEWWORD_CORE_SOURCES})
target_include_directories(NewWordMakeDemo PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/engine
)
target_compile_definitions(NewWordMakeDemo PRIVATE NEWWORD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(NewWordMakeDemo PRIVATE Qt6::Gui Qt6::Widgets MINIZIP::minizip-ng z)
add_custom_command(TARGET NewWordMakeDemo POST_BUILD
    COMMAND "$<TARGET_FILE:NewWordMakeDemo>"
    COMMENT "Generate examples/demo.docx"
    VERBATIM
)
