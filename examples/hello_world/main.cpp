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

// The smallest BrReport program: no widgets, no data, writes hello.pdf.
// On a server without a display run it with QT_QPA_PLATFORM=offscreen.

#include "brreportcore.h"

#include <QGuiApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    BrReport report;
    report.setTitle(QStringLiteral("Hello, BrReport!"));

    // A Title band prints once, on the first page.
    BrBand *title = report.addBand(Br::BandType::Title, 20);
    auto *caption = new BrSystemInfo(Br::InfoType::Title, title);
    caption->setGeometry(0, 0, 190, 20);
    caption->setAlignment(Br::TextAlign::Center);
    caption->setFont(QFont(QStringLiteral("Arial"), 24, QFont::Bold));

    // Without a data source the Detail band prints exactly once.
    BrBand *detail = report.addBand(Br::BandType::Detail, 30);
    auto *memo = new BrMemo(detail);
    memo->setGeometry(0, 5, 190, 10);
    memo->setAlignment(Br::TextAlign::Justify);
    memo->setText(QStringLiteral(
        "BrReport is a band-oriented report generator for C++/Qt. "
        "Reports are built from bands (header, title, column header, detail, column footer, "
        "summary and footer), groups and sub-details, and can be previewed, printed or exported "
        "to PDF, HTML and images."));

    BrBand *footer = report.addBand(Br::BandType::Footer, 8);
    auto *page = new BrSystemInfo(Br::InfoType::PagePreview, footer);
    page->setGeometry(0, 0, 190, 8);
    page->setAlignment(Br::TextAlign::Right);
    page->setFormat(QStringLiteral("Page %1 of %2"));
    page->setBorders(BrBorders{false, true, false, false});

    const QString file = QStringLiteral("hello.pdf");
    if (!report.exportToPdf(file)) {
        QTextStream(stderr) << report.errorString() << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << "Written " << file << Qt::endl;
    return 0;
}
