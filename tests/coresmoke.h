#ifndef CORESMOKE_H
#define CORESMOKE_H

#include "pagegeometry.h"
#include "pagelayout.h"
#include "docxexporter.h"
#include "docximporter.h"
#include "docxio.h"
#include "docxmeta.h"
#include "docxpackage.h"
#include "editorviewlayout.h"
#include "pagededitorwidget.h"
#include "documentmodel.h"
#include "layoutengine.h"
#include "pagedocumentpainter.h"
#include "paginationmetrics.h"
#include "qtextadapter.h"
#include "reviewnotes.h"
#include "styleutils.h"
#include "tablegeometry.h"
#include "floatingtextbox.h"
#include "textstats.h"

#include <QDir>
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QObject>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QTextFrameFormat>
#include <QTextImageFormat>
#include <QTextLength>
#include <QTextTable>
#include <QTextTableFormat>
#include <QUrl>
#include <QFrame>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextEdit>
#include <QtTest/QtTest>

class CoreSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void pageGeometry_a4_scalesWithZoom();
    void pageGeometry_chromeHeights();
    void docx_roundTrip_preservesPlainParagraph();
    void docx_roundTrip_preservesHeadingAndBold();
    void layoutEngine_emptyDocument_onePage();
    void layoutEngine_longText_paginates();
    void layoutEngine_pageCount_matchesPainterApi();
    void adapter_preservesTextColor();
    void adapter_preservesImageResource();
    void adapter_preservesTableCells();
    void layoutEngine_inlineTextBox_survives();
    void floatingTextBoxes_storeRoundTrip();
    void floatingTextBoxes_layoutAndPaint();
    void pagination_visualVsEngine_samplesWithinTolerance();
    void pagination_writeEvalMarkdown();
    void layoutEngine_pageBreaksHaveDocPositions();
    void layoutEngine_page2FirstLineAtTop();
    void layoutEngine_blanksThenText_page2Y();
    void layoutEngine_floatWrapClearedAcrossPage();
    void layoutEngine_oversizedImage_scalesToFit();
    void layoutEngine_sectionSetup_switchesPageBox();
    void editorViewLayout_fastStripNeverCollapsesBelowEngineFloor();
    void editorViewLayout_fastStripTracksContent();
    void debug_pagedEditorPrimitives();
    void pagedEditorWidget_pageBoundaryTyping();
    void pagedEditorWidget_renderAndEdit();
    void pagedEditorWidget_floatingTextBoxes();
    void pagedEditorWidget_gridAndColumnResize();
    void docx_modelExporter_roundTrip_plainAndStyled();
    void docx_modelExporter_preservesTable();
    void docx_importer_modelClosedLoop_headingBoldTable();
    void adapter_preservesMergedTableCells();
    void docx_model_mergedCells_roundTrip();
    void layoutEngine_mergedCells_paintHeights();
    void textStats_cjkAndLatinAndPunctuation();
    void textStats_selectionUsesParagraphSeparator();
    void docx_meta_headerFooter_roundTrip();
    void layoutEngine_floatImage_wrapsTextBeside();
    void adapter_snapshotCache_suffixMatchesFull();
    void adapter_extractsFootnotes_skipsAppendix();
    void layoutEngine_footnotes_atPageBottom();
    void docx_footnotes_modelRoundTrip();
    void docx_package_detectsFootnotes();
    void adapter_extractsComments();
    void docx_comments_modelRoundTrip();
    void adapter_extractsEndnotes_skipsAppendix();
    void layoutEngine_endnotes_atDocumentEnd();
    void docx_endnotes_modelRoundTrip();

};

#endif // CORESMOKE_H
