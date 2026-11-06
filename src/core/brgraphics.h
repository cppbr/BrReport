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

#ifndef BRGRAPHICS_H
#define BRGRAPHICS_H

#include "brcontrol.h"

#include <QImage>

/*! Static image. */
class BRREPORT_EXPORT BrImage : public BrControl
{
    Q_OBJECT
    Q_PROPERTY(QImage image READ image WRITE setImage)
    Q_PROPERTY(QString svg READ svg WRITE setSvg)
    Q_PROPERTY(Br::ImageScale scaling READ scaling WRITE setScaling)
public:
    explicit BrImage(BrContainer *parent = nullptr);

    QImage image() const { return m_image; }
    void setImage(const QImage &image) { m_image = image; }
    /*!
     * Loads the image from a file or Qt resource (":/logo.png"). A ".svg" file is
     * kept as vectors, when BrReport is built with QtSvg.
     */
    bool load(const QString &fileName);

    /*! XML of an SVG image; it prints instead of image(), sharp at any size. */
    QString svg() const { return m_svg; }
    void setSvg(const QString &xml) { m_svg = xml; }

    Br::ImageScale scaling() const { return m_scaling; }
    void setScaling(Br::ImageScale scaling) { m_scaling = scaling; }

    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

protected:
    /*! Image to print for the current record. */
    virtual QImage currentImage(BrRenderContext &ctx);
    /*! SVG to print for the current record, empty to print currentImage(). */
    virtual QString currentSvg(BrRenderContext &ctx);

private:
    /*! Where an image of \a natural size goes inside \a rect. */
    QRectF placement(const QRectF &rect, const QSizeF &natural) const;

    QImage m_image;
    QString m_svg;
    Br::ImageScale m_scaling = Br::ImageScale::KeepAspect;
};

/*!
 * Image from a data field. The field may contain the encoded image
 * bytes (QByteArray: PNG, JPEG, SVG...), a QImage/QPixmap, or a file name.
 */
class BRREPORT_EXPORT BrDBImage : public BrImage
{
    Q_OBJECT
    Q_PROPERTY(QString dataField READ dataField WRITE setDataField)
public:
    explicit BrDBImage(BrContainer *parent = nullptr);
    BrDBImage(const QString &dataField, BrContainer *parent);

    QString dataField() const { return m_dataField; }
    void setDataField(const QString &field) { m_dataField = field; }

protected:
    QImage currentImage(BrRenderContext &ctx) override;
    QString currentSvg(BrRenderContext &ctx) override;

private:
    QString m_dataField;
};

/*! Geometric shape. */
class BRREPORT_EXPORT BrDraw : public BrControl
{
    Q_OBJECT
    Q_PROPERTY(Br::DrawKind kind READ kind WRITE setKind)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor)
    Q_PROPERTY(double penWidth READ penWidth WRITE setPenWidth)
    Q_PROPERTY(Qt::PenStyle penStyle READ penStyle WRITE setPenStyle)
    Q_PROPERTY(QColor fillColor READ fillColor WRITE setFillColor)
    Q_PROPERTY(double radius READ radius WRITE setRadius)
public:
    explicit BrDraw(BrContainer *parent = nullptr);
    BrDraw(Br::DrawKind kind, BrContainer *parent);

    Br::DrawKind kind() const { return m_kind; }
    void setKind(Br::DrawKind kind) { m_kind = kind; }

    QColor penColor() const { return m_penColor; }
    void setPenColor(const QColor &color) { m_penColor = color; }

    /*! Line width in mm (0 = hairline). */
    double penWidth() const { return m_penWidth; }
    void setPenWidth(double mm) { m_penWidth = mm; }

    Qt::PenStyle penStyle() const { return m_penStyle; }
    void setPenStyle(Qt::PenStyle style) { m_penStyle = style; }

    /*! Fill color; invalid (default) = no fill. */
    QColor fillColor() const { return m_fillColor; }
    void setFillColor(const QColor &color) { m_fillColor = color; }

    /*! Corner radius for RoundedRectangle, in mm. */
    double radius() const { return m_radius; }
    void setRadius(double mm) { m_radius = mm; }

    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

private:
    Br::DrawKind m_kind = Br::DrawKind::Rectangle;
    QColor m_penColor = Qt::black;
    double m_penWidth = 0.2;
    Qt::PenStyle m_penStyle = Qt::SolidLine;
    QColor m_fillColor;
    double m_radius = 2.0;
};

#endif // BRGRAPHICS_H
