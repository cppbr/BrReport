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

#ifndef BRREPORTCORE_H
#define BRREPORTCORE_H

// Convenience header: includes the whole BrReport core API.

#include "brglobal.h"
#include "brband.h"
#include "brbarcode.h"
#include "brcontrol.h"
#include "brdatasource.h"
#include "brdocument.h"
#include "brexport.h"
#include "brexpression.h"
#include "brformat.h"
#include "brgraphics.h"
#include "brpager.h"
#include "brqrcode.h"
#include "brrendercontext.h"
#include "brrenderer.h"
#include "brreport.h"
#include "brtextcontrols.h"
#include "brtextmetrics.h"
#ifdef BRREPORT_HAS_SQL
#include "brsqldatasource.h"
#endif

#endif // BRREPORTCORE_H
