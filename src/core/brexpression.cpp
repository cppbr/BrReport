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

#include "brformat.h"

#include <QDate>
#include <QDateTime>
#include <QVector>

#include <cmath>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

BrExpressionContext::~BrExpressionContext() = default;

QVariant BrExpressionContext::variable(const QString &, bool *found) const
{
    *found = false;
    return {};
}

QVariant BrExpressionContext::function(const QString &, const QVariantList &, bool *found) const
{
    *found = false;
    return {};
}

QLocale BrExpressionContext::locale() const
{
    return QLocale();
}

// ---------------------------------------------------------------------------
// AST
// ---------------------------------------------------------------------------

struct BrExpression::Node
{
    enum Type { Literal, Identifier, Unary, Binary, Call };
    Type type = Literal;
    QVariant value;   // literal
    QString name;     // identifier / function / operator
    QVector<std::shared_ptr<const Node>> children;
};

namespace {

using NodePtr = std::shared_ptr<const BrExpression::Node>;

struct ParseError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

struct Token
{
    enum Kind { End, Number, String, Ident, Op, LParen, RParen, Comma };
    Kind kind = End;
    QString text;
    QVariant value;
    int pos = 0;
};

class Lexer
{
public:
    explicit Lexer(const QString &s) : m_s(s) {}

    QVector<Token> tokenize()
    {
        QVector<Token> tokens;
        while (true) {
            skipSpaces();
            Token t;
            t.pos = m_i;
            if (m_i >= m_s.size()) {
                tokens << t;
                break;
            }
            const QChar c = m_s.at(m_i);
            if (c.isDigit() || (c == QLatin1Char('.') && peek(1).isDigit())) {
                int start = m_i;
                while (m_i < m_s.size() && (m_s.at(m_i).isDigit() || m_s.at(m_i) == QLatin1Char('.')))
                    ++m_i;
                const QString num = m_s.mid(start, m_i - start);
                bool ok = false;
                if (num.contains(QLatin1Char('.')))
                    t.value = num.toDouble(&ok);
                else {
                    t.value = num.toLongLong(&ok);
                    if (!ok)
                        t.value = num.toDouble(&ok);
                }
                if (!ok)
                    throw ParseError("invalid number");
                t.kind = Token::Number;
                t.text = num;
            } else if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
                const QChar quote = c;
                ++m_i;
                QString str;
                bool closed = false;
                while (m_i < m_s.size()) {
                    const QChar ch = m_s.at(m_i++);
                    if (ch == quote) {
                        if (m_i < m_s.size() && m_s.at(m_i) == quote) { // doubled quote = escape
                            str += quote;
                            ++m_i;
                            continue;
                        }
                        closed = true;
                        break;
                    }
                    str += ch;
                }
                if (!closed)
                    throw ParseError("unterminated string");
                t.kind = Token::String;
                t.value = str;
            } else if (c == QLatin1Char('[')) {
                const int end = m_s.indexOf(QLatin1Char(']'), m_i);
                if (end < 0)
                    throw ParseError("missing ']'");
                t.kind = Token::Ident;
                t.text = m_s.mid(m_i + 1, end - m_i - 1).trimmed();
                m_i = end + 1;
            } else if (c.isLetter() || c == QLatin1Char('_')) {
                const int start = m_i;
                while (m_i < m_s.size()
                       && (m_s.at(m_i).isLetterOrNumber() || m_s.at(m_i) == QLatin1Char('_')
                           || m_s.at(m_i) == QLatin1Char('.')))
                    ++m_i;
                t.text = m_s.mid(start, m_i - start);
                const QString lower = t.text.toLower();
                if (lower == QLatin1String("and") || lower == QLatin1String("or")
                    || lower == QLatin1String("not")) {
                    t.kind = Token::Op;
                    t.text = lower;
                } else {
                    t.kind = Token::Ident;
                }
            } else if (c == QLatin1Char('(')) {
                t.kind = Token::LParen;
                ++m_i;
            } else if (c == QLatin1Char(')')) {
                t.kind = Token::RParen;
                ++m_i;
            } else if (c == QLatin1Char(',') || c == QLatin1Char(';')) {
                t.kind = Token::Comma;
                ++m_i;
            } else {
                static const char *ops[] = {"<>", "!=", "==", "<=", ">=", "&&", "||", "+", "-",
                                            "*",  "/",  "%",  "=",  "<",  ">",  "!",  "&"};
                bool matched = false;
                for (const char *op : ops) {
                    const QString o = QString::fromLatin1(op);
                    if (m_s.mid(m_i, o.size()) == o) {
                        t.kind = Token::Op;
                        t.text = o;
                        m_i += o.size();
                        matched = true;
                        break;
                    }
                }
                if (!matched)
                    throw ParseError("unexpected character");
                if (t.text == QLatin1String("&&"))
                    t.text = QStringLiteral("and");
                else if (t.text == QLatin1String("||"))
                    t.text = QStringLiteral("or");
                else if (t.text == QLatin1String("!"))
                    t.text = QStringLiteral("not");
                else if (t.text == QLatin1String("=="))
                    t.text = QStringLiteral("=");
                else if (t.text == QLatin1String("!="))
                    t.text = QStringLiteral("<>");
            }
            tokens << t;
        }
        return tokens;
    }

private:
    QChar peek(int n) const { return m_i + n < m_s.size() ? m_s.at(m_i + n) : QChar(); }
    void skipSpaces()
    {
        while (m_i < m_s.size() && m_s.at(m_i).isSpace())
            ++m_i;
    }
    QString m_s;
    int m_i = 0;
};

class Parser
{
public:
    explicit Parser(const QVector<Token> &tokens) : m_t(tokens) {}

    NodePtr parse()
    {
        NodePtr n = parseOr();
        if (cur().kind != Token::End)
            throw ParseError("unexpected token");
        return n;
    }

private:
    const Token &cur() const { return m_t.at(m_p); }
    bool isOp(const char *op) const
    {
        return cur().kind == Token::Op && cur().text == QLatin1String(op);
    }
    static NodePtr binary(const QString &op, NodePtr a, NodePtr b)
    {
        auto n = std::make_shared<BrExpression::Node>();
        n->type = BrExpression::Node::Binary;
        n->name = op;
        n->children << a << b;
        return n;
    }
    static NodePtr unary(const QString &op, NodePtr a)
    {
        auto n = std::make_shared<BrExpression::Node>();
        n->type = BrExpression::Node::Unary;
        n->name = op;
        n->children << a;
        return n;
    }

    NodePtr parseOr()
    {
        NodePtr n = parseAnd();
        while (isOp("or")) {
            ++m_p;
            n = binary(QStringLiteral("or"), n, parseAnd());
        }
        return n;
    }
    NodePtr parseAnd()
    {
        NodePtr n = parseNot();
        while (isOp("and")) {
            ++m_p;
            n = binary(QStringLiteral("and"), n, parseNot());
        }
        return n;
    }
    NodePtr parseNot()
    {
        if (isOp("not")) {
            ++m_p;
            return unary(QStringLiteral("not"), parseNot());
        }
        return parseCompare();
    }
    NodePtr parseCompare()
    {
        NodePtr n = parseAdd();
        static const char *ops[] = {"=", "<>", "<", ">", "<=", ">="};
        for (const char *op : ops) {
            if (isOp(op)) {
                ++m_p;
                return binary(QString::fromLatin1(op), n, parseAdd());
            }
        }
        return n;
    }
    NodePtr parseAdd()
    {
        NodePtr n = parseMul();
        while (isOp("+") || isOp("-") || isOp("&")) {
            const QString op = cur().text;
            ++m_p;
            n = binary(op, n, parseMul());
        }
        return n;
    }
    NodePtr parseMul()
    {
        NodePtr n = parseUnary();
        while (isOp("*") || isOp("/") || isOp("%")) {
            const QString op = cur().text;
            ++m_p;
            n = binary(op, n, parseUnary());
        }
        return n;
    }
    NodePtr parseUnary()
    {
        if (isOp("-")) {
            ++m_p;
            return unary(QStringLiteral("-"), parseUnary());
        }
        if (isOp("+")) {
            ++m_p;
            return parseUnary();
        }
        return parsePrimary();
    }
    NodePtr parsePrimary()
    {
        const Token t = cur();
        auto n = std::make_shared<BrExpression::Node>();
        switch (t.kind) {
        case Token::Number:
        case Token::String:
            ++m_p;
            n->type = BrExpression::Node::Literal;
            n->value = t.value;
            return n;
        case Token::LParen: {
            ++m_p;
            NodePtr inner = parseOr();
            if (cur().kind != Token::RParen)
                throw ParseError("missing ')'");
            ++m_p;
            return inner;
        }
        case Token::Ident: {
            ++m_p;
            const QString lower = t.text.toLower();
            if (cur().kind == Token::LParen) {
                ++m_p;
                n->type = BrExpression::Node::Call;
                n->name = lower;
                if (cur().kind != Token::RParen) {
                    while (true) {
                        n->children << parseOr();
                        if (cur().kind == Token::Comma) {
                            ++m_p;
                            continue;
                        }
                        break;
                    }
                }
                if (cur().kind != Token::RParen)
                    throw ParseError("missing ')' in function call");
                ++m_p;
                return n;
            }
            n->type = BrExpression::Node::Literal;
            if (lower == QLatin1String("true")) {
                n->value = true;
            } else if (lower == QLatin1String("false")) {
                n->value = false;
            } else if (lower == QLatin1String("null")) {
                n->value = QVariant();
            } else {
                n->type = BrExpression::Node::Identifier;
                n->name = t.text;
            }
            return n;
        }
        default:
            throw ParseError("unexpected token");
        }
    }

    QVector<Token> m_t;
    int m_p = 0;
};

// ---------------------------------------------------------------------------
// Evaluation helpers
// ---------------------------------------------------------------------------

bool isNull(const QVariant &v)
{
    return !v.isValid() || v.isNull();
}

double toNumber(const QVariant &v)
{
    if (isNull(v))
        return 0.0;
    if (v.userType() == QMetaType::Bool)
        return v.toBool() ? 1.0 : 0.0;
    if (v.userType() == QMetaType::QString)
        return v.toString().trimmed().toDouble();
    return v.toDouble();
}

bool toBool(const QVariant &v)
{
    if (isNull(v))
        return false;
    if (v.userType() == QMetaType::QString) {
        const QString s = v.toString().trimmed().toLower();
        return !(s.isEmpty() || s == QLatin1String("false") || s == QLatin1String("0"));
    }
    if (BrFormat::isNumeric(v))
        return toNumber(v) != 0.0;
    return v.toBool();
}

bool isIntegral(const QVariant &v)
{
    const int t = v.userType();
    return t == QMetaType::Int || t == QMetaType::LongLong || t == QMetaType::UInt
           || t == QMetaType::ULongLong || t == QMetaType::Short || t == QMetaType::Long;
}

QVariant numberResult(double d, bool integral)
{
    if (integral && std::abs(d) < 9.0e15)
        return QVariant(qlonglong(d));
    return QVariant(d);
}

bool isStringLike(const QVariant &v)
{
    return v.userType() == QMetaType::QString || v.userType() == QMetaType::QChar;
}

int compare(const QVariant &a, const QVariant &b)
{
    if (isNull(a) || isNull(b)) {
        if (isNull(a) && isNull(b))
            return 0;
        if (isNull(a))
            return isStringLike(b) && b.toString().isEmpty() ? 0 : -1;
        return isStringLike(a) && a.toString().isEmpty() ? 0 : 1;
    }
    if (BrFormat::isDateTime(a) && BrFormat::isDateTime(b)) {
        const QDateTime da = a.userType() == QMetaType::QDate ? a.toDate().startOfDay() : a.toDateTime();
        const QDateTime db = b.userType() == QMetaType::QDate ? b.toDate().startOfDay() : b.toDateTime();
        return da < db ? -1 : (da > db ? 1 : 0);
    }
    const bool aNum = BrFormat::isNumeric(a) || a.userType() == QMetaType::Bool;
    const bool bNum = BrFormat::isNumeric(b) || b.userType() == QMetaType::Bool;
    if (aNum && bNum) {
        const double x = toNumber(a), y = toNumber(b);
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    return QString::compare(a.toString(), b.toString());
}

QVariant evalNode(const BrExpression::Node &n, const BrExpressionContext &ctx);

QVariant callBuiltin(const QString &name, const QVariantList &a, const BrExpressionContext &ctx,
                     bool *found)
{
    *found = true;
    auto arg = [&](int i) { return i < a.size() ? a.at(i) : QVariant(); };
    auto str = [&](int i) { return arg(i).toString(); };
    auto num = [&](int i) { return toNumber(arg(i)); };

    if (name == QLatin1String("upper"))
        return str(0).toUpper();
    if (name == QLatin1String("lower"))
        return str(0).toLower();
    if (name == QLatin1String("trim"))
        return str(0).trimmed();
    if (name == QLatin1String("len") || name == QLatin1String("length"))
        return qlonglong(str(0).size());
    if (name == QLatin1String("left"))
        return str(0).left(int(num(1)));
    if (name == QLatin1String("right"))
        return str(0).right(int(num(1)));
    if (name == QLatin1String("substr") || name == QLatin1String("copy")) {
        const int start = qMax(0, int(num(1)) - 1);
        return a.size() > 2 ? str(0).mid(start, int(num(2))) : str(0).mid(start);
    }
    if (name == QLatin1String("replace"))
        return str(0).replace(str(1), str(2));
    if (name == QLatin1String("padleft") || name == QLatin1String("padright")) {
        const QString fill = a.size() > 2 && !str(2).isEmpty() ? str(2) : QStringLiteral(" ");
        return name == QLatin1String("padleft") ? str(0).rightJustified(int(num(1)), fill.at(0))
                                                : str(0).leftJustified(int(num(1)), fill.at(0));
    }
    if (name == QLatin1String("round")) {
        const double f = std::pow(10.0, a.size() > 1 ? num(1) : 0.0);
        const double r = std::round(num(0) * f) / f;
        return a.size() > 1 && num(1) > 0 ? QVariant(r) : numberResult(r, true);
    }
    if (name == QLatin1String("trunc") || name == QLatin1String("int"))
        return numberResult(std::trunc(num(0)), true);
    if (name == QLatin1String("abs"))
        return numberResult(std::abs(num(0)), isIntegral(arg(0)));
    if (name == QLatin1String("min") || name == QLatin1String("max")) {
        if (a.isEmpty())
            return {};
        QVariant best = a.first();
        for (const QVariant &v : a) {
            const int c = compare(v, best);
            if ((name == QLatin1String("min") && c < 0) || (name == QLatin1String("max") && c > 0))
                best = v;
        }
        return best;
    }
    if (name == QLatin1String("str"))
        return BrFormat::format(arg(0), a.size() > 1 ? str(1) : QString(), ctx.locale());
    if (name == QLatin1String("num"))
        return num(0);
    if (name == QLatin1String("isnull") || name == QLatin1String("isempty"))
        return isNull(arg(0)) || (isStringLike(arg(0)) && str(0).isEmpty());
    if (name == QLatin1String("coalesce") || name == QLatin1String("nvl")) {
        for (const QVariant &v : a)
            if (!isNull(v) && !(isStringLike(v) && v.toString().isEmpty()))
                return v;
        return {};
    }
    if (name == QLatin1String("formatnumber"))
        return BrFormat::formatNumber(num(0), str(1), ctx.locale());
    if (name == QLatin1String("formatdate") || name == QLatin1String("formatdatetime")) {
        QVariant v = arg(0);
        if (isStringLike(v))
            v = QDateTime::fromString(v.toString(), Qt::ISODate);
        return BrFormat::format(v, str(1), ctx.locale());
    }
    if (name == QLatin1String("today"))
        return QDate::currentDate();
    if (name == QLatin1String("now"))
        return QDateTime::currentDateTime();
    if (name == QLatin1String("year") || name == QLatin1String("month")
        || name == QLatin1String("day")) {
        QVariant v = arg(0);
        QDate d = v.userType() == QMetaType::QDateTime ? v.toDateTime().date() : v.toDate();
        if (isStringLike(v))
            d = QDate::fromString(v.toString().left(10), Qt::ISODate);
        if (!d.isValid())
            return {};
        if (name == QLatin1String("year"))
            return qlonglong(d.year());
        if (name == QLatin1String("month"))
            return qlonglong(d.month());
        return qlonglong(d.day());
    }
    *found = false;
    return {};
}

QVariant evalNode(const BrExpression::Node &n, const BrExpressionContext &ctx)
{
    switch (n.type) {
    case BrExpression::Node::Literal:
        return n.value;
    case BrExpression::Node::Identifier: {
        bool found = false;
        const QVariant v = ctx.variable(n.name, &found);
        if (!found)
            throw ParseError(QStringLiteral("unknown identifier '%1'").arg(n.name).toStdString());
        return v;
    }
    case BrExpression::Node::Unary: {
        const QVariant v = evalNode(*n.children.at(0), ctx);
        if (n.name == QLatin1String("not"))
            return !toBool(v);
        return numberResult(-toNumber(v), isIntegral(v));
    }
    case BrExpression::Node::Binary: {
        const QString &op = n.name;
        if (op == QLatin1String("and"))
            return toBool(evalNode(*n.children.at(0), ctx)) && toBool(evalNode(*n.children.at(1), ctx));
        if (op == QLatin1String("or"))
            return toBool(evalNode(*n.children.at(0), ctx)) || toBool(evalNode(*n.children.at(1), ctx));
        const QVariant a = evalNode(*n.children.at(0), ctx);
        const QVariant b = evalNode(*n.children.at(1), ctx);
        if (op == QLatin1String("&"))
            return BrFormat::format(a, QString(), ctx.locale()) + BrFormat::format(b, QString(), ctx.locale());
        if (op == QLatin1String("+")) {
            if (isStringLike(a) || isStringLike(b)) {
                if (!(BrFormat::isNumeric(a) && BrFormat::isNumeric(b))
                    || (isStringLike(a) && isStringLike(b)))
                    return BrFormat::format(a, QString(), ctx.locale())
                           + BrFormat::format(b, QString(), ctx.locale());
            }
            return numberResult(toNumber(a) + toNumber(b), isIntegral(a) && isIntegral(b));
        }
        if (op == QLatin1String("-"))
            return numberResult(toNumber(a) - toNumber(b), isIntegral(a) && isIntegral(b));
        if (op == QLatin1String("*"))
            return numberResult(toNumber(a) * toNumber(b), isIntegral(a) && isIntegral(b));
        if (op == QLatin1String("/")) {
            const double d = toNumber(b);
            if (d == 0.0)
                return QVariant();
            return toNumber(a) / d;
        }
        if (op == QLatin1String("%")) {
            const double d = toNumber(b);
            if (d == 0.0)
                return QVariant();
            return numberResult(std::fmod(toNumber(a), d), isIntegral(a) && isIntegral(b));
        }
        const int c = compare(a, b);
        if (op == QLatin1String("="))
            return c == 0;
        if (op == QLatin1String("<>"))
            return c != 0;
        if (op == QLatin1String("<"))
            return c < 0;
        if (op == QLatin1String(">"))
            return c > 0;
        if (op == QLatin1String("<="))
            return c <= 0;
        if (op == QLatin1String(">="))
            return c >= 0;
        throw ParseError("unknown operator");
    }
    case BrExpression::Node::Call: {
        if (n.name == QLatin1String("if") || n.name == QLatin1String("iif")) {
            if (n.children.size() < 2)
                throw ParseError("if() needs at least 2 arguments");
            if (toBool(evalNode(*n.children.at(0), ctx)))
                return evalNode(*n.children.at(1), ctx);
            return n.children.size() > 2 ? evalNode(*n.children.at(2), ctx) : QVariant();
        }
        QVariantList args;
        for (const auto &child : n.children)
            args << evalNode(*child, ctx);
        bool found = false;
        // User/context functions first, so applications can override built-ins.
        QVariant v = ctx.function(n.name, args, &found);
        if (found)
            return v;
        v = callBuiltin(n.name, args, ctx, &found);
        if (!found)
            throw ParseError(QStringLiteral("unknown function '%1'").arg(n.name).toStdString());
        return v;
    }
    }
    return {};
}

class ResolverContext : public BrExpressionContext
{
public:
    explicit ResolverContext(const std::function<QVariant(const QString &)> &r) : m_r(r) {}
    QVariant variable(const QString &name, bool *found) const override
    {
        *found = true;
        return m_r ? m_r(name) : QVariant();
    }

private:
    std::function<QVariant(const QString &)> m_r;
};

} // namespace

// ---------------------------------------------------------------------------
// BrExpression
// ---------------------------------------------------------------------------

BrExpression::BrExpression() = default;

BrExpression::BrExpression(const QString &source)
{
    setSource(source);
}

void BrExpression::setSource(const QString &source)
{
    m_source = source;
    m_error.clear();
    m_root.reset();
    if (source.trimmed().isEmpty())
        return;
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        m_root = parser.parse();
    } catch (const std::exception &e) {
        m_error = QString::fromUtf8(e.what());
    }
}

QVariant BrExpression::evaluate(const BrExpressionContext &context) const
{
    if (!m_root)
        return {};
    try {
        return evalNode(*m_root, context);
    } catch (const std::exception &e) {
        qWarning("BrExpression: error evaluating '%s': %s", qPrintable(m_source), e.what());
        return {};
    }
}

QVariant BrExpression::evaluate(const std::function<QVariant(const QString &)> &resolver) const
{
    return evaluate(ResolverContext(resolver));
}

QVariant BrExpression::evaluate(const QString &source, const BrExpressionContext &context,
                                QString *error)
{
    BrExpression expr(source);
    if (!expr.isValid()) {
        if (error)
            *error = expr.errorString();
        return {};
    }
    try {
        if (error)
            error->clear();
        return evalNode(*expr.m_root, context);
    } catch (const std::exception &e) {
        if (error)
            *error = QString::fromUtf8(e.what());
        return {};
    }
}
