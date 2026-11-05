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

#include "brexpression.h"

#include <QDate>
#include <QTest>

class TestContext : public BrExpressionContext
{
public:
    QVariantMap fields;
    QVariant variable(const QString &name, bool *found) const override
    {
        *found = fields.contains(name);
        return fields.value(name);
    }
};

class TstExpression : public QObject
{
    Q_OBJECT
private slots:
    void arithmetic_data()
    {
        QTest::addColumn<QString>("expr");
        QTest::addColumn<QVariant>("expected");
        QTest::newRow("int") << "1 + 2 * 3" << QVariant(7LL);
        QTest::newRow("parens") << "(1 + 2) * 3" << QVariant(9LL);
        QTest::newRow("division") << "7 / 2" << QVariant(3.5);
        QTest::newRow("modulo") << "7 % 3" << QVariant(1LL);
        QTest::newRow("unary") << "-2 + 5" << QVariant(3LL);
        QTest::newRow("concat") << "'a' + 'b'" << QVariant(QStringLiteral("ab"));
        QTest::newRow("concat amp") << "'n' & 1" << QVariant(QStringLiteral("n1"));
        QTest::newRow("compare") << "2 > 1 and 'x' = 'x'" << QVariant(true);
        QTest::newRow("not") << "not (1 = 1)" << QVariant(false);
        QTest::newRow("neq") << "1 <> 2" << QVariant(true);
        QTest::newRow("if") << "if(1 > 2, 'yes', 'no')" << QVariant(QStringLiteral("no"));
        QTest::newRow("upper") << "upper('abc')" << QVariant(QStringLiteral("ABC"));
        QTest::newRow("substr") << "substr('abcdef', 2, 3)" << QVariant(QStringLiteral("bcd"));
        QTest::newRow("round") << "round(2.345, 2)" << QVariant(2.35);
        QTest::newRow("padleft") << "padleft('7', 3, '0')" << QVariant(QStringLiteral("007"));
        QTest::newRow("coalesce") << "coalesce(null, '', 'x')" << QVariant(QStringLiteral("x"));
        QTest::newRow("escaped quote") << "'it''s'" << QVariant(QStringLiteral("it's"));
    }
    void arithmetic()
    {
        QFETCH(QString, expr);
        QFETCH(QVariant, expected);
        TestContext ctx;
        QString error;
        const QVariant v = BrExpression::evaluate(expr, ctx, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(v, expected);
    }

    void fields()
    {
        TestContext ctx;
        ctx.fields = {{QStringLiteral("Price"), 2.5},
                      {QStringLiteral("Qty"), 4},
                      {QStringLiteral("customer.name"), QStringLiteral("Ana")},
                      {QStringLiteral("Full Name"), QStringLiteral("Ana Silva")}};
        QCOMPARE(BrExpression::evaluate(QStringLiteral("Price * Qty"), ctx).toDouble(), 10.0);
        QCOMPARE(BrExpression::evaluate(QStringLiteral("customer.name"), ctx).toString(),
                 QStringLiteral("Ana"));
        QCOMPARE(BrExpression::evaluate(QStringLiteral("[Full Name]"), ctx).toString(),
                 QStringLiteral("Ana Silva"));
    }

    void errors()
    {
        TestContext ctx;
        QString error;
        BrExpression::evaluate(QStringLiteral("1 +"), ctx, &error);
        QVERIFY(!error.isEmpty());
        BrExpression::evaluate(QStringLiteral("unknownField"), ctx, &error);
        QVERIFY(error.contains(QStringLiteral("unknownField")));
        BrExpression::evaluate(QStringLiteral("nosuchfn(1)"), ctx, &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(!BrExpression(QStringLiteral("'open")).isValid());
    }

    void namesUsed()
    {
        const BrExpression e(QStringLiteral("if(Upper(name) = 'A', [Full Name] + name, str(sale.total + 1))"));
        QCOMPARE(e.identifiers(), QStringList({QStringLiteral("name"), QStringLiteral("Full Name"),
                                               QStringLiteral("sale.total")}));
        QCOMPARE(e.functions(), QStringList({QStringLiteral("if"), QStringLiteral("upper"),
                                             QStringLiteral("str")}));
        QVERIFY(BrExpression(QStringLiteral("true and null")).identifiers().isEmpty());
        QVERIFY(BrExpression(QStringLiteral("1 +")).functions().isEmpty());
    }

    void dates()
    {
        TestContext ctx;
        ctx.fields = {{QStringLiteral("d"), QDate(2026, 3, 15)}};
        QCOMPARE(BrExpression::evaluate(QStringLiteral("year(d)"), ctx).toInt(), 2026);
        QCOMPARE(BrExpression::evaluate(QStringLiteral("formatdate(d, 'dd/MM/yyyy')"), ctx).toString(),
                 QStringLiteral("15/03/2026"));
    }
};

QTEST_GUILESS_MAIN(TstExpression)
#include "tst_expression.moc"
