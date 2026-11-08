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

#include "brtranslations.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QPointer>
#include <QTranslator>

namespace {

const char *const kResourceDir = ":/brreport/translations";

QPointer<QTranslator> &brTranslator()
{
    static QPointer<QTranslator> t;
    return t;
}

QPointer<QTranslator> &qtTranslator()
{
    static QPointer<QTranslator> t;
    return t;
}

QString qtTranslationsPath()
{
    return QLibraryInfo::path(QLibraryInfo::TranslationsPath);
}

} // namespace

static void initResources()
{
#ifdef BRREPORT_HAS_TRANSLATIONS
    Q_INIT_RESOURCE(brreport_translations);
#endif
}

bool BrTranslations::install(const QLocale &locale)
{
    initResources();
    uninstall();
    if (!QCoreApplication::instance())
        return false;

    auto *qt = new QTranslator(QCoreApplication::instance());
    if (qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), qtTranslationsPath())) {
        QCoreApplication::installTranslator(qt);
        qtTranslator() = qt;
    } else {
        delete qt;
    }

    auto *br = new QTranslator(QCoreApplication::instance());
    if (br->load(locale, QStringLiteral("brreport"), QStringLiteral("_"), QString::fromLatin1(kResourceDir))) {
        QCoreApplication::installTranslator(br);
        brTranslator() = br;
        return true;
    }
    delete br;
    return false;
}

void BrTranslations::uninstall()
{
    for (QPointer<QTranslator> *t : {&brTranslator(), &qtTranslator()}) {
        if (*t) {
            QCoreApplication::removeTranslator(*t);
            delete t->data();
        }
    }
}

QStringList BrTranslations::availableLanguages()
{
    initResources();
    QStringList languages;
    const QStringList files = QDir(QString::fromLatin1(kResourceDir)).entryList({QStringLiteral("brreport_*.qm")});
    for (QString f : files) {
        f.remove(QStringLiteral("brreport_"));
        f.chop(3);
        languages << f;
    }
    return languages;
}
