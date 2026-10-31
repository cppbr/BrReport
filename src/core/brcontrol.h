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

#ifndef BRCONTROL_H
#define BRCONTROL_H

#include "brdocument.h"
#include "brglobal.h"

#include <QColor>
#include <QFont>
#include <QList>
#include <QObject>
#include <QRectF>

#include <functional>

class BrBand;
class BrContainer;
class BrPager;
class BrRenderContext;
class BrReport;

/*! Frame lines drawn around a control. */
struct BRREPORT_EXPORT BrBorders
{
    bool left = false;
    bool top = false;
    bool right = false;
    bool bottom = false;
    double width = 0.2; //!< mm
    QColor color = Qt::black;
    Qt::PenStyle style = Qt::SolidLine;

    bool any() const { return left || top || right || bottom; }
    /*! All four sides. */
    static BrBorders all(double width = 0.2, const QColor &color = Qt::black);
    /*! Only the bottom side (common for column headers). */
    static BrBorders bottomOnly(double width = 0.2, const QColor &color = Qt::black);
};

/*!
 * Base class of every printable element.
 *
 * Controls live inside a container (a band or a panel), which owns them through
 * the QObject parent/child mechanism. Geometry is relative to the container, in mm.
 */
class BRREPORT_EXPORT BrControl : public QObject
{
    Q_OBJECT
public:
    /*!
     * Called once per print. Change the control (text, colors, visibility...) or
     * set \a printIt to false to skip it.
     */
    using BeforePrintHandler = std::function<void(BrControl &control, bool &printIt)>;

    explicit BrControl(BrContainer *parent = nullptr);
    ~BrControl() override;

    QRectF geometry() const { return m_geometry; }
    void setGeometry(const QRectF &mm) { m_geometry = mm; }
    void setGeometry(double x, double y, double width, double height)
    {
        m_geometry = QRectF(x, y, width, height);
    }
    double x() const { return m_geometry.x(); }
    double y() const { return m_geometry.y(); }
    double width() const { return m_geometry.width(); }
    double height() const { return m_geometry.height(); }
    void setHeight(double h) { m_geometry.setHeight(h); }
    void setWidth(double w) { m_geometry.setWidth(w); }

    Br::Align align() const { return m_align; }
    void setAlign(Br::Align align) { m_align = align; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    /*! Background color; an invalid color (default) means transparent. */
    QColor background() const { return m_background; }
    void setBackground(const QColor &color) { m_background = color; }

    BrBorders borders() const { return m_borders; }
    void setBorders(const BrBorders &borders) { m_borders = borders; }

    /*! Font, inherited from the parent (container, then report) unless set. */
    QFont font() const;
    void setFont(const QFont &font);
    void resetFont() { m_hasFont = false; }
    bool hasOwnFont() const { return m_hasFont; }

    /*! Text/foreground color (default black). */
    QColor color() const { return m_color; }
    void setColor(const QColor &color) { m_color = color; }

    void setBeforePrint(const BeforePrintHandler &handler) { m_beforePrint = handler; }

    /*!
     * Stretches the control down to the bottom of its band/panel when the band
     * grows (keeps table cells and vertical lines aligned). Default false.
     */
    bool stretchToBand() const { return m_stretchToBand; }
    void setStretchToBand(bool on) { m_stretchToBand = on; }

    BrContainer *parentContainer() const;
    /*! The band this control belongs to (nullptr if not inside a band). */
    BrBand *band() const;
    /*! The report this control belongs to. */
    virtual BrReport *report() const;

    // -- Engine interface (used by BrReport while preparing; useful for custom controls) --

    /*! Runs the BeforePrint handler. Returns whether the control prints. */
    virtual bool checkPrint(BrRenderContext &ctx);
    bool willPrint() const { return m_willPrint; }
    /*! Computes content for the current record/page (text, values...). Can be called again. */
    virtual void prepare(BrRenderContext &ctx);
    /*!
     * Height needed when the control has the given width. Return a value greater
     * than height() to stretch (and expand the band, when allowed).
     */
    virtual double measureHeight(BrRenderContext &ctx, double width);
    /*! Emits the drawing items for \a rect (absolute page mm). */
    virtual void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page);

protected:
    /*! For subclasses whose parent is not a container (e.g. bands owned by a pager). */
    BrControl(QObject *parent, bool);

    /*! Draws background and borders. */
    void renderFrame(const QRectF &rect, BrPage &page) const;

private:
    QRectF m_geometry = QRectF(0, 0, 30, 5);
    Br::Align m_align = Br::Align::None;
    bool m_visible = true;
    bool m_willPrint = true;
    bool m_stretchToBand = false;
    QColor m_background;
    QColor m_color = Qt::black;
    BrBorders m_borders;
    QFont m_font;
    bool m_hasFont = false;
    BeforePrintHandler m_beforePrint;
};

/*!
 * A control that holds other controls and lays them out (base of BrBand and BrPanel).
 */
class BRREPORT_EXPORT BrContainer : public BrControl
{
    Q_OBJECT
public:
    explicit BrContainer(BrContainer *parent = nullptr);

    /*! Child controls, in creation order. */
    QList<BrControl *> controls() const;

    /*! Grows to fit stretched children (default true). */
    bool autoExpand() const { return m_autoExpand; }
    void setAutoExpand(bool on) { m_autoExpand = on; }

    /*!
     * When a child grows (stretched memo), move down the children placed below it
     * that share part of its horizontal span (default true).
     */
    bool shiftOnStretch() const { return m_shiftOnStretch; }
    void setShiftOnStretch(bool on) { m_shiftOnStretch = on; }

    bool checkPrint(BrRenderContext &ctx) override;
    void prepare(BrRenderContext &ctx) override;
    double measureHeight(BrRenderContext &ctx, double width) override;
    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

    struct Placement
    {
        BrControl *control;
        QRectF rect; //!< relative to the container
    };
    /*!
     * Lays out printable children for a container of the given size. Returns the
     * resulting height (grown if autoExpand() and children need more room).
     */
    double layoutChildren(BrRenderContext &ctx, double width, double height, bool expand,
                          QList<Placement> *placements);

protected:
    BrContainer(QObject *parent, bool);

private:
    bool m_autoExpand = true;
    bool m_shiftOnStretch = true;
};

/*! Plain container with optional background and borders. */
class BRREPORT_EXPORT BrPanel : public BrContainer
{
    Q_OBJECT
public:
    explicit BrPanel(BrContainer *parent = nullptr);
};

#endif // BRCONTROL_H
