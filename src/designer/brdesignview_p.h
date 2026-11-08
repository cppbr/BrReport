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

#ifndef BRDESIGNVIEW_P_H
#define BRDESIGNVIEW_P_H

//
//  W A R N I N G
//  -------------
// This file is not part of the BrReport API. It may change without notice.
//

#include "brdesignerglobal.h"

#include <QGraphicsView>

class BrDesignScene;
class BrGuideItem;
class BrDesignView;

/*! Ruler in millimeters along the view; dragging from it creates a guide. */
class BRREPORT_DESIGNER_PRIVATE_EXPORT BrRuler : public QWidget
{
    Q_OBJECT
public:
    BrRuler(Qt::Orientation orientation, BrDesignView *view);

    /*! Marks the mouse position, in pixels from the start of the ruler (negative = none). */
    void setMarker(int position);

    /*! Millimeters between the numbered ticks for \a pixelsPerMm. */
    static int labelStep(double pixelsPerMm);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    double sceneValue(int pixel) const;
    QPoint viewportPos(QMouseEvent *event) const;
    void moveGuide(const QPoint &viewportPos);

    Qt::Orientation m_orientation;
    BrDesignView *m_view;
    BrGuideItem *m_guide = nullptr;
    int m_marker = -1;
};

/*! View of the report layout with a ruler on the top and another on the left. */
class BRREPORT_DESIGNER_PRIVATE_EXPORT BrDesignView : public QGraphicsView
{
    Q_OBJECT
public:
    static constexpr int RulerSize = 20; // pixels

    explicit BrDesignView(BrDesignScene *scene, QWidget *parent = nullptr);

    BrDesignScene *designScene() const;

    bool rulersVisible() const { return m_rulersVisible; }
    void setRulersVisible(bool visible);
    /*! Repaints the rulers; call it after changing the zoom. */
    void updateRulers();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void placeRulers();

    BrRuler *m_top;
    BrRuler *m_left;
    QWidget *m_corner;
    bool m_rulersVisible = true;
};

#endif // BRDESIGNVIEW_P_H
