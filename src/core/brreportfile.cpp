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

#include "brreportfile.h"

#include "brband.h"
#include "brbarcode.h"
#include "brchart.h"
#include "brcontrol.h"
#include "brgraphics.h"
#include "brpager.h"
#include "brreport.h"
#include "brtextcontrols.h"

#include <QBuffer>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMarginsF>
#include <QMetaEnum>
#include <QRectF>
#include <QSet>
#include <QSizeF>

#include <memory>

namespace {

QHash<QString, BrReportFile::Factory> &registry()
{
    static QHash<QString, BrReportFile::Factory> types = [] {
        QHash<QString, BrReportFile::Factory> t;
        t.insert(QStringLiteral("Band"), [] { return new BrBand(nullptr); });
        t.insert(QStringLiteral("Group"), [] { return new BrGroup(nullptr); });
        t.insert(QStringLiteral("SubDetail"), [] { return new BrSubDetail(nullptr); });
        t.insert(QStringLiteral("Panel"), [] { return new BrPanel(nullptr); });
        t.insert(QStringLiteral("Label"), [] { return new BrLabel(nullptr); });
        t.insert(QStringLiteral("Memo"), [] { return new BrMemo(nullptr); });
        t.insert(QStringLiteral("DBText"), [] { return new BrDBText(nullptr); });
        t.insert(QStringLiteral("DBMemo"), [] { return new BrDBMemo(nullptr); });
        t.insert(QStringLiteral("DBResult"), [] { return new BrDBResult(nullptr); });
        t.insert(QStringLiteral("SystemInfo"), [] { return new BrSystemInfo(nullptr); });
        t.insert(QStringLiteral("Image"), [] { return new BrImage(nullptr); });
        t.insert(QStringLiteral("DBImage"), [] { return new BrDBImage(nullptr); });
        t.insert(QStringLiteral("Draw"), [] { return new BrDraw(nullptr); });
        t.insert(QStringLiteral("Barcode"), [] { return new BrBarcode(nullptr); });
        t.insert(QStringLiteral("DBBarcode"), [] { return new BrDBBarcode(nullptr); });
        t.insert(QStringLiteral("Chart"), [] { return new BrChart(nullptr); });
        return t;
    }();
    return types;
}

bool isLayoutObject(const QObject *o)
{
    return qobject_cast<const BrControl *>(o) || qobject_cast<const BrPager *>(o);
}


QJsonArray numbers(std::initializer_list<double> values)
{
    QJsonArray a;
    for (double v : values)
        a << v;
    return a;
}

QJsonValue colorToJson(const QColor &c)
{
    if (!c.isValid())
        return QJsonValue(QJsonValue::Null);
    return c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb);
}

QColor colorFromJson(const QJsonValue &v)
{
    return v.isString() ? QColor(v.toString()) : QColor();
}

QJsonValue fontToJson(const QFont &f)
{
    QJsonObject o;
    o.insert(QStringLiteral("family"), f.family());
    o.insert(QStringLiteral("size"), f.pointSizeF());
    if (f.bold())
        o.insert(QStringLiteral("bold"), true);
    if (f.italic())
        o.insert(QStringLiteral("italic"), true);
    if (f.underline())
        o.insert(QStringLiteral("underline"), true);
    if (f.strikeOut())
        o.insert(QStringLiteral("strikeOut"), true);
    return o;
}

QFont fontFromJson(const QJsonValue &v)
{
    const QJsonObject o = v.toObject();
    QFont f(o.value(QStringLiteral("family")).toString(QStringLiteral("Arial")));
    f.setPointSizeF(o.value(QStringLiteral("size")).toDouble(10.0));
    f.setBold(o.value(QStringLiteral("bold")).toBool());
    f.setItalic(o.value(QStringLiteral("italic")).toBool());
    f.setUnderline(o.value(QStringLiteral("underline")).toBool());
    f.setStrikeOut(o.value(QStringLiteral("strikeOut")).toBool());
    return f;
}

QString penStyleName(Qt::PenStyle s)
{
    const QMetaEnum e = QMetaEnum::fromType<Qt::PenStyle>();
    return QString::fromLatin1(e.valueToKey(int(s)));
}

Qt::PenStyle penStyleFromName(const QString &name)
{
    bool ok = false;
    const int v = QMetaEnum::fromType<Qt::PenStyle>().keyToValue(name.toLatin1().constData(), &ok);
    return ok ? Qt::PenStyle(v) : Qt::SolidLine;
}

QJsonValue bordersToJson(const BrBorders &b)
{
    QJsonObject o;
    QString sides;
    if (b.left)
        sides += QLatin1Char('l');
    if (b.top)
        sides += QLatin1Char('t');
    if (b.right)
        sides += QLatin1Char('r');
    if (b.bottom)
        sides += QLatin1Char('b');
    o.insert(QStringLiteral("sides"), sides);
    o.insert(QStringLiteral("width"), b.width);
    o.insert(QStringLiteral("color"), colorToJson(b.color));
    if (b.style != Qt::SolidLine)
        o.insert(QStringLiteral("style"), penStyleName(b.style));
    return o;
}

BrBorders bordersFromJson(const QJsonValue &v)
{
    const QJsonObject o = v.toObject();
    BrBorders b;
    const QString sides = o.value(QStringLiteral("sides")).toString();
    b.left = sides.contains(QLatin1Char('l'));
    b.top = sides.contains(QLatin1Char('t'));
    b.right = sides.contains(QLatin1Char('r'));
    b.bottom = sides.contains(QLatin1Char('b'));
    b.width = o.value(QStringLiteral("width")).toDouble(0.2);
    if (o.contains(QStringLiteral("color")))
        b.color = colorFromJson(o.value(QStringLiteral("color")));
    if (o.contains(QStringLiteral("style")))
        b.style = penStyleFromName(o.value(QStringLiteral("style")).toString());
    return b;
}

QJsonValue protectionToJson(const BrProtection &p)
{
    QJsonObject o;
    if (p.move)
        o.insert(QStringLiteral("move"), true);
    if (p.remove)
        o.insert(QStringLiteral("delete"), true);
    if (p.properties)
        o.insert(QStringLiteral("properties"), true);
    if (p.contents)
        o.insert(QStringLiteral("contents"), true);
    if (!p.hiddenProperties.isEmpty())
        o.insert(QStringLiteral("hidden"), QJsonArray::fromStringList(p.hiddenProperties));
    return o;
}

BrProtection protectionFromJson(const QJsonValue &v)
{
    const QJsonObject o = v.toObject();
    BrProtection p;
    p.move = o.value(QStringLiteral("move")).toBool();
    p.remove = o.value(QStringLiteral("delete")).toBool();
    p.properties = o.value(QStringLiteral("properties")).toBool();
    p.contents = o.value(QStringLiteral("contents")).toBool();
    for (const QJsonValue &name : o.value(QStringLiteral("hidden")).toArray())
        p.hiddenProperties << name.toString();
    return p;
}

QJsonValue imageToJson(const QImage &img)
{
    if (img.isNull())
        return QJsonValue(QJsonValue::Null);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
}

QImage imageFromJson(const QJsonValue &v)
{
    QString s = v.toString();
    const int comma = s.indexOf(QLatin1Char(','));
    if (s.startsWith(QLatin1String("data:")) && comma > 0)
        s = s.mid(comma + 1);
    return QImage::fromData(QByteArray::fromBase64(s.toLatin1()));
}

QList<double> doubles(const QJsonValue &v, int count, bool *ok)
{
    QList<double> list;
    const QJsonArray a = v.toArray();
    for (const QJsonValue &x : a)
        list << x.toDouble();
    *ok = list.size() == count;
    return list;
}

QList<QMetaProperty> layoutProperties(const QObject *o)
{
    // Most derived declarations win (a subclass may redeclare a property).
    QList<QMetaProperty> props;
    QSet<QByteArray> seen;
    const QMetaObject *mo = o->metaObject();
    for (int i = mo->propertyCount() - 1; i >= QObject::staticMetaObject.propertyCount(); --i) {
        const QMetaProperty p = mo->property(i);
        if (seen.contains(p.name()))
            continue;
        seen.insert(p.name());
        props.prepend(p);
    }
    return props;
}

bool propertyIsStored(const QMetaProperty &p, const QObject *o)
{
    Q_UNUSED(o)
    return p.isStored();
}

// Human-friendly JSON: "type"/"name" first, "children" last, short values on one line.
bool isScalar(const QJsonValue &v)
{
    return !v.isArray() && !v.isObject();
}

bool isInline(const QJsonValue &v)
{
    if (isScalar(v))
        return true;
    if (v.isArray()) {
        for (const QJsonValue &x : v.toArray())
            if (!isScalar(x))
                return false;
        return v.toArray().size() <= 8;
    }
    const QJsonObject o = v.toObject();
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!isScalar(it.value()))
            return false;
    return o.size() <= 6;
}

QString scalarText(const QJsonValue &v)
{
    QJsonArray wrapper;
    wrapper << v;
    QByteArray s = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(s.mid(1, s.size() - 2));
}

QStringList orderedKeys(const QJsonObject &o)
{
    QStringList keys = o.keys();
    keys.sort();
    QStringList first, last;
    for (const char *k : {"format", "version", "type", "name"})
        if (keys.removeOne(QString::fromLatin1(k)))
            first << QString::fromLatin1(k);
    for (const char *k : {"report", "children"})
        if (keys.removeOne(QString::fromLatin1(k)))
            last << QString::fromLatin1(k);
    return first + keys + last;
}

void writeJson(const QJsonValue &v, int indent, QString &out)
{
    const QString pad(indent, QLatin1Char(' '));
    const QString inner(indent + 4, QLatin1Char(' '));
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.isEmpty() || isInline(v)) {
            QStringList parts;
            for (const QJsonValue &x : a)
                parts << scalarText(x);
            out += QLatin1Char('[') + parts.join(QStringLiteral(", ")) + QLatin1Char(']');
            return;
        }
        out += QStringLiteral("[\n");
        for (int i = 0; i < a.size(); ++i) {
            out += inner;
            writeJson(a.at(i), indent + 4, out);
            out += i + 1 < a.size() ? QStringLiteral(",\n") : QStringLiteral("\n");
        }
        out += pad + QLatin1Char(']');
        return;
    }
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        const QStringList keys = orderedKeys(o);
        if (o.isEmpty() || isInline(v)) {
            QStringList parts;
            for (const QString &k : keys)
                parts << scalarText(k) + QStringLiteral(": ") + scalarText(o.value(k));
            out += QStringLiteral("{ ") + parts.join(QStringLiteral(", ")) + QStringLiteral(" }");
            return;
        }
        out += QStringLiteral("{\n");
        for (int i = 0; i < keys.size(); ++i) {
            out += inner + scalarText(keys.at(i)) + QStringLiteral(": ");
            writeJson(o.value(keys.at(i)), indent + 4, out);
            out += i + 1 < keys.size() ? QStringLiteral(",\n") : QStringLiteral("\n");
        }
        out += pad + QLatin1Char('}');
        return;
    }
    out += scalarText(v);
}

class Writer
{
public:
    QJsonObject write(const QObject *o, bool root)
    {
        QJsonObject j;
        const QString type = root ? QStringLiteral("Report") : BrReportFile::typeName(o);
        if (!root)
            j.insert(QStringLiteral("type"), type);
        if (!o->objectName().isEmpty())
            j.insert(QStringLiteral("name"), o->objectName());

        std::unique_ptr<QObject> def(root ? new BrReport : BrReportFile::create(type));
        const auto *control = qobject_cast<const BrControl *>(o);
        for (const QMetaProperty &p : layoutProperties(o)) {
            if (!propertyIsStored(p, o) || !p.isReadable() || !p.isWritable())
                continue;
            const QString name = QString::fromLatin1(p.name());
            if (control && name == QLatin1String("font") && !control->hasOwnFont())
                continue;
            const QJsonValue v = BrReportFile::propertyToJson(p.read(o), p);
            if (def && def->metaObject()->indexOfProperty(p.name()) >= 0) {
                const QJsonValue d = BrReportFile::propertyToJson(p.read(def.get()), p);
                if (v == d && !(control && name == QLatin1String("font")))
                    continue;
            }
            j.insert(name, v);
        }

        QJsonArray children;
        for (const QObject *child : o->children())
            if (isLayoutObject(child))
                children << write(child, false);
        if (!children.isEmpty())
            j.insert(QStringLiteral("children"), children);
        return j;
    }
};

class Reader
{
public:
    QString error;
    QStringList warnings;

    void applyProperties(QObject *o, const QJsonObject &j, const QString &where)
    {
        const QMetaObject *mo = o->metaObject();
        for (auto it = j.constBegin(); it != j.constEnd(); ++it) {
            const QString key = it.key();
            if (key == QLatin1String("type") || key == QLatin1String("name")
                || key == QLatin1String("children"))
                continue;
            const int index = mo->indexOfProperty(key.toLatin1().constData());
            if (index < 0) {
                warnings << QStringLiteral("%1: unknown property '%2'").arg(where, key);
                continue;
            }
            const QMetaProperty p = mo->property(index);
            bool ok = false;
            const QVariant v = BrReportFile::propertyFromJson(it.value(), p, &ok);
            if (!ok || !p.write(o, v))
                warnings << QStringLiteral("%1: invalid value for '%2'").arg(where, key);
        }
    }

    QObject *readObject(QObject *parent, const QJsonObject &j, const QString &where)
    {
        QJsonArray one;
        one << j;
        const int before = int(parent->children().size());
        if (!readChildren(parent, one, where)) {
            if (parent->children().size() > before)
                delete parent->children().last(); // remove the half-built object
            return nullptr;
        }
        return parent->children().size() > before ? parent->children().last() : nullptr;
    }

    bool readChildren(QObject *parent, const QJsonValue &value, const QString &where)
    {
        const QJsonArray children = value.toArray();
        for (const QJsonValue &c : children) {
            const QJsonObject j = c.toObject();
            const QString type = j.value(QStringLiteral("type")).toString();
            const QString name = j.value(QStringLiteral("name")).toString();
            const QString here = where + QLatin1Char('/') + (name.isEmpty() ? type : name);
            QObject *o = BrReportFile::create(type);
            if (!o) {
                error = QStringLiteral("%1: unknown object type '%2'").arg(here, type);
                return false;
            }
            const bool isBand = qobject_cast<BrBand *>(o);
            const bool isPager = qobject_cast<BrPager *>(o);
            const bool parentIsPager = qobject_cast<BrPager *>(parent);
            const bool parentIsContainer = qobject_cast<BrContainer *>(parent) && !qobject_cast<BrPager *>(parent);
            if (((isBand || isPager) && !parentIsPager) || (!isBand && !isPager && !parentIsContainer)) {
                delete o;
                error = QStringLiteral("%1: a '%2' cannot be placed here").arg(here, type);
                return false;
            }
            o->setParent(parent);
            o->setObjectName(name);
            applyProperties(o, j, here);
            if (!readChildren(o, j.value(QStringLiteral("children")), here))
                return false;
        }
        return true;
    }
};

} // namespace

void BrReportFile::registerType(const QString &typeName, const Factory &factory)
{
    registry().insert(typeName, factory);
}

QStringList BrReportFile::typeNames()
{
    QStringList names = registry().keys();
    names.sort();
    return names;
}

QString BrReportFile::typeName(const QObject *object)
{
    QString name = QString::fromLatin1(object->metaObject()->className());
    if (name.startsWith(QLatin1String("Br")) && name.size() > 2 && name.at(2).isUpper())
        name = name.mid(2);
    return name;
}

QObject *BrReportFile::create(const QString &typeName)
{
    const Factory f = registry().value(typeName);
    return f ? f() : nullptr;
}

QJsonValue BrReportFile::propertyToJson(const QVariant &value, const QMetaProperty &property)
{
    if (property.isEnumType()) {
        const QMetaEnum e = property.enumerator();
        const char *key = e.valueToKey(value.toInt());
        return key ? QJsonValue(QString::fromLatin1(key)) : QJsonValue(value.toInt());
    }
    const int type = property.userType();
    if (type == qMetaTypeId<BrBorders>())
        return bordersToJson(value.value<BrBorders>());
    if (type == qMetaTypeId<BrProtection>())
        return protectionToJson(value.value<BrProtection>());
    switch (type) {
    case QMetaType::QColor:
        return colorToJson(value.value<QColor>());
    case QMetaType::QFont:
        return fontToJson(value.value<QFont>());
    case QMetaType::QRectF: {
        const QRectF r = value.toRectF();
        return numbers({r.x(), r.y(), r.width(), r.height()});
    }
    case QMetaType::QSizeF: {
        const QSizeF s = value.toSizeF();
        return numbers({s.width(), s.height()});
    }
    case QMetaType::QImage:
        return imageToJson(value.value<QImage>());
    case QMetaType::QStringList:
        return QJsonArray::fromStringList(value.toStringList());
    default:
        break;
    }
    if (type == qMetaTypeId<QMarginsF>()) {
        const QMarginsF m = value.value<QMarginsF>();
        return numbers({m.left(), m.top(), m.right(), m.bottom()});
    }
    return QJsonValue::fromVariant(value);
}

QVariant BrReportFile::propertyFromJson(const QJsonValue &value, const QMetaProperty &property, bool *ok)
{
    bool dummy = false;
    bool &good = ok ? *ok : dummy;
    good = true;
    if (property.isEnumType()) {
        if (value.isString()) {
            const int v = property.enumerator().keyToValue(value.toString().toLatin1().constData(), &good);
            return v;
        }
        good = value.isDouble();
        return value.toInt();
    }
    const int type = property.userType();
    if (type == qMetaTypeId<BrBorders>()) {
        good = value.isObject();
        return QVariant::fromValue(bordersFromJson(value));
    }
    if (type == qMetaTypeId<BrProtection>()) {
        good = value.isObject();
        return QVariant::fromValue(protectionFromJson(value));
    }
    if (type == qMetaTypeId<QMarginsF>()) {
        const QList<double> m = doubles(value, 4, &good);
        return good ? QVariant::fromValue(QMarginsF(m[0], m[1], m[2], m[3])) : QVariant();
    }
    switch (type) {
    case QMetaType::QColor:
        good = value.isString() || value.isNull();
        return QVariant::fromValue(colorFromJson(value));
    case QMetaType::QFont:
        good = value.isObject();
        return QVariant::fromValue(fontFromJson(value));
    case QMetaType::QRectF: {
        const QList<double> r = doubles(value, 4, &good);
        return good ? QVariant(QRectF(r[0], r[1], r[2], r[3])) : QVariant();
    }
    case QMetaType::QSizeF: {
        const QList<double> s = doubles(value, 2, &good);
        return good ? QVariant(QSizeF(s[0], s[1])) : QVariant();
    }
    case QMetaType::QImage:
        return QVariant::fromValue(value.isNull() ? QImage() : imageFromJson(value));
    case QMetaType::QStringList: {
        QStringList list;
        for (const QJsonValue &v : value.toArray())
            list << v.toString();
        good = value.isArray();
        return list;
    }
    case QMetaType::Bool:
        good = value.isBool();
        return value.toBool();
    case QMetaType::Int:
        good = value.isDouble();
        return value.toInt();
    case QMetaType::Double:
        good = value.isDouble();
        return value.toDouble();
    case QMetaType::QString:
        good = value.isString() || value.isNull();
        return value.toString();
    default:
        return value.toVariant();
    }
}

QByteArray BrReportFile::toJson(const BrReport *report, bool compact)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("BrReport"));
    root.insert(QStringLiteral("version"), FormatVersion);
    root.insert(QStringLiteral("report"), Writer().write(report, true));
    if (compact)
        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    QString out;
    writeJson(root, 0, out);
    out += QLatin1Char('\n');
    return out.toUtf8();
}

bool BrReportFile::fromJson(BrReport *report, const QByteArray &json, QString *error,
                            QStringList *warnings)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    if (doc.isNull())
        return fail(QStringLiteral("Invalid JSON at offset %1: %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()));
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String("BrReport"))
        return fail(QStringLiteral("Not a BrReport file."));
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version < 1 || version > FormatVersion)
        return fail(QStringLiteral("Unsupported file version %1.").arg(version));
    const QJsonObject r = root.value(QStringLiteral("report")).toObject();

    // Reset the report properties to their defaults, then apply the file.
    report->clearLayout();
    {
        BrReport defaults;
        for (const QMetaProperty &p : layoutProperties(report))
            if (p.isWritable() && propertyIsStored(p, report))
                p.write(report, p.read(&defaults));
    }
    Reader reader;
    if (r.contains(QStringLiteral("name")))
        report->setObjectName(r.value(QStringLiteral("name")).toString());
    reader.applyProperties(report, r, QStringLiteral("Report"));
    if (!reader.readChildren(report, r.value(QStringLiteral("children")), QStringLiteral("Report"))) {
        report->clearLayout();
        return fail(reader.error);
    }
    for (const QString &w : reader.warnings)
        qWarning("BrReportFile: %s", qPrintable(w));
    if (warnings)
        *warnings = reader.warnings;
    if (error)
        error->clear();
    return true;
}

QJsonObject BrReportFile::objectToJson(const QObject *object)
{
    return Writer().write(object, qobject_cast<const BrReport *>(object) != nullptr);
}

QObject *BrReportFile::objectFromJson(const QJsonObject &json, QObject *parent, QString *error)
{
    Reader reader;
    QObject *o = reader.readObject(parent, json, BrReportFile::typeName(parent));
    if (!o && error)
        *error = reader.error;
    for (const QString &w : reader.warnings)
        qWarning("BrReportFile: %s", qPrintable(w));
    return o;
}

bool BrReportFile::canContain(const QObject *parent, const QString &typeName)
{
    std::unique_ptr<QObject> probe(create(typeName));
    if (!probe || !parent)
        return false;
    const bool isBand = qobject_cast<BrBand *>(probe.get());
    const bool isPager = qobject_cast<BrPager *>(probe.get());
    if (isBand || isPager)
        return qobject_cast<const BrPager *>(parent) != nullptr;
    return qobject_cast<const BrContainer *>(parent) != nullptr;
}
