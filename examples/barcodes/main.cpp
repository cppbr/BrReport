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

// Product labels: QR Code, Code 128, EAN-13 and Interleaved 2 of 5 barcodes, shapes,
// rotated text, images and carbon copies.

#include "exampleutil.h"

#include <QApplication>
#include <QPainter>

namespace {

QImage makeLogo()
{
    QImage img(200, 200, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(0x00, 0x9c, 0x3b));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(10, 10, 180, 180), 30, 30);
    p.setBrush(QColor(0xfe, 0xdf, 0x00));
    p.drawPolygon(QPolygonF({QPointF(100, 30), QPointF(175, 100), QPointF(100, 170), QPointF(25, 100)}));
    p.setBrush(QColor(0x00, 0x27, 0x76));
    p.drawEllipse(QPointF(100, 100), 42, 42);
    return img;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BrListDataSource products({
        {{"sku", "BR-0001"}, {"name", "Organic coffee 500 g"}, {"ean", "789100031550"}, {"price", 32.9}},
        {{"sku", "BR-0002"}, {"name", "Brown sugar 1 kg"}, {"ean", "789100031551"}, {"price", 7.49}},
        {{"sku", "BR-0003"}, {"name", "Cassava flour 1 kg"}, {"ean", "789100031552"}, {"price", 9.99}},
        {{"sku", "BR-0004"}, {"name", "Guarana soda 2 L"}, {"ean", "789100031553"}, {"price", 8.5}},
    });

    BrReport report;
    report.setTitle(QStringLiteral("Barcodes and graphics"));
    report.setDataSource(&products);
    report.setLocale(QLocale(QLocale::Portuguese, QLocale::Brazil));

    BrBand *title = report.addBand(Br::BandType::Title, 34);
    auto *caption = new BrSystemInfo(Br::InfoType::Title, title);
    caption->setGeometry(0, 0, 150, 12);
    caption->setFont(QFont(QStringLiteral("Arial"), 16, QFont::Bold));

    // Interleaved 2 of 5, as printed on Brazilian bank slips ("boletos")
    auto *bank = new BrBarcode(Br::BarcodeType::Interleaved2of5,
                               QStringLiteral("23793381286000001234567890123456789012345678"), title);
    bank->setGeometry(0, 14, 103, 13);
    bank->setShowText(false);

    // QR Code with a PIX-like payment payload
    auto *pix = new BrBarcode(Br::BarcodeType::QrCode,
                              QStringLiteral("00020126580014BR.GOV.BCB.PIX0136123e4567-e12d-3a45-"
                                             "b678-4266141740005204000053039865406123.455802BR"
                                             "5913BRREPORT LTDA6009SAO PAULO62070503***6304B14F"),
                              title);
    pix->setGeometry(160, 0, 30, 30);
    pix->setQrErrorCorrection(Br::QrErrorCorrection::Medium);
    auto *pixLabel = new BrLabel(QStringLiteral("Pay with PIX"), title);
    pixLabel->setGeometry(110, 10, 48, 8);
    pixLabel->setAlignment(Br::TextAlign::Right);

    // One card per product
    BrBand *card = report.addBand(Br::BandType::Detail, 48);
    auto *frame = new BrDraw(Br::DrawKind::RoundedRectangle, card);
    frame->setGeometry(0, 2, 190, 44);
    frame->setPenWidth(0.4);

    auto *logo = new BrImage(card);
    logo->setImage(makeLogo());
    logo->setGeometry(4, 6, 24, 24);

    auto *name = new BrDBText(QStringLiteral("name"), card);
    name->setGeometry(32, 5, 100, 9);
    name->setFont(QFont(QStringLiteral("Arial"), 13, QFont::Bold));
    auto *sku = new BrDBText(QStringLiteral("sku"), card);
    sku->setGeometry(32, 14, 60, 6);
    sku->setPrefix(QStringLiteral("SKU: "));
    auto *price = new BrDBText(QStringLiteral("price"), card);
    price->setGeometry(32, 21, 60, 10);
    price->setDisplayFormat(QStringLiteral("'R$ '#,##0.00"));
    price->setFont(QFont(QStringLiteral("Arial"), 16, QFont::Bold));
    price->setColor(QColor(0x00, 0x7a, 0x33));

    auto *ean = new BrDBBarcode(Br::BarcodeType::Ean13, QStringLiteral("ean"), card);
    ean->setGeometry(100, 16, 40, 22);
    ean->setModuleWidth(0.33);
    auto *code128 = new BrDBBarcode(Br::BarcodeType::Code128, QStringLiteral("sku"), card);
    code128->setGeometry(142, 16, 26, 22);
    code128->setFont(QFont(QStringLiteral("Arial"), 7));

    auto *vertical = new BrLabel(QStringLiteral("SAMPLE"), card);
    vertical->setGeometry(181, 6, 7, 36);
    vertical->setAngle(90);
    vertical->setAlignment(Br::TextAlign::Center);
    vertical->setColor(QColor(0xc0, 0x39, 0x2b));
    vertical->setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));

    auto *ellipse = new BrDraw(Br::DrawKind::Ellipse, card);
    ellipse->setGeometry(4, 33, 10, 10);
    ellipse->setFillColor(QColor(0xfe, 0xdf, 0x00));
    auto *triangle = new BrDraw(Br::DrawKind::Triangle, card);
    triangle->setGeometry(17, 33, 10, 10);
    triangle->setFillColor(QColor(0x00, 0x27, 0x76));
    auto *diag = new BrDraw(Br::DrawKind::DiagonalDown, card);
    diag->setGeometry(30, 33, 10, 10);
    diag->setPenStyle(Qt::DashLine);

    // Two copies of every card: "1/2" and "2/2"
    card->setCarbonCopies(2);
    auto *copy = new BrSystemInfo(Br::InfoType::CarbonCopy, card);
    copy->setGeometry(160, 5, 18, 6);
    copy->setAlignment(Br::TextAlign::Right);
    copy->setFormat(QStringLiteral("copy %1/2"));
    copy->setFont(QFont(QStringLiteral("Arial"), 7));

    return runExample(report);
}
