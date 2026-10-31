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

// brview: opens prepared BrReport documents (.brp) for viewing, printing and
// converting, without the application or the data that produced them.
//
//   brview report.brp                 preview window
//   brview report.brp --pdf out.pdf   convert (also --html, --png "page-%1.png")
//   brview report.brp --print         print with the system dialog

#include "brexport.h"
#include "brpreviewdialog.h"
#include "brprinting.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("brview"));
    QApplication::setApplicationVersion(QStringLiteral(BRREPORT_VERSION_STR));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Viewer for prepared BrReport documents (.brp)."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Document to open (.brp)."));
    parser.addOption({QStringLiteral("pdf"), QStringLiteral("Convert to PDF."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("html"), QStringLiteral("Convert to HTML."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("png"), QStringLiteral("Convert to PNG images (pattern with %1)."),
                      QStringLiteral("pattern")});
    parser.addOption({QStringLiteral("print"), QStringLiteral("Print with the system print dialog.")});
    parser.process(app);

    QTextStream err(stderr);
    QString fileName = parser.positionalArguments().value(0);
    const bool batch = parser.isSet(QStringLiteral("pdf")) || parser.isSet(QStringLiteral("html"))
                       || parser.isSet(QStringLiteral("png"));
    if (fileName.isEmpty()) {
        if (batch) {
            err << "brview: no input file" << Qt::endl;
            return 2;
        }
        fileName = QFileDialog::getOpenFileName(nullptr, QObject::tr("Open"), QString(),
                                                QObject::tr("BrReport document (*.brp)"));
        if (fileName.isEmpty())
            return 0;
    }

    BrDocument doc;
    if (!doc.load(fileName)) {
        const QString msg = QObject::tr("'%1' is not a valid BrReport document.").arg(fileName);
        if (batch)
            err << "brview: " << msg << Qt::endl;
        else
            QMessageBox::critical(nullptr, QStringLiteral("brview"), msg);
        return 1;
    }

    if (batch) {
        bool ok = true;
        if (parser.isSet(QStringLiteral("pdf")))
            ok = ok && BrExport::toPdf(doc, parser.value(QStringLiteral("pdf")));
        if (parser.isSet(QStringLiteral("html")))
            ok = ok && BrExport::toHtml(doc, parser.value(QStringLiteral("html")));
        if (parser.isSet(QStringLiteral("png")))
            ok = ok && !BrExport::toImages(doc, parser.value(QStringLiteral("png"))).isEmpty();
        if (!ok)
            err << "brview: conversion failed" << Qt::endl;
        return ok ? 0 : 1;
    }
    if (parser.isSet(QStringLiteral("print")))
        return BrPrinting::print(doc, nullptr, true) ? 0 : 1;

    BrPreviewDialog dialog;
    dialog.setDocument(doc);
    dialog.exec();
    return 0;
}
