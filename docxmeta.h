#ifndef DOCXMETA_H
#define DOCXMETA_H

#include "headerfootersettings.h"
#include "pagelayout.h"

/** Sidecar document chrome persisted in DOCX (headers / page setup). */
struct DocxDocumentMeta {
    HeaderFooterSettings headerFooter;
    PageLayoutSettings pageLayout;
    bool writeHeaderFooter = true;
    bool writePageLayout = true;
};

#endif // DOCXMETA_H
