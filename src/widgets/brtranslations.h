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

#ifndef BRTRANSLATIONS_H
#define BRTRANSLATIONS_H

#include "brwidgetsglobal.h"

#include <QLocale>
#include <QStringList>

/*!
 * Translations of the BrReport user interface (preview window and designer).
 *
 * The translations are embedded in the libraries. Call install() once, after
 * creating the QApplication:
 *
 * \code
 * QApplication app(argc, argv);
 * BrTranslations::install();                    // system language
 * BrTranslations::install(QLocale("pt_BR"));    // or a specific one
 * \endcode
 */
class BRREPORT_WIDGETS_EXPORT BrTranslations
{
public:
    /*!
     * Installs the BrReport translation for \a locale and, when available, Qt's
     * own translation (standard dialogs and buttons). Returns true if BrReport has
     * a translation for that language. Calling it again replaces the previous one.
     */
    static bool install(const QLocale &locale = QLocale());

    /*! Removes the translators installed by install(). */
    static void uninstall();

    /*! Languages with a BrReport translation, e.g. {"pt_BR"}. */
    static QStringList availableLanguages();
};

#endif // BRTRANSLATIONS_H
