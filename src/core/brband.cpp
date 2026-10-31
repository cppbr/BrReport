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

#include "brband.h"

#include "brpager.h"

BrBand::BrBand(BrPager *pager, Br::BandType type, double height)
    : BrContainer(pager, true), m_type(type)
{
    setGeometry(0, 0, 0, height);
}

BrPager *BrBand::pager() const
{
    return qobject_cast<BrPager *>(parent());
}

BrReport *BrBand::report() const
{
    BrPager *p = pager();
    return p ? p->report() : nullptr;
}

void BrBand::callAfterPrint()
{
    if (m_afterPrint)
        m_afterPrint(*this);
}
