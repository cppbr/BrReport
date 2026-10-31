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

// Loads the layout from a report file (customers.brt) and only provides the data.
// Edit the .brt file (or pass another one) and run again: no recompilation needed.
//
//   report_file                    uses customers.brt from this folder
//   report_file other.brt --pdf out.pdf

#include "exampleutil.h"

#include <QApplication>
#include <QDate>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString layout = QStringLiteral(BRREPORT_EXAMPLE_DIR "/customers.brt");
    for (const QString &arg : QCoreApplication::arguments().mid(1))
        if (arg.endsWith(QLatin1String(".brt")))
            layout = arg;

    // The data: any BrDataSource registered under the name used in the file.
    QList<QVariantMap> rows;
    const QStringList names = {"Ana Silva", "Bruno Souza", "Carla Lima", "Diego Costa", "Elisa Rocha"};
    const QStringList cities = {"Sao Paulo", "Curitiba", "Recife", "Salvador", "Belem"};
    for (int i = 0; i < 60; ++i)
        rows << QVariantMap{{"id", 2000 + i},
                            {"name", names.at(i % names.size())},
                            {"city", cities.at((i * 3) % cities.size())},
                            {"since", QDate(2020, 1, 1).addDays(i * 23)},
                            {"balance", ((i * 7919) % 50000) / 10.0 - 800.0}};
    BrListDataSource customers(rows);

    BrReport report;
    report.addDataSource(QStringLiteral("customers"), &customers);
    if (!report.load(layout)) {
        QTextStream(stderr) << "Could not load " << layout << ": " << report.errorString() << Qt::endl;
        return 1;
    }
    return runExample(report);
}
