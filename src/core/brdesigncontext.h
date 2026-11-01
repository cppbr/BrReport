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

#ifndef BRDESIGNCONTEXT_H
#define BRDESIGNCONTEXT_H

#include "brdocument.h"
#include "brrendercontext.h"

class BrControl;
class BrContainer;

/*!
 * Render context used by report designers: no data, placeholders for fields
 * ("[name]") and page numbers ("#"/"##").
 */
class BRREPORT_EXPORT BrDesignContext : public BrRenderContext
{
public:
    explicit BrDesignContext(BrReport *report = nullptr);

    BrReport *report() const override { return m_report; }
    int pageNumber() const override { return 1; }
    int documentPageNumber() const override { return 1; }
    QString pageNumberText() const override { return QStringLiteral("#"); }
    QString pageCountText() const override { return QStringLiteral("##"); }
    QString documentPageCountText() const override { return QStringLiteral("##"); }
    int recNo() const override { return 1; }
    int detailCount() const override { return 1; }
    int carbonCopy() const override { return 1; }
    QVariant fieldValue(const QString &name, bool *found = nullptr) const override;
    QDateTime reportDateTime() const override;
    QVariant variable(const QString &name, bool *found) const override;
    QLocale locale() const override;
    bool isDesignMode() const override { return true; }

    /*!
     * Drawing items of a single object at the origin, with the given size. For
     * containers (bands, panels) only the frame is drawn, not the children.
     */
    static QList<BrPageItem> renderControl(BrControl *control, const QSizeF &size);

    /*!
     * Rectangles of the children of a container laid out as in a printed report,
     * for a container of the given size (relative to the container).
     */
    static QList<QPair<BrControl *, QRectF>> layout(BrContainer *container, const QSizeF &size);

private:
    BrReport *m_report;
};

#endif // BRDESIGNCONTEXT_H
