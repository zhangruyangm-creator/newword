#ifndef ENGINE_DOCUMENTMODEL_H
#define ENGINE_DOCUMENTMODEL_H

#include "pagelayout.h"
#include "styleutils.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QString>
#include <QVector>
#include <Qt>

// FloatingTextBox is defined at project root.
#include "../floatingtextbox.h"

namespace Engine {

struct CharStyle {
    QFont font;
    QColor foreground {0, 0, 0};
    QColor background; //!< invalid = none
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool superscript = false;
};

struct DocRun {
    QString text;
    CharStyle style;
    //! Atomic non-text (image/formula). Prefer `image` when set.
    bool isAtomic = false;
    qreal atomicHeightPt = 0;
    qreal atomicWidthPt = 0;
    QImage image;
    //! 0=inline, 1=block, 2=floatLeft, 3=floatRight (see ImageProps::Wrap)
    int imageWrap = 1;
    //! Qt::AlignLeft / AlignHCenter / AlignRight
    int imageAlign = int(Qt::AlignHCenter);
    //! Non-empty → footnote reference marker (body in DocumentModel::footnoteBodies).
    QString footnoteId;
    int footnoteNumber = 0;
    //! Non-empty → endnote reference marker (body in DocumentModel::endnoteBodies).
    QString endnoteId;
    int endnoteNumber = 0;
    //! Non-empty → this run is inside a comment range (body in DocumentModel::comments).
    QString commentId;
};

struct DocParagraph {
    int documentPosition = 0; // QTextDocument character offset
    int headingLevel = 0; // 0 = body, 1–4 = heading
    StyleUtils::StyleId styleId = StyleUtils::StyleId::Normal;
    bool pageBreakBefore = false;
    qreal spaceAfterPt = 0.0;
    QVector<DocRun> runs;

    [[nodiscard]] QString plainText() const;
};

struct DocTableCell {
    QVector<DocParagraph> paragraphs;
    QColor background;
    int columnSpan = 1; //!< w:gridSpan / QTextTableCell::columnSpan
    int rowSpan = 1;    //!< resolved from vMerge / QTextTableCell::rowSpan
    bool covered = false; //!< true = occupied by another cell's span (no own content)
};

struct DocTable {
    int documentPosition = 0;
    int columnCount = 0;
    QVector<QVector<DocTableCell>> rows; // [row][col] full grid including covered slots
    QVector<qreal> columnWeights;        // relative; empty → equal
    QVector<qreal> rowMinHeightsPt;      // parallel to rows; 0 / missing → auto
    qreal borderPt = 0.5;
    QColor borderColor {160, 160, 160};
    qreal cellPaddingPt = 4.0;
};

struct DocBlock {
    enum class Kind { Paragraph, Table };
    Kind kind = Kind::Paragraph;
    int documentPosition = 0;
    DocParagraph paragraph;
    DocTable table;
};

struct DocSection {
    PageLayoutSettings pageSetup;
    QVector<DocBlock> blocks;
};

struct DocComment {
    QString author;
    QString text;
};

struct DocumentModel {
    QVector<DocSection> sections;
    //! Footnote id → body text (markers reference via DocRun::footnoteId).
    QHash<QString, QString> footnoteBodies;
    //! Stable display order of footnote ids (document order).
    QVector<QString> footnoteOrder;
    //! Endnote id → body text (markers reference via DocRun::endnoteId).
    QHash<QString, QString> endnoteBodies;
    //! Stable display order of endnote ids (document order).
    QVector<QString> endnoteOrder;
    //! Comment id → author/text (ranges tagged via DocRun::commentId).
    QHash<QString, DocComment> comments;
    //! Stable display order of comment ids (document order).
    QVector<QString> commentOrder;
    //! Absolute floating text boxes (not part of flow).
    QVector<FloatingTextBox> floatingBoxes;

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] int paragraphCount() const;
    [[nodiscard]] int blockCount() const;
};

} // namespace Engine

#endif // ENGINE_DOCUMENTMODEL_H
