#include "documentmodel.h"

namespace Engine {

QString DocParagraph::plainText() const
{
    QString out;
    for (const DocRun &run : runs) {
        if (!run.isAtomic)
            out += run.text;
    }
    return out;
}

bool DocumentModel::isEmpty() const
{
    return blockCount() == 0;
}

int DocumentModel::blockCount() const
{
    int n = 0;
    for (const DocSection &section : sections)
        n += section.blocks.size();
    return n;
}

int DocumentModel::paragraphCount() const
{
    int n = 0;
    for (const DocSection &section : sections) {
        for (const DocBlock &block : section.blocks) {
            if (block.kind == DocBlock::Kind::Paragraph) {
                ++n;
            } else {
                for (const auto &row : block.table.rows) {
                    for (const DocTableCell &cell : row)
                        n += cell.paragraphs.size();
                }
            }
        }
    }
    return n;
}

} // namespace Engine
