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

#ifndef BRPRINTING_H
#define BRPRINTING_H

#include "brdocument.h"
#include "brwidgetsglobal.h"

class BrReport;
class QPrinter;
class QWidget;

/*! Printing and preview helpers. */
class BRREPORT_WIDGETS_EXPORT BrPrinting
{
public:
    /*!
     * Prints pages [fromPage, toPage] (1-based, 0 = all) of \a document on an
     * already configured printer.
     */
    static bool print(const BrDocument &document, QPrinter *printer, int fromPage = 0,
                      int toPage = 0);

    /*!
     * Prints a document, optionally showing the system print dialog first.
     * Returns false if canceled or on error.
     */
    static bool print(const BrDocument &document, QWidget *parent = nullptr,
                      bool showDialog = true);

    /*!
     * Asks the user for the report parameters (see BrReport::parameters()) and
     * stores the answers as report variables. Returns false if the user cancels;
     * returns true without asking if the report has no parameters.
     */
    static bool askParameters(BrReport *report, QWidget *parent = nullptr);

    /*!
     * Prepares (if needed, asking for the report parameters first) and prints a
     * report. Returns false if canceled or on error.
     */
    static bool print(BrReport *report, QWidget *parent = nullptr, bool showDialog = true);

    /*!
     * Prepares (if needed, asking for the report parameters first) and shows a
     * report in a modal preview window. A report already prepared is shown as is;
     * call BrReport::clearDocument() to ask again.
     */
    static void preview(BrReport *report, QWidget *parent = nullptr);

    /*! Shows a prepared document (e.g. loaded from a .brp file) in a modal preview window. */
    static void preview(const BrDocument &document, QWidget *parent = nullptr);
};

#endif // BRPRINTING_H
