/*
 * BrReport - Band-oriented report generator for C++/Qt
 * Copyright 2026 João Elson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "brprinting.h"

#include "brpreviewdialog.h"
#include "brrenderer.h"
#include "brreport.h"

#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>

namespace {

QPageLayout layoutFor(const BrPage &page)
{
    const bool landscape = page.size.width() > page.size.height();
    const QSizeF portrait = landscape ? page.size.transposed() : page.size;
    return QPageLayout(QPageSize(portrait, QPageSize::Millimeter),
                       landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                       QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

} // namespace

bool BrPrinting::print(const BrDocument &document, QPrinter *printer, int fromPage, int toPage)
{
    if (document.isEmpty() || !printer)
        return false;
    const int first = fromPage > 0 ? fromPage - 1 : 0;
    const int last = toPage > 0 ? std::min(toPage, document.pageCount()) - 1 : document.pageCount() - 1;
    if (first > last)
        return false;

    printer->setFullPage(true);
    printer->setDocName(document.title);
    printer->setPageLayout(layoutFor(document.pages.at(first)));

    QPainter painter;
    if (!painter.begin(printer))
        return false;
    for (int i = first; i <= last; ++i) {
        if (i > first) {
            printer->setPageLayout(layoutFor(document.pages.at(i)));
            printer->newPage();
        }
        // With fullPage, painter (0,0) is the physical paper corner, like page mm (0,0).
        const double pxPerMm = printer->resolution() / 25.4;
        BrRenderer::renderPage(&painter, document.pages.at(i), pxPerMm);
    }
    return painter.end();
}

bool BrPrinting::print(const BrDocument &document, QWidget *parent, bool showDialog)
{
    if (document.isEmpty())
        return false;
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(document.title);
    printer.setPageLayout(layoutFor(document.pages.first()));
    int from = 0, to = 0;
    if (showDialog) {
        QPrintDialog dialog(&printer, parent);
        dialog.setMinMax(1, document.pageCount());
        dialog.setOption(QAbstractPrintDialog::PrintPageRange, true);
        dialog.setOption(QAbstractPrintDialog::PrintCurrentPage, false);
        if (dialog.exec() != QDialog::Accepted)
            return false;
        if (printer.printRange() == QPrinter::PageRange) {
            from = printer.fromPage();
            to = printer.toPage();
        }
    }
    return print(document, &printer, from, to);
}

bool BrPrinting::print(BrReport *report, QWidget *parent, bool showDialog)
{
    if (!report)
        return false;
    if (!report->isPrepared() && !report->prepare()) {
        QMessageBox::warning(parent, QObject::tr("Print"), report->errorString());
        return false;
    }
    return print(report->document(), parent, showDialog);
}

void BrPrinting::preview(BrReport *report, QWidget *parent)
{
    if (!report)
        return;
    if (!report->isPrepared() && !report->prepare() && report->document().isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("Preview"), report->errorString());
        return;
    }
    preview(report->document(), parent);
}

void BrPrinting::preview(const BrDocument &document, QWidget *parent)
{
    BrPreviewDialog dialog(parent);
    dialog.setDocument(document);
    dialog.exec();
}
