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

#include "brpreviewdialog.h"

#include "brexport.h"
#include "brpreviewwidget.h"
#include "brprinting.h"

#include <QAction>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

QIcon themeIcon(const char *name, QStyle::StandardPixmap fallback, QStyle *style)
{
    return QIcon::fromTheme(QString::fromLatin1(name), style->standardIcon(fallback));
}

} // namespace

BrPreviewDialog::BrPreviewDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Print Preview"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    resize(900, 1000);

    m_view = new BrPreviewWidget(this);
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(20, 20));
    QStyle *st = style();

    QAction *first = m_toolBar->addAction(themeIcon("go-first", QStyle::SP_MediaSkipBackward, st),
                                          tr("First page"), m_view, &BrPreviewWidget::firstPage);
    QAction *prev = m_toolBar->addAction(themeIcon("go-previous", QStyle::SP_ArrowBack, st),
                                         tr("Previous page"), m_view,
                                         &BrPreviewWidget::previousPage);
    m_pageSpin = new QSpinBox(this);
    m_pageSpin->setMinimum(1);
    m_pageSpin->setKeyboardTracking(false);
    m_toolBar->addWidget(m_pageSpin);
    m_pageCountLabel = new QLabel(this);
    m_pageCountLabel->setContentsMargins(4, 0, 4, 0);
    m_toolBar->addWidget(m_pageCountLabel);
    QAction *next = m_toolBar->addAction(themeIcon("go-next", QStyle::SP_ArrowForward, st),
                                         tr("Next page"), m_view, &BrPreviewWidget::nextPage);
    QAction *last = m_toolBar->addAction(themeIcon("go-last", QStyle::SP_MediaSkipForward, st),
                                         tr("Last page"), m_view, &BrPreviewWidget::lastPage);
    first->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Home));
    last->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_End));
    Q_UNUSED(prev)
    Q_UNUSED(next)
    m_toolBar->addSeparator();

    QAction *zoomOut = m_toolBar->addAction(QIcon::fromTheme(QStringLiteral("zoom-out")),
                                            tr("Zoom out"), m_view, &BrPreviewWidget::zoomOut);
    if (zoomOut->icon().isNull())
        zoomOut->setText(QStringLiteral("−"));
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    m_zoomCombo = new QComboBox(this);
    m_zoomCombo->setEditable(true);
    m_zoomCombo->setInsertPolicy(QComboBox::NoInsert);
    m_zoomCombo->addItem(tr("Fit width"));
    m_zoomCombo->addItem(tr("Fit page"));
    for (int z : {50, 75, 100, 125, 150, 200, 300, 400})
        m_zoomCombo->addItem(QStringLiteral("%1%").arg(z), z);
    m_zoomCombo->setMinimumContentsLength(8);
    m_toolBar->addWidget(m_zoomCombo);
    QAction *zoomIn = m_toolBar->addAction(QIcon::fromTheme(QStringLiteral("zoom-in")),
                                           tr("Zoom in"), m_view, &BrPreviewWidget::zoomIn);
    if (zoomIn->icon().isNull())
        zoomIn->setText(QStringLiteral("+"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    m_toolBar->addSeparator();

    QAction *printAction = m_toolBar->addAction(
        themeIcon("document-print", QStyle::SP_DialogOkButton, st), tr("Print..."), this,
        &BrPreviewDialog::print);
    printAction->setShortcut(QKeySequence::Print);
    QAction *exportAction = m_toolBar->addAction(
        themeIcon("document-save-as", QStyle::SP_DialogSaveButton, st), tr("Save as..."), this,
        &BrPreviewDialog::exportDocument);
    exportAction->setShortcut(QKeySequence::Save);
    QAction *openAction = m_toolBar->addAction(
        themeIcon("document-open", QStyle::SP_DialogOpenButton, st), tr("Open..."), this,
        &BrPreviewDialog::openDocument);
    openAction->setShortcut(QKeySequence::Open);
    m_toolBar->addSeparator();
    m_toolBar->addAction(themeIcon("window-close", QStyle::SP_DialogCloseButton, st), tr("Close"),
                         this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolBar);
    layout->addWidget(m_view, 1);

    connect(m_view, &BrPreviewWidget::currentPageChanged, this,
            &BrPreviewDialog::updatePageControls);
    connect(m_view, &BrPreviewWidget::zoomChanged, this, &BrPreviewDialog::updateZoomCombo);
    connect(m_pageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int page) { m_view->goToPage(page - 1); });
    connect(m_zoomCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == 0)
            m_view->setZoomMode(BrPreviewWidget::ZoomMode::FitWidth);
        else if (index == 1)
            m_view->setZoomMode(BrPreviewWidget::ZoomMode::FitPage);
        else
            m_view->setZoom(m_zoomCombo->itemData(index).toInt() / 100.0);
    });
    connect(m_zoomCombo->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        QString text = m_zoomCombo->currentText();
        text.remove(QLatin1Char('%'));
        bool ok = false;
        const double z = text.trimmed().toDouble(&ok);
        if (ok && z > 0)
            m_view->setZoom(z / 100.0);
        else
            updateZoomCombo(m_view->zoom());
    });

    m_view->setFocus();
}

void BrPreviewDialog::setDocument(const BrDocument &document)
{
    if (!document.title.isEmpty())
        setWindowTitle(tr("Print Preview - %1").arg(document.title));
    m_view->setDocument(document);
    m_pageSpin->setMaximum(std::max(1, document.pageCount()));
    m_pageCountLabel->setText(tr("of %1").arg(document.pageCount()));
    updatePageControls(m_view->currentPage());
    updateZoomCombo(m_view->zoom());
}

const BrDocument &BrPreviewDialog::document() const
{
    return m_view->document();
}

void BrPreviewDialog::updatePageControls(int page)
{
    const QSignalBlocker blocker(m_pageSpin);
    m_pageSpin->setValue(page + 1);
}

void BrPreviewDialog::updateZoomCombo(double zoom)
{
    switch (m_view->zoomMode()) {
    case BrPreviewWidget::ZoomMode::FitWidth:
        m_zoomCombo->setCurrentIndex(0);
        break;
    case BrPreviewWidget::ZoomMode::FitPage:
        m_zoomCombo->setCurrentIndex(1);
        break;
    default:
        m_zoomCombo->setEditText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
        break;
    }
}

void BrPreviewDialog::print()
{
    BrPrinting::print(document(), this, true);
}

void BrPreviewDialog::exportDocument()
{
    const QString pdf = tr("PDF document (*.pdf)");
    const QString html = tr("HTML page (*.html)");
    const QString png = tr("PNG images, one per page (*.png)");
    const QString brp = tr("BrReport document (*.brp)");
    QString filter = pdf;
    QString base = document().title.isEmpty() ? tr("report") : document().title;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save as"), base + QStringLiteral(".pdf"),
                                                    QStringList({pdf, html, png, brp}).join(QStringLiteral(";;")),
                                                    &filter);
    if (fileName.isEmpty())
        return;

    auto ensureSuffix = [&fileName](const char *suffix) {
        if (QFileInfo(fileName).suffix().isEmpty())
            fileName += QLatin1Char('.') + QLatin1String(suffix);
    };
    bool ok = false;
    if (filter == html) {
        ensureSuffix("html");
        ok = BrExport::toHtml(document(), fileName);
    } else if (filter == png) {
        ensureSuffix("png");
        const QFileInfo fi(fileName);
        const QString pattern = fi.path() + QLatin1Char('/') + fi.completeBaseName()
                                + QStringLiteral("-%1.") + fi.suffix();
        ok = !BrExport::toImages(document(), pattern).isEmpty();
    } else if (filter == brp) {
        ensureSuffix("brp");
        ok = document().save(fileName);
    } else {
        ensureSuffix("pdf");
        ok = BrExport::toPdf(document(), fileName);
    }
    if (!ok)
        QMessageBox::warning(this, tr("Save as"), tr("Could not write '%1'.").arg(fileName));
}

void BrPreviewDialog::openDocument()
{
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open"), QString(),
                                                          tr("BrReport document (*.brp)"));
    if (fileName.isEmpty())
        return;
    BrDocument doc;
    if (!doc.load(fileName)) {
        QMessageBox::warning(this, tr("Open"), tr("'%1' is not a valid BrReport document.").arg(fileName));
        return;
    }
    setDocument(doc);
}
