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

#ifndef BRDESIGNSCENE_P_H
#define BRDESIGNSCENE_P_H

//
//  W A R N I N G
//  -------------
// This file is not part of the BrReport API. It may change without notice.
//

#include "brdesignerglobal.h"

#include "brband.h"
#include "brcontrol.h"
#include "brdocument.h"
#include "brreport.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QIcon>
#include <QJsonObject>
#include <QLineF>
#include <QPointer>
#include <QSet>

class BrBand;
class BrContainer;
class BrControl;
class BrDesignScene;
class BrGuideItem;
class BrReport;

/*! Padlock shown next to protected objects in the Structure panel. */
QIcon brProtectionIcon();

/*! Graphics item showing one report object (control) while designing. */
class BrControlItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };

    BrControlItem(BrControl *control, const QRectF &rect, bool movable, QGraphicsItem *parent);

    int type() const override { return Type; }
    BrControl *control() const { return m_control; }
    QSizeF size() const { return m_size; }
    bool isMovable() const { return m_movable; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    BrDesignScene *designScene() const;
    double handleSize() const;
    int handleAt(const QPointF &pos) const;
    QRectF handleRect(int handle) const;
    void refreshItems();

    QPointer<BrControl> m_control;
    QSizeF m_size;
    bool m_movable;
    bool m_protected;
    QList<BrPageItem> m_items;
    int m_handle = -1;
    bool m_moving = false;
    QPointF m_pressPos;
    QRectF m_startRect;
};

/*! Graphics item showing a band: caption strip above, band area below. */
class BrBandItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 2 };
    static constexpr double CaptionHeight = 4.5; // mm

    BrBandItem(BrBand *band, const QString &caption, double width, int depth, bool resizable);

    int type() const override { return Type; }
    BrBand *band() const { return m_band; }
    QRectF bandRect() const { return QRectF(0, 0, m_width, m_height); }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    BrDesignScene *designScene() const;
    bool onBottomEdge(const QPointF &pos) const;

    QPointer<BrBand> m_band;
    QString m_caption;
    double m_width;
    double m_height;
    int m_depth;
    bool m_resizable;
    bool m_protected;
    QList<BrPageItem> m_items;
    bool m_resizing = false;
};

/*! Line that helps to align objects. It belongs to the design, it is never printed. */
class BrGuideItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 3 };

    /*! A Qt::Vertical guide is at the x \a position, a Qt::Horizontal one at the y. */
    BrGuideItem(Qt::Orientation orientation, double position, const QRectF &area);

    int type() const override { return Type; }
    Qt::Orientation orientation() const { return m_orientation; }
    double position() const;
    void setPosition(double position);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    BrDesignScene *designScene() const;

    Qt::Orientation m_orientation;
    QRectF m_area;
};

/*!
 * Scene showing the report page with its bands and objects, in millimeters.
 * Edits made with the mouse are applied to the report objects and announced
 * with layoutEdited().
 */
class BRREPORT_DESIGNER_PRIVATE_EXPORT BrDesignScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit BrDesignScene(QObject *parent = nullptr);

    void setReport(BrReport *report);
    BrReport *report() const { return m_report; }

    /*! Rebuilds every item from the report (keeps the selected objects selected). */
    void rebuild();

    QList<QObject *> selectedObjects() const;
    void selectObjects(const QList<QObject *> &objects);

    double gridSize() const { return m_grid; }
    void setGridSize(double mm) { m_grid = mm; }
    bool snapToGrid() const { return m_snap; }
    void setSnapToGrid(bool on) { m_snap = on; }
    QPointF snap(const QPointF &p) const;
    double snap(double v) const;

    /*! Align moved and resized objects with the other objects and the guides. */
    bool snapToObjects() const { return m_snapObjects; }
    void setSnapToObjects(bool on) { m_snapObjects = on; }

    /*! Obey the protection of the objects (end users). Takes effect on rebuild(). */
    bool protectionEnforced() const { return m_enforceProtection; }
    void setProtectionEnforced(bool on) { m_enforceProtection = on; }

    /*! Starts moving the selection with the mouse, \a grabber being under it. */
    void beginMove(BrControlItem *grabber);
    /*! Moves the selection by \a delta (mm) from where it started, snapped as a whole. */
    void moveSelection(const QPointF &delta);
    /*! Starts resizing \a item with the mouse. */
    void beginResize(BrControlItem *item);
    /*!
     * Snaps the edge \a value of the item being resized, in the coordinates of its
     * parent. \a axis is the direction the edge moves in.
     */
    double snapEdge(BrControlItem *item, Qt::Orientation axis, double value);
    void clearSnapLines();
    void endInteraction();
    /*! Lines showing what the object in movement is aligned with. */
    QList<QLineF> snapLines() const { return m_snapLines; }

    /*! Adds a guide; finishGuide() stores it in the report. */
    BrGuideItem *addGuide(Qt::Orientation orientation, double position);
    /*! Ends the drag of \a guide, removing it unless \a keep is true. */
    void finishGuide(BrGuideItem *guide, bool keep);
    void clearGuides();
    /*! True if \a scenePos is out of the visible area (the guide was dropped on a ruler). */
    bool isOutOfView(const QPointF &scenePos) const;

    /*! Template of the object to insert on the next click (empty = selection mode). */
    void setInsertTemplate(const QJsonObject &json) { m_insertTemplate = json; }
    QJsonObject insertTemplate() const { return m_insertTemplate; }

    /*! Scale of the first view (pixels per mm), used for handle sizes. */
    double viewScale() const;

    // Called by the items
    void notifyLayoutEdited() { emit layoutEdited(); }
    void notifyRelayout();

signals:
    /*! The user moved/resized objects or bands (the report has been updated). */
    void layoutEdited();
    /*! The user asked to insert \a json at \a pos (mm) inside \a container. */
    void insertRequested(BrContainer *container, const QPointF &pos, const QJsonObject &json);
    void selectedObjectsChanged();

protected:
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;

private:
    void addPager(QObject *pager, int depth, const QString &label, double &y);
    void addBand(BrBand *band, int depth, const QString &label, double &y);
    void addControls(BrContainer *container, const QSizeF &size, QGraphicsItem *parentItem);
    bool containerAt(const QPointF &scenePos, BrContainer **container, QPointF *local) const;
    QList<double> snapTargets(Qt::Orientation axis) const;
    bool snapToTarget(Qt::Orientation axis, const QList<double> &edges, double *offset);
    void storeGuides();

    struct MovingItem
    {
        BrControlItem *item;
        QPointF start;
    };

    QPointer<BrReport> m_report;
    double m_grid = 1.0;
    bool m_snap = true;
    bool m_snapObjects = true;
    bool m_enforceProtection = true;
    QList<MovingItem> m_moving;
    QRectF m_movingBounds;
    QSet<QGraphicsItem *> m_snapExcluded; // what is being moved or resized
    QGraphicsItem *m_snapBand = nullptr;  // the band of the object in movement
    QList<QLineF> m_snapLines;
    QJsonObject m_insertTemplate;
    bool m_rebuilding = false;
};

/*! MIME type used to drag report objects (JSON templates) into the scene. */
#define BRREPORT_OBJECT_MIME "application/x-brreport-object"

#endif // BRDESIGNSCENE_P_H
