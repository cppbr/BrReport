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

#ifndef BRDESIGNERGLOBAL_H
#define BRDESIGNERGLOBAL_H

#include "brglobal.h"

#if defined(BRREPORT_STATIC)
#  define BRREPORT_DESIGNER_EXPORT
#elif defined(BRREPORT_DESIGNER_LIBRARY)
#  define BRREPORT_DESIGNER_EXPORT Q_DECL_EXPORT
#else
#  define BRREPORT_DESIGNER_EXPORT Q_DECL_IMPORT
#endif

// Internal classes (declared in *_p.h headers) exported only so the tests can use
// them. They are not part of the public API and may change without notice.
#define BRREPORT_DESIGNER_PRIVATE_EXPORT BRREPORT_DESIGNER_EXPORT

#endif // BRDESIGNERGLOBAL_H
