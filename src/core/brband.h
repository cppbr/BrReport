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

#ifndef BRBAND_H
#define BRBAND_H

#include "brcontrol.h"

class BrPager;

/*!
 * A horizontal strip of the page holding controls.
 *
 * The band's role is given by its BandType; its width is always the printable
 * width of the page and its height is height(), growing automatically when
 * autoExpand() is on and children stretch (memos). Controls are created with the
 * band as parent: new BrLabel("Name", band).
 */
class BRREPORT_EXPORT BrBand : public BrContainer
{
    Q_OBJECT
public:
    using AfterPrintHandler = std::function<void(BrBand &band)>;

    explicit BrBand(BrPager *pager, Br::BandType type = Br::BandType::Detail, double height = 8.0);

    Br::BandType bandType() const { return m_type; }
    void setBandType(Br::BandType type) { m_type = type; }

    /*! The pager (report, group or sub-detail) that owns this band. */
    BrPager *pager() const;

    Br::PageBreak pageBreak() const { return m_pageBreak; }
    void setPageBreak(Br::PageBreak pageBreak) { m_pageBreak = pageBreak; }

    /*! Number of times the band is printed in sequence (e.g. invoice copies). */
    int carbonCopies() const { return m_carbonCopies; }
    void setCarbonCopies(int copies) { m_carbonCopies = qMax(1, copies); }

    void setAfterPrint(const AfterPrintHandler &handler) { m_afterPrint = handler; }
    void callAfterPrint();

    BrReport *report() const override;

private:
    Br::BandType m_type;
    Br::PageBreak m_pageBreak = Br::PageBreak::None;
    int m_carbonCopies = 1;
    AfterPrintHandler m_afterPrint;
};

#endif // BRBAND_H
