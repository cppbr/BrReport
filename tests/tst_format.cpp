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

#include "brformat.h"

#include <QDate>
#include <QTest>

class TstFormat : public QObject
{
    Q_OBJECT
private slots:
    void numbers_data()
    {
        QTest::addColumn<double>("value");
        QTest::addColumn<QString>("mask");
        QTest::addColumn<QString>("locale");
        QTest::addColumn<QString>("expected");
        QTest::newRow("en group") << 1234.567 << "#,##0.00" << "en_US" << "1,234.57";
        QTest::newRow("pt group") << 1234.567 << "#,##0.00" << "pt_BR" << "1.234,57";
        QTest::newRow("no group") << 1234.5 << "0.00" << "en_US" << "1234.50";
        QTest::newRow("optional") << 2.5 << "0.##" << "en_US" << "2.5";
        QTest::newRow("integer") << 3.0 << "0.##" << "en_US" << "3";
        QTest::newRow("leading") << 7.0 << "000" << "en_US" << "007";
        QTest::newRow("prefix") << 10.0 << "'R$ '#,##0.00" << "pt_BR" << "R$ 10,00";
        QTest::newRow("negative") << -5.0 << "#,##0.00" << "en_US" << "-5.00";
        QTest::newRow("neg section") << -5.0 << "0.00;(0.00)" << "en_US" << "(5.00)";
        QTest::newRow("zero section") << 0.0 << "0.00;(0.00);'-'" << "en_US" << "-";
        QTest::newRow("millions") << 1234567.0 << "#,##0" << "en_US" << "1,234,567";
        QTest::newRow("neg zero") << -0.001 << "0.00" << "en_US" << "0.00";
    }
    void numbers()
    {
        QFETCH(double, value);
        QFETCH(QString, mask);
        QFETCH(QString, locale);
        QFETCH(QString, expected);
        QCOMPARE(BrFormat::formatNumber(value, mask, QLocale(locale)), expected);
    }

    void values()
    {
        const QLocale en(QStringLiteral("en_US"));
        QCOMPARE(BrFormat::format(QVariant(), QString(), en), QString());
        QCOMPARE(BrFormat::format(42, QString(), en), QStringLiteral("42"));
        QCOMPARE(BrFormat::format(2.5, QString(), en), QStringLiteral("2.5"));
        QCOMPARE(BrFormat::format(QDate(2026, 1, 2), QStringLiteral("dd/MM/yyyy"), en),
                 QStringLiteral("02/01/2026"));
        QCOMPARE(BrFormat::format(QStringLiteral("12.5"), QStringLiteral("0.00"), en),
                 QStringLiteral("12.50"));
        QCOMPARE(BrFormat::format(QStringLiteral("2026-10-16"), QStringLiteral("dd/MM/yyyy"), en),
                 QStringLiteral("16/10/2026"));
        QCOMPARE(BrFormat::format(QStringLiteral("abc"), QStringLiteral("0.00"), en),
                 QStringLiteral("abc"));
    }
};

QTEST_GUILESS_MAIN(TstFormat)
#include "tst_format.moc"
