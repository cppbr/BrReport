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

#ifndef EXAMPLEUTIL_H
#define EXAMPLEUTIL_H

#include "brreportwidgets.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

/*!
 * Shared by the examples:
 *   example                 -> opens the preview window
 *   example --pdf out.pdf   -> exports to PDF (also --html, --png "page-%1.png", --brp)
 *   example --save out.brt  -> saves the layout as a report file
 */
inline int runExample(BrReport &report)
{
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("pdf"), QStringLiteral("Export to PDF."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("html"), QStringLiteral("Export to HTML."), QStringLiteral("file")});
    parser.addOption({QStringLiteral("png"), QStringLiteral("Export pages to PNG (pattern with %1)."),
                      QStringLiteral("pattern")});
    parser.addOption({QStringLiteral("brp"), QStringLiteral("Save the prepared document."),
                      QStringLiteral("file")});
    parser.addOption({QStringLiteral("save"), QStringLiteral("Save the layout as a report file (.brt)."),
                      QStringLiteral("file")});
    parser.process(*QCoreApplication::instance());

    QTextStream out(stdout);
    bool exported = false;
    if (parser.isSet(QStringLiteral("save"))) {
        exported = true;
        if (!report.save(parser.value(QStringLiteral("save"))))
            return out << "Could not save the report file." << Qt::endl, 1;
    }
    if (parser.isSet(QStringLiteral("pdf"))) {
        exported = true;
        if (!report.exportToPdf(parser.value(QStringLiteral("pdf"))))
            return out << report.errorString() << Qt::endl, 1;
    }
    if (parser.isSet(QStringLiteral("html"))) {
        exported = true;
        if (!report.exportToHtml(parser.value(QStringLiteral("html"))))
            return out << report.errorString() << Qt::endl, 1;
    }
    if (parser.isSet(QStringLiteral("png"))) {
        exported = true;
        if (report.exportToImages(parser.value(QStringLiteral("png"))).isEmpty())
            return out << report.errorString() << Qt::endl, 1;
    }
    if (parser.isSet(QStringLiteral("brp"))) {
        exported = true;
        if (!report.saveDocument(parser.value(QStringLiteral("brp"))))
            return out << report.errorString() << Qt::endl, 1;
    }
    if (exported) {
        if (report.isPrepared())
            out << report.document().pageCount() << " page(s) exported." << Qt::endl;
        return 0;
    }
    BrPrinting::preview(&report);
    return 0;
}

#endif // EXAMPLEUTIL_H
