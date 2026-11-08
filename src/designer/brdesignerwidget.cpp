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

#include "brdesignerwidget.h"

#include "brband.h"
#include "brcontrol.h"
#include "brdatasource.h"
#include "brdesignerlabels_p.h"
#include "brdesignscene_p.h"
#include "brdesignview_p.h"
#include "brpager.h"
#include "brparametersdialog.h"
#include "brparameterseditor_p.h"
#include "brpreviewdialog.h"
#include "brpropertyeditor_p.h"
#include "brreportwizard_p.h"
#include "brreport.h"
#include "brreportfile.h"
#include "brtextcontrols.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDrag>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaEnum>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QLineEdit>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>

#ifdef BRREPORT_HAS_SQL
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#endif

#include <algorithm>

namespace {

using TreeItems = const QList<QTreeWidgetItem *> &;

const char kSettingsGroup[] = "designer";
const int kMaxRecentFiles = 8;

bool isLayoutObject(const QObject *o)
{
    return qobject_cast<const BrControl *>(o) || qobject_cast<const BrPager *>(o);
}

QList<QObject *> layoutChildren(const QObject *o)
{
    QList<QObject *> list;
    for (QObject *c : o->children())
        if (isLayoutObject(c))
            list << c;
    return list;
}

QList<int> pathOf(const QObject *object, const QObject *root)
{
    QList<int> path;
    for (const QObject *o = object; o && o != root; o = o->parent()) {
        if (!o->parent())
            return {};
        path.prepend(int(layoutChildren(o->parent()).indexOf(const_cast<QObject *>(o))));
    }
    return path;
}

QObject *objectAt(const QList<int> &path, QObject *root)
{
    QObject *o = root;
    for (int i : path) {
        const QList<QObject *> kids = layoutChildren(o);
        if (i < 0 || i >= kids.size())
            return nullptr;
        o = kids.at(i);
    }
    return o;
}

/*! \a parent's children with \a moved taken out and inserted at \a index of the current list. */
QList<QObject *> childrenWithMoved(const QObject *parent, const QList<QObject *> &moved, int index)
{
    const QList<QObject *> siblings = layoutChildren(parent);
    QList<QObject *> result;
    for (int i = 0; i <= siblings.size(); ++i) {
        if (i == index)
            result << moved;
        if (i < siblings.size() && !moved.contains(siblings.at(i)))
            result << siblings.at(i);
    }
    return result;
}

void setChildOrder(QObject *parent, const QList<QObject *> &children)
{
    for (QObject *c : children) { // re-append in the new order
        c->setParent(nullptr);
        c->setParent(parent);
    }
}

/*! The part of the report where an object prints; the order only matters within it. */
int printSection(const QObject *o)
{
    if (auto *b = qobject_cast<const BrBand *>(o))
        return int(b->bandType());
    if (auto *s = qobject_cast<const BrSubDetail *>(o))
        return int(s->positioning());
    return int(Br::BandType::Detail);
}

/*! Whether going from \a before to \a after changes what is printed: bands print by type. */
bool printsDifferently(QList<QObject *> before, QList<QObject *> after)
{
    const auto bySection = [](const QObject *a, const QObject *b) { return printSection(a) < printSection(b); };
    std::stable_sort(before.begin(), before.end(), bySection);
    std::stable_sort(after.begin(), after.end(), bySection);
    return before != after;
}

QObject *objectOf(const QModelIndex &index)
{
    return static_cast<QObject *>(index.data(Qt::UserRole).value<void *>());
}

QString describe(const QObject *o)
{
    QString type = BrDesignerLabels::typeLabel(qobject_cast<const BrReport *>(o) ? QStringLiteral("Report")
                                                                                 : BrReportFile::typeName(o));
    QString detail;
    if (auto *b = qobject_cast<const BrBand *>(o)) {
        const QMetaEnum e = QMetaEnum::fromType<Br::BandType>();
        detail = BrDesignerLabels::label(QString::fromLatin1(e.valueToKey(int(b->bandType()))));
    } else if (auto *l = qobject_cast<const BrLabel *>(o)) {
        detail = l->text().left(30);
    } else if (auto *t = qobject_cast<const BrDBText *>(o)) {
        detail = t->dataField().isEmpty() ? t->dataFormula().left(30) : t->dataField();
    } else if (auto *g = qobject_cast<const BrGroup *>(o)) {
        detail = g->dataFields().isEmpty() ? g->dataFormula() : g->dataFields();
    } else if (auto *s = qobject_cast<const BrSubDetail *>(o)) {
        detail = s->dataSourceName();
    }
    QString text = type;
    if (!o->objectName().isEmpty())
        text += QStringLiteral(" \"%1\"").arg(o->objectName());
    if (!detail.isEmpty())
        text += QStringLiteral(": ") + detail.simplified();
    return text;
}

/*! A copy made by an end user belongs to that user, who must be able to change it. */
QJsonObject withoutProtection(QJsonObject json)
{
    json.remove(QStringLiteral("protection"));
    if (json.contains(QStringLiteral("children"))) {
        QJsonArray children = json.value(QStringLiteral("children")).toArray();
        for (int i = 0; i < children.size(); ++i)
            children[i] = withoutProtection(children.at(i).toObject());
        json.insert(QStringLiteral("children"), children);
    }
    return json;
}

QJsonObject geometryTemplate(const char *type, double w, double h, QJsonObject extra = {})
{
    extra.insert(QStringLiteral("type"), QString::fromLatin1(type));
    extra.insert(QStringLiteral("geometry"), QJsonArray{0, 0, w, h});
    return extra;
}

/*! Tree of fields that can be dragged into the report. */
class DataTree : public QTreeWidget
{
public:
    explicit DataTree(QWidget *parent) : QTreeWidget(parent)
    {
        setHeaderHidden(true);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    QStringList mimeTypes() const override { return {QStringLiteral(BRREPORT_OBJECT_MIME)}; }
    QMimeData *mimeData(TreeItems items) const override
    {
        if (items.isEmpty() || items.first()->data(0, Qt::UserRole).isNull())
            return nullptr;
        auto *mime = new QMimeData;
        mime->setData(QStringLiteral(BRREPORT_OBJECT_MIME), items.first()->data(0, Qt::UserRole).toByteArray());
        return mime;
    }
};

/*!
 * Report structure. The selected siblings can be dragged to another place among
 * their siblings; the tree is rebuilt from the report, so the drop only asks for it.
 */
class StructureTree : public QTreeWidget
{
public:
    using MoveHandler = std::function<bool(const QList<QObject *> &, QObject *, int)>;

    explicit StructureTree(QWidget *parent) : QTreeWidget(parent)
    {
        setHeaderHidden(true);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(false); // it cannot show a place after a whole expanded branch
        setExpandsOnDoubleClick(false);
        setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    }

    /*! Restores the position after a rebuild, which resets it. */
    void setScrollPosition(int position)
    {
        executeDelayedItemsLayout();
        updateGeometries();
        verticalScrollBar()->setValue(position);
    }

    MoveHandler canDrop; //!< Whether a drop there would change something.
    MoveHandler drop;    //!< Called after the drag, when the tree can be rebuilt.

protected:
    // The default would serialize the items, and their objects cannot be: the selection says what moves.
    QStringList mimeTypes() const override { return {structureMime()}; }
    QMimeData *mimeData(TreeItems) const override
    {
        auto *mime = new QMimeData;
        mime->setData(structureMime(), QByteArray());
        return mime;
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        QTreeWidget::dragEnterEvent(event);
        event->setAccepted(isOwnDrag(event));
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        QTreeWidget::dragMoveEvent(event); // scrolls near the edges
        const Target target = targetAt(event);
        setDropLine(target.valid ? target.line : QRect());
        event->setAccepted(target.valid);
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        QTreeWidget::dragLeaveEvent(event);
        setDropLine(QRect());
    }

    void dropEvent(QDropEvent *event) override
    {
        const Target target = targetAt(event);
        setDropLine(QRect());
        stopAutoScroll();
        setState(NoState);
        if (!target.valid) {
            event->ignore();
            return;
        }
        event->setDropAction(Qt::CopyAction); // a move would make the view remove the rows
        event->accept();
        QMetaObject::invokeMethod(
            this, [this, target]() { drop(target.objects, target.parent, target.index); }, Qt::QueuedConnection);
    }

    void paintEvent(QPaintEvent *event) override
    {
        QTreeWidget::paintEvent(event);
        if (!m_dropLine.isNull()) {
            QPainter painter(viewport());
            painter.fillRect(m_dropLine, palette().highlight());
        }
    }

private:
    struct Target
    {
        bool valid = false;
        QList<QObject *> objects;
        QObject *parent = nullptr;
        int index = -1;
        QRect line;
    };

    bool isOwnDrag(const QDropEvent *event) const { return event->mimeData()->hasFormat(structureMime()); }
    static QString structureMime() { return QStringLiteral("application/x-brreport-structure"); }

    Target targetAt(const QDropEvent *event) const
    {
        Target target;
        const QList<QTreeWidgetItem *> items = selectedItems();
        if (!isOwnDrag(event) || items.isEmpty() || !items.first()->parent())
            return target;
        QTreeWidgetItem *parentItem = items.first()->parent();
        for (QTreeWidgetItem *item : items)
            if (item->parent() != parentItem)
                return target;

        // A row inside a sibling counts as that sibling, with everything it contains.
        const QPoint pos = event->position().toPoint();
        QTreeWidgetItem *anchor = itemAt(pos);
        while (anchor && anchor->parent() != parentItem)
            anchor = anchor->parent();
        if (!anchor)
            return target;
        QRect branch = visualItemRect(anchor);
        for (QTreeWidgetItem *last = anchor; last->isExpanded() && last->childCount() > 0;) {
            last = last->child(last->childCount() - 1);
            branch |= visualItemRect(last);
        }
        const bool after = pos.y() > branch.center().y();

        QList<QTreeWidgetItem *> ordered = items;
        std::sort(ordered.begin(), ordered.end(), [parentItem](QTreeWidgetItem *a, QTreeWidgetItem *b) {
            return parentItem->indexOfChild(a) < parentItem->indexOfChild(b);
        });
        for (QTreeWidgetItem *item : std::as_const(ordered))
            target.objects << static_cast<QObject *>(item->data(0, Qt::UserRole).value<void *>());
        target.parent = static_cast<QObject *>(parentItem->data(0, Qt::UserRole).value<void *>());
        target.index = parentItem->indexOfChild(anchor) + (after ? 1 : 0);
        target.line = QRect(branch.left(), (after ? branch.bottom() : branch.top()) - 1,
                            viewport()->width() - branch.left(), 2);
        target.valid = canDrop(target.objects, target.parent, target.index);
        return target;
    }

    void setDropLine(const QRect &line)
    {
        if (line != m_dropLine) {
            m_dropLine = line;
            viewport()->update();
        }
    }

    QRect m_dropLine;
};

/*! Edits only the name of an object, which is a part of the text of its row. */
class StructureDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    std::function<void(QObject *, const QString &)> renamed;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *edit = qobject_cast<QLineEdit *>(editor))
            edit->setText(objectOf(index)->objectName());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override
    {
        if (auto *edit = qobject_cast<QLineEdit *>(editor))
            renamed(objectOf(index), edit->text().trimmed());
    }
};

} // namespace

// ---------------------------------------------------------------------------

class BrSnapshotCommand : public QUndoCommand
{
public:
    BrSnapshotCommand(BrDesignerWidget *w, const QByteArray &before, const QByteArray &after, const QString &text)
        : QUndoCommand(text), m_w(w), m_before(before), m_after(after)
    {
    }
    void undo() override { m_w->restoreSnapshot(m_before); }
    void redo() override
    {
        if (m_first) {
            m_first = false;
            return;
        }
        m_w->restoreSnapshot(m_after);
    }

private:
    BrDesignerWidget *m_w;
    QByteArray m_before, m_after;
    bool m_first = true;
};

struct BrDesignerWidget::Private
{
    BrReport report;
    BrDesignScene *scene = nullptr;
    BrDesignView *view = nullptr;
    StructureTree *tree = nullptr;
    DataTree *data = nullptr;
    BrPropertyEditor *properties = nullptr;
    QUndoStack undo;
    QByteArray current; // JSON of the last committed state
    QHash<QString, BrDataSource *> dataSources;
    QVariantMap lastParameters; // values typed in the last preview
    BrDesignerWidget::SaveHandler saveHandler;
    QActionGroup *tools = nullptr;
    QAction *pointerTool = nullptr;
    QAction *snapAction = nullptr;
    QList<QAction *> fileOnlyActions;
    QList<QAction *> developerActions;
    QMenu *recentMenu = nullptr;
    QStringList recentFiles;
    double zoom = 1.0;
    bool zoomRestored = false;
    bool syncing = false;
    bool developerMode = false;
};

BrDesignerWidget::BrDesignerWidget(QWidget *parent) : QMainWindow(parent), d(new Private)
{
    setWindowTitle(tr("Report Designer"));
    resize(1280, 860);
    setDockNestingEnabled(true);

    // --- Central view
    d->scene = new BrDesignScene(this);
    d->scene->setReport(&d->report);
    d->view = new BrDesignView(d->scene, this);
    d->view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    d->view->setDragMode(QGraphicsView::RubberBandDrag);
    d->view->setAcceptDrops(true);
    d->view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    d->view->viewport()->installEventFilter(this);
    d->view->installEventFilter(this);
    setCentralWidget(d->view);

    // --- Docks
    d->tree = new StructureTree(this);
    d->tree->canDrop = [this](const QList<QObject *> &objects, QObject *parent, int index) {
        return printsDifferently(layoutChildren(parent), childrenWithMoved(parent, objects, index));
    };
    d->tree->drop = [this](const QList<QObject *> &objects, QObject *parent, int index) {
        moveInTree(objects, parent, index);
        return true;
    };
    auto *treeDelegate = new StructureDelegate(d->tree);
    treeDelegate->renamed = [this](QObject *object, const QString &name) {
        // The editor is still closing: the tree cannot be rebuilt yet.
        QMetaObject::invokeMethod(this, [this, object, name]() { renameObject(object, name); }, Qt::QueuedConnection);
    };
    d->tree->setItemDelegate(treeDelegate);
    auto *treeDock = new QDockWidget(tr("Structure"), this);
    treeDock->setObjectName(QStringLiteral("structureDock"));
    treeDock->setWidget(d->tree);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);

    d->data = new DataTree(this);
    auto *dataDock = new QDockWidget(tr("Data"), this);
    dataDock->setObjectName(QStringLiteral("dataDock"));
    dataDock->setWidget(d->data);
    addDockWidget(Qt::LeftDockWidgetArea, dataDock);

    d->properties = new BrPropertyEditor(this);
    auto *propDock = new QDockWidget(tr("Properties"), this);
    propDock->setObjectName(QStringLiteral("propertiesDock"));
    propDock->setWidget(d->properties);
    propDock->setMinimumWidth(320);
    addDockWidget(Qt::RightDockWidgetArea, propDock);

    QStyle *st = style();
    auto icon = [st](const char *theme, QStyle::StandardPixmap fallback) {
        return QIcon::fromTheme(QString::fromLatin1(theme), st->standardIcon(fallback));
    };

    // --- File
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *newAct = m_fileMenu->addAction(icon("document-new", QStyle::SP_FileIcon), tr("&New"), this,
                                            &BrDesignerWidget::newReport);
    newAct->setShortcut(QKeySequence::New);
    m_fileMenu->addAction(tr("New from &wizard..."), this, &BrDesignerWidget::newReportWizard);
    QAction *openAct = m_fileMenu->addAction(icon("document-open", QStyle::SP_DialogOpenButton), tr("&Open..."),
                                             this, [this]() { openFile(); });
    openAct->setShortcut(QKeySequence::Open);
    QAction *saveAct = m_fileMenu->addAction(icon("document-save", QStyle::SP_DialogSaveButton), tr("&Save"),
                                             this, &BrDesignerWidget::save);
    saveAct->setShortcut(QKeySequence::Save);
    QAction *saveAsAct = m_fileMenu->addAction(tr("Save &as..."), this, &BrDesignerWidget::saveAs);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    d->recentMenu = m_fileMenu->addMenu(tr("Recent &files"));
    d->recentMenu->setObjectName(QStringLiteral("recentFilesMenu"));
    d->recentMenu->setToolTipsVisible(true);
    updateRecentMenu();
    d->fileOnlyActions = {openAct, saveAsAct, d->recentMenu->menuAction()};
    m_fileMenu->addSeparator();
    QAction *previewAct = m_fileMenu->addAction(icon("document-print-preview", QStyle::SP_FileDialogContentsView),
                                                tr("&Preview"), this, &BrDesignerWidget::preview);
    previewAct->setShortcut(QKeySequence(Qt::Key_F5));

    // --- Edit
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction *undoAct = d->undo.createUndoAction(this, tr("&Undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setIcon(icon("edit-undo", QStyle::SP_ArrowBack));
    QAction *redoAct = d->undo.createRedoAction(this, tr("&Redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setIcon(icon("edit-redo", QStyle::SP_ArrowForward));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    QAction *cutAct = editMenu->addAction(icon("edit-cut", QStyle::SP_DialogDiscardButton), tr("Cu&t"), this, [this]() {
        QList<QObject *> objects = selection();
        objects.erase(std::remove_if(objects.begin(), objects.end(),
                                     [this](const QObject *o) { return !canDelete(o); }),
                      objects.end());
        copyObjects(objects);
        deleteSelection();
    });
    cutAct->setShortcut(QKeySequence::Cut);
    QAction *copyAct = editMenu->addAction(icon("edit-copy", QStyle::SP_FileIcon), tr("&Copy"), this,
                                           &BrDesignerWidget::copySelection);
    copyAct->setShortcut(QKeySequence::Copy);
    QAction *pasteAct = editMenu->addAction(icon("edit-paste", QStyle::SP_DialogApplyButton), tr("&Paste"), this,
                                            &BrDesignerWidget::paste);
    pasteAct->setShortcut(QKeySequence::Paste);
    QAction *dupAct = editMenu->addAction(tr("D&uplicate"), this, &BrDesignerWidget::duplicateSelection);
    dupAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    QAction *delAct = editMenu->addAction(icon("edit-delete", QStyle::SP_TrashIcon), tr("&Delete"), this,
                                          &BrDesignerWidget::deleteSelection);
    delAct->setShortcut(QKeySequence::Delete);
    editMenu->addSeparator();
    QAction *upAct = editMenu->addAction(tr("Move &up / backward"), this, [this]() { moveInOrder(-1); });
    upAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    QAction *downAct = editMenu->addAction(tr("Move do&wn / forward"), this, [this]() { moveInOrder(1); });
    downAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));

    // --- Insert (toolbox)
    QMenu *insertMenu = menuBar()->addMenu(tr("&Insert"));
    QToolBar *toolbox = new QToolBar(tr("Toolbox"), this);
    toolbox->setObjectName(QStringLiteral("toolbox"));
    toolbox->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d->tools = new QActionGroup(this);
    d->pointerTool = toolbox->addAction(tr("Select"));
    d->pointerTool->setCheckable(true);
    d->pointerTool->setChecked(true);
    d->tools->addAction(d->pointerTool);
    connect(d->pointerTool, &QAction::triggered, this, [this]() { d->scene->setInsertTemplate(QJsonObject()); });
    struct Tool { QString text; QJsonObject json; };
    const QList<Tool> tools = {
        {tr("Text"), geometryTemplate("Label", 30, 6, {{QStringLiteral("text"), tr("Text")}})},
        {tr("Field"), geometryTemplate("DBText", 30, 6)},
        {tr("Memo"), geometryTemplate("Memo", 60, 12, {{QStringLiteral("text"), tr("Memo")}})},
        {tr("Total"), geometryTemplate("DBResult", 30, 6)},
        {tr("Page number"), geometryTemplate("SystemInfo", 40, 6, {{QStringLiteral("info"), QStringLiteral("PagePreview")},
                                                               {QStringLiteral("format"), tr("Page %1 of %2")}})},
        {tr("Image"), geometryTemplate("Image", 20, 20)},
        {tr("Rectangle"), geometryTemplate("Draw", 30, 15)},
        {tr("Line"), geometryTemplate("Draw", 40, 2, {{QStringLiteral("kind"), QStringLiteral("HorizontalLine")}})},
        {tr("Barcode"), geometryTemplate("Barcode", 40, 12, {{QStringLiteral("text"), QStringLiteral("123456789")}})},
        {tr("QR Code"), geometryTemplate("Barcode", 20, 20, {{QStringLiteral("barcodeType"), QStringLiteral("QrCode")},
                                                             {QStringLiteral("text"), QStringLiteral("https://")}})},
        {tr("Chart"), geometryTemplate("Chart", 80, 50, {{QStringLiteral("labels"), QJsonArray{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")}},
                                                         {QStringLiteral("seriesData"),
                                                          QJsonArray{QJsonObject{{QStringLiteral("name"), tr("Series")},
                                                                                 {QStringLiteral("values"), QJsonArray{3, 5, 4}}}}}})},
        {tr("Panel"), geometryTemplate("Panel", 40, 20, {{QStringLiteral("borders"),
                                                          QJsonObject{{QStringLiteral("sides"), QStringLiteral("ltrb")},
                                                                      {QStringLiteral("width"), 0.2}}}})},
    };
    for (const Tool &tool : tools) {
        QAction *a = toolbox->addAction(tool.text);
        a->setCheckable(true);
        a->setToolTip(tr("Insert %1: click inside a band").arg(tool.text.toLower()));
        d->tools->addAction(a);
        insertMenu->addAction(a);
        const QJsonObject json = tool.json;
        connect(a, &QAction::triggered, this, [this, json]() { d->scene->setInsertTemplate(json); });
    }
    addToolBar(Qt::LeftToolBarArea, toolbox);

    // --- Report
    QMenu *reportMenu = menuBar()->addMenu(tr("&Report"));
    QMenu *bandMenu = reportMenu->addMenu(tr("Add &band"));
    const QMetaEnum bandTypes = QMetaEnum::fromType<Br::BandType>();
    for (int i = 0; i < bandTypes.keyCount(); ++i) {
        const int value = bandTypes.value(i);
        bandMenu->addAction(BrPropertyEditor::labelFor(QString::fromLatin1(bandTypes.key(i))), this,
                            [this, value]() { addBand(value); });
    }
    reportMenu->addAction(tr("Add &group..."), this, &BrDesignerWidget::addGroup);
    reportMenu->addAction(tr("Add &sub-report..."), this, &BrDesignerWidget::addSubReport);
    reportMenu->addAction(tr("Add &cover page"), this, [this]() { addTemplatePage(Br::BandType::Title); });
    reportMenu->addAction(tr("Add c&losing page"), this, [this]() { addTemplatePage(Br::BandType::Summary); });
    reportMenu->addSeparator();
    reportMenu->addAction(tr("&Page setup and report properties"), this, [this]() {
        d->scene->clearSelection();
        d->properties->setObjects({&d->report});
    });
    QAction *protectSeparator = reportMenu->addSeparator();
    QAction *protectAct = reportMenu->addAction(tr("Pro&tect selection"), this,
                                                [this]() { setSelectionProtected(true); });
    protectAct->setToolTip(tr("Lock the selected objects and everything inside them for end users"));
    QAction *unprotectAct = reportMenu->addAction(tr("Remove prot&ection"), this,
                                                  [this]() { setSelectionProtected(false); });
    d->developerActions = {protectSeparator, protectAct, unprotectAct};
    for (QAction *a : std::as_const(d->developerActions))
        a->setVisible(false);

    // --- Format (alignment)
    QMenu *formatMenu = menuBar()->addMenu(tr("F&ormat"));
    QToolBar *alignBar = new QToolBar(tr("Alignment"), this);
    alignBar->setObjectName(QStringLiteral("alignBar"));
    const QStringList alignNames = {tr("Align left"), tr("Align right"), tr("Align top"), tr("Align bottom"),
                                    tr("Center horizontally"), tr("Same width"), tr("Same height")};
    for (int i = 0; i < alignNames.size(); ++i) {
        QAction *a = formatMenu->addAction(alignNames.at(i), this, [this, i]() { align(i); });
        alignBar->addAction(a);
    }
    formatMenu->addSeparator();
    alignBar->addSeparator();
    alignBar->addAction(formatMenu->addAction(tr("Distribute horizontally"), this,
                                              [this]() { distribute(Qt::Horizontal); }));
    alignBar->addAction(formatMenu->addAction(tr("Distribute vertically"), this,
                                              [this]() { distribute(Qt::Vertical); }));

    // --- Data
    m_dataMenu = menuBar()->addMenu(tr("&Data"));
#ifdef BRREPORT_HAS_SQL
    m_dataMenu->addAction(tr("Add SQL &query..."), this, [this]() { editQuery(QString()); });
    m_dataMenu->addAction(tr("&Edit selected query..."), this, [this]() {
        if (QTreeWidgetItem *item = d->data->currentItem()) {
            while (item->parent())
                item = item->parent();
            editQuery(item->data(0, Qt::UserRole + 1).toString());
        }
    });
    m_dataMenu->addAction(tr("&Remove selected query"), this, [this]() {
        if (QTreeWidgetItem *item = d->data->currentItem()) {
            while (item->parent())
                item = item->parent();
            removeQuery(item->data(0, Qt::UserRole + 1).toString());
        }
    });
    m_dataMenu->addSeparator();
#endif
    m_dataMenu->addAction(tr("Report &parameters..."), this, &BrDesignerWidget::editParameters);
    m_dataMenu->addAction(tr("Re&fresh fields"), this, &BrDesignerWidget::rebuildDataPanel);

    // --- View
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *zoomIn = viewMenu->addAction(tr("Zoom &in"), this, [this]() { setZoom(d->zoom * 1.25); });
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    QAction *zoomOut = viewMenu->addAction(tr("Zoom &out"), this, [this]() { setZoom(d->zoom / 1.25); });
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(tr("&Actual size"), this, [this]() { setZoom(1.0); });
    QAction *fitAct = viewMenu->addAction(tr("&Fit width"), this, [this]() {
        const double base = logicalDpiX() / 25.4;
        const double width = d->scene->sceneRect().width();
        if (width > 0)
            setZoom((d->view->viewport()->width() - 4) / (width * base));
    });
    fitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    viewMenu->addSeparator();
    d->snapAction = viewMenu->addAction(tr("&Snap to grid (1 mm)"));
    d->snapAction->setCheckable(true);
    d->snapAction->setChecked(true);
    connect(d->snapAction, &QAction::toggled, this, [this](bool on) { d->scene->setSnapToGrid(on); });
    QAction *snapObjects = viewMenu->addAction(tr("Snap to &objects and guides"));
    snapObjects->setCheckable(true);
    snapObjects->setChecked(true);
    connect(snapObjects, &QAction::toggled, this, [this](bool on) { d->scene->setSnapToObjects(on); });
    QAction *rulers = viewMenu->addAction(tr("&Rulers"));
    rulers->setCheckable(true);
    rulers->setChecked(true);
    connect(rulers, &QAction::toggled, this, [this](bool on) { d->view->setRulersVisible(on); });
    viewMenu->addAction(tr("&Clear guides"), this, [this]() { d->scene->clearGuides(); });
    viewMenu->addSeparator();
    viewMenu->addAction(treeDock->toggleViewAction());
    viewMenu->addAction(dataDock->toggleViewAction());
    viewMenu->addAction(propDock->toggleViewAction());

    // --- Main toolbar
    QToolBar *mainBar = addToolBar(tr("Main"));
    mainBar->setObjectName(QStringLiteral("mainBar"));
    mainBar->addAction(newAct);
    mainBar->addAction(openAct);
    mainBar->addAction(saveAct);
    mainBar->addSeparator();
    mainBar->addAction(undoAct);
    mainBar->addAction(redoAct);
    mainBar->addSeparator();
    mainBar->addAction(cutAct);
    mainBar->addAction(copyAct);
    mainBar->addAction(pasteAct);
    mainBar->addAction(delAct);
    mainBar->addSeparator();
    mainBar->addAction(previewAct);
    addToolBar(alignBar);

    // --- Connections
    connect(d->scene, &BrDesignScene::layoutEdited, this, [this]() { commit(tr("Move / resize")); },
            Qt::QueuedConnection);
    connect(d->scene, &BrDesignScene::insertRequested, this,
            [this](BrContainer *c, const QPointF &pos, const QJsonObject &json) { insertObject(c, pos, json); },
            Qt::QueuedConnection);
    connect(d->scene, &BrDesignScene::selectedObjectsChanged, this, &BrDesignerWidget::syncFromScene);
    connect(d->tree, &QTreeWidget::itemSelectionChanged, this, &BrDesignerWidget::syncFromTree);
    connect(d->properties, &BrPropertyEditor::edited, this,
            [this](const QString &name) { commit(tr("Change %1").arg(BrPropertyEditor::labelFor(name).toLower())); },
            Qt::QueuedConnection);
    connect(d->data, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        const QByteArray json = item->data(0, Qt::UserRole).toByteArray();
        if (json.isEmpty())
            return;
        QObject *target = nullptr;
        for (QObject *o : selection())
            if (qobject_cast<BrContainer *>(o)) {
                target = o;
                break;
            }
        if (!target) {
            QMessageBox::information(this, tr("Insert field"), tr("Select a band first, or drag the field into a band."));
            return;
        }
        insertObject(target, QPointF(0, 0), QJsonDocument::fromJson(json).object());
    });
    connect(&d->undo, &QUndoStack::cleanChanged, this, [this](bool clean) {
        updateTitle();
        emit modifiedChanged(!clean);
    });

    newReport();
    setZoom(1.0);
    // Fit the page to the window once it is shown, unless the zoom was restored.
    QTimer::singleShot(0, this, [this, fitAct]() {
        if (!d->zoomRestored)
            fitAct->trigger();
    });
}

BrDesignerWidget::~BrDesignerWidget()
{
    d->scene->setReport(nullptr);
}

BrReport *BrDesignerWidget::report() const
{
    return &d->report;
}

void BrDesignerWidget::addDataSource(const QString &name, BrDataSource *dataSource)
{
    d->dataSources.insert(name, dataSource);
    d->report.addDataSource(name, dataSource);
    rebuildDataPanel();
}

void BrDesignerWidget::setDeveloperMode(bool on)
{
    d->developerMode = on;
    d->scene->setProtectionEnforced(!on);
    d->properties->setDeveloperMode(on);
    for (QAction *a : std::as_const(d->developerActions))
        a->setVisible(on);
    refreshAll();
}

bool BrDesignerWidget::isDeveloperMode() const
{
    return d->developerMode;
}

bool BrDesignerWidget::isModified() const
{
    return !d->undo.isClean();
}

QByteArray BrDesignerWidget::toJson() const
{
    return d->report.toJson();
}

void BrDesignerWidget::setSaveHandler(const SaveHandler &handler)
{
    d->saveHandler = handler;
    for (QAction *a : d->fileOnlyActions)
        a->setVisible(!handler);
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

bool BrDesignerWidget::loadFromJson(const QByteArray &json, QString *error)
{
    if (!d->report.loadFromJson(json)) {
        if (error)
            *error = d->report.errorString();
        return false;
    }
    d->current = d->report.toJson();
    d->lastParameters.clear();
    d->undo.clear();
    d->undo.setClean();
    refreshAll();
    rebuildDataPanel();
    return true;
}

void BrDesignerWidget::newReport()
{
    if (maybeSave())
        startBlankReport();
}

void BrDesignerWidget::newReportWizard()
{
    QMap<QString, BrDataSource *> dataSources;
    for (auto it = d->dataSources.begin(); it != d->dataSources.end(); ++it)
        dataSources.insert(it.key(), it.value());
    BrReportWizard wizard(dataSources, this);
    if (wizard.exec() != QDialog::Accepted || !maybeSave())
        return;
    startBlankReport();
    // A change to the blank report, so that it is not lost without asking to save it.
    BrReportFile::fromJson(&d->report, BrReportWizard::buildLayout(wizard.options()));
    commit(tr("New report wizard"));
    rebuildDataPanel();
}

void BrDesignerWidget::startBlankReport()
{
    const QString page = tr("Page %1 of %2");
    const QString json = QStringLiteral(R"({"format":"BrReport","version":1,"report":{"title":"%1","children":[
        {"type":"Band","bandType":"Header","height":12,"borders":{"sides":"b","width":0.3},"children":[
            {"type":"SystemInfo","info":"Title","geometry":[0,0,120,10],"font":{"family":"Arial","size":14,"bold":true}}]},
        {"type":"Band","bandType":"Detail","height":7},
        {"type":"Band","bandType":"Footer","height":6,"children":[
            {"type":"SystemInfo","info":"PagePreview","format":"%2","geometry":[130,0,60,6],"alignment":"Right"}]}]}})")
                             .arg(tr("New report"), page);
    loadFromJson(json.toUtf8());
    m_fileName.clear();
    updateTitle();
    emit fileNameChanged(m_fileName);
}

bool BrDesignerWidget::openFile(const QString &fileName)
{
    if (!maybeSave())
        return false;
    QString file = fileName;
    if (file.isEmpty())
        file = QFileDialog::getOpenFileName(this, tr("Open report"), QString(), tr("Reports (*.brt)"));
    if (file.isEmpty())
        return false;
    QFile f(file);
    QString error;
    if (!f.open(QIODevice::ReadOnly) || !loadFromJson(f.readAll(), &error)) {
        QMessageBox::warning(this, tr("Open report"),
                             tr("Could not open '%1'.\n%2").arg(file, error.isEmpty() ? f.errorString() : error));
        return false;
    }
    m_fileName = file;
    addRecentFile(file);
    updateTitle();
    emit fileNameChanged(m_fileName);
    return true;
}

bool BrDesignerWidget::save()
{
    if (d->saveHandler) {
        if (!d->saveHandler(toJson()))
            return false;
        d->undo.setClean();
        emit saved();
        return true;
    }
    if (m_fileName.isEmpty())
        return saveAs();
    if (!d->report.save(m_fileName)) {
        QMessageBox::warning(this, tr("Save report"), tr("Could not write '%1'.").arg(m_fileName));
        return false;
    }
    addRecentFile(m_fileName);
    d->undo.setClean();
    emit saved();
    return true;
}

bool BrDesignerWidget::saveAs()
{
    QString file = QFileDialog::getSaveFileName(this, tr("Save report"), m_fileName, tr("Reports (*.brt)"));
    if (file.isEmpty())
        return false;
    if (QFileInfo(file).suffix().isEmpty())
        file += QStringLiteral(".brt");
    m_fileName = file;
    emit fileNameChanged(m_fileName);
    return save();
}

bool BrDesignerWidget::maybeSave()
{
    if (!isModified())
        return true;
    const auto answer = QMessageBox::question(this, tr("Report Designer"), tr("Save the changes to the report?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void BrDesignerWidget::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

void BrDesignerWidget::saveSettings(QSettings &settings) const
{
    settings.beginGroup(QLatin1String(kSettingsGroup));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("windowState"), saveState());
    settings.setValue(QStringLiteral("zoom"), d->zoom);
    settings.setValue(QStringLiteral("recentFiles"), d->recentFiles);
    settings.endGroup();
}

void BrDesignerWidget::restoreSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String(kSettingsGroup));
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("windowState")).toByteArray());
    if (settings.contains(QStringLiteral("zoom"))) {
        setZoom(settings.value(QStringLiteral("zoom")).toDouble());
        d->zoomRestored = true;
    }
    d->recentFiles = settings.value(QStringLiteral("recentFiles")).toStringList().mid(0, kMaxRecentFiles);
    settings.endGroup();
    updateRecentMenu();
}

void BrDesignerWidget::addRecentFile(const QString &fileName)
{
    const QString path = QFileInfo(fileName).absoluteFilePath();
    d->recentFiles.removeAll(path);
    d->recentFiles.prepend(path);
    d->recentFiles = d->recentFiles.mid(0, kMaxRecentFiles);
    updateRecentMenu();
}

void BrDesignerWidget::openRecentFile(const QString &fileName)
{
    // A file that is gone leaves the list; one that failed to open may be fixed later.
    if (!openFile(fileName) && !QFileInfo::exists(fileName)) {
        d->recentFiles.removeAll(fileName);
        updateRecentMenu();
    }
}

void BrDesignerWidget::updateRecentMenu()
{
    d->recentMenu->clear();
    d->recentMenu->setEnabled(!d->recentFiles.isEmpty());
    for (int i = 0; i < d->recentFiles.size(); ++i) {
        const QString path = d->recentFiles.at(i);
        QString name = QFileInfo(path).fileName();
        name.replace(QLatin1Char('&'), QStringLiteral("&&"));
        QAction *a = d->recentMenu->addAction(QStringLiteral("&%1 %2").arg(i + 1).arg(name));
        a->setToolTip(QDir::toNativeSeparators(path));
        // Queued: opening the file rebuilds this menu, deleting the action.
        connect(a, &QAction::triggered, this, [this, path]() { openRecentFile(path); }, Qt::QueuedConnection);
    }
    d->recentMenu->addSeparator();
    QAction *clear = d->recentMenu->addAction(tr("&Clear list"));
    connect(clear, &QAction::triggered, this, [this]() {
        d->recentFiles.clear();
        updateRecentMenu();
    }, Qt::QueuedConnection);
}

void BrDesignerWidget::updateTitle()
{
    const QString name = m_fileName.isEmpty() ? d->report.title() : QFileInfo(m_fileName).fileName();
    setWindowTitle(tr("%1%2 - Report Designer").arg(name, isModified() ? QStringLiteral(" *") : QString()));
}

void BrDesignerWidget::preview()
{
    // Values typed for a preview are remembered here, never saved in the report.
    const QVariantMap variables = d->report.variables();
    QVariantMap values = variables;
    for (auto it = d->lastParameters.cbegin(); it != d->lastParameters.cend(); ++it)
        values.insert(it.key(), it.value());
    d->report.setVariables(values);
    if (!d->report.parameters().isEmpty()) {
        BrParametersDialog dialog(&d->report, this);
        if (dialog.exec() != QDialog::Accepted) {
            d->report.setVariables(variables);
            return;
        }
        d->lastParameters = dialog.values();
        for (auto it = d->lastParameters.cbegin(); it != d->lastParameters.cend(); ++it)
            d->report.setVariable(it.key(), it.value());
    }
    d->report.clearDocument();
    const bool prepared = d->report.prepare();
    d->report.setVariables(variables);
    if (!prepared && d->report.document().isEmpty()) {
        QMessageBox::warning(this, tr("Preview"), d->report.errorString());
        return;
    }
    BrPreviewDialog dialog(this);
    dialog.setDocument(d->report.document());
    dialog.exec();
    d->report.clearDocument();
}

void BrDesignerWidget::undo()
{
    d->undo.undo();
}

void BrDesignerWidget::redo()
{
    d->undo.redo();
}

// ---------------------------------------------------------------------------
// Undo / refresh
// ---------------------------------------------------------------------------

void BrDesignerWidget::commit(const QString &text)
{
    const QByteArray after = d->report.toJson();
    if (after != d->current) {
        d->undo.push(new BrSnapshotCommand(this, d->current, after, text));
        d->current = after;
    }
    refreshAll();
}

void BrDesignerWidget::restoreSnapshot(const QByteArray &json)
{
    QList<QList<int>> paths;
    for (QObject *o : selection())
        paths << pathOf(o, &d->report);
    BrReportFile::fromJson(&d->report, json);
    d->current = json;
    refreshAll();
    QList<QObject *> objs;
    for (const QList<int> &p : paths)
        if (QObject *o = objectAt(p, &d->report))
            objs << o;
    d->scene->selectObjects(objs);
    showProperties();
}

void BrDesignerWidget::refreshAll()
{
    d->syncing = true;
    d->scene->rebuild();
    rebuildTree();
    d->syncing = false;
    showProperties();
    updateTitle();
}

QList<QObject *> BrDesignerWidget::selection() const
{
    QList<QObject *> list = d->scene->selectedObjects();
    if (list.isEmpty()) {
        for (QTreeWidgetItem *item : d->tree->selectedItems())
            if (auto *o = static_cast<QObject *>(item->data(0, Qt::UserRole).value<void *>()))
                list << o;
    }
    return list;
}

void BrDesignerWidget::showProperties()
{
    QList<QObject *> objs = selection();
    if (objs.isEmpty())
        objs << &d->report;
    d->properties->setObjects(objs);
}

void BrDesignerWidget::rebuildTree()
{
    const bool was = d->syncing;
    d->syncing = true;
    const QList<QObject *> selected = d->scene->selectedObjects();
    const int scroll = d->tree->verticalScrollBar()->value();
    d->tree->clear();
    std::function<void(QObject *, QTreeWidgetItem *)> add = [&](QObject *o, QTreeWidgetItem *parent) {
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(d->tree);
        item->setText(0, describe(o));
        item->setData(0, Qt::UserRole, QVariant::fromValue<void *>(o));
        Qt::ItemFlags flags = item->flags();
        flags.setFlag(Qt::ItemIsDragEnabled, parent != nullptr);
        flags.setFlag(Qt::ItemIsEditable, canRename(o));
        item->setFlags(flags);
        if (BrProtection::of(o).any())
            item->setIcon(0, brProtectionIcon());
        item->setSelected(selected.contains(o));
        for (QObject *c : layoutChildren(o))
            add(c, item);
    };
    add(&d->report, nullptr);
    d->tree->expandAll();
    d->tree->setScrollPosition(scroll);
    d->syncing = was;
}

void BrDesignerWidget::syncFromScene()
{
    if (d->syncing)
        return;
    d->syncing = true;
    const QList<QObject *> selected = d->scene->selectedObjects();
    QTreeWidgetItemIterator it(d->tree);
    while (*it) {
        auto *o = static_cast<QObject *>((*it)->data(0, Qt::UserRole).value<void *>());
        (*it)->setSelected(selected.contains(o));
        ++it;
    }
    d->syncing = false;
    showProperties();
}

void BrDesignerWidget::syncFromTree()
{
    if (d->syncing)
        return;
    d->syncing = true;
    QList<QObject *> objs;
    for (QTreeWidgetItem *item : d->tree->selectedItems())
        if (auto *o = static_cast<QObject *>(item->data(0, Qt::UserRole).value<void *>()))
            objs << o;
    d->scene->selectObjects(objs);
    d->syncing = false;
    if (objs.isEmpty())
        objs << &d->report;
    d->properties->setObjects(objs);
}

// ---------------------------------------------------------------------------
// Data panel
// ---------------------------------------------------------------------------

void BrDesignerWidget::rebuildDataPanel()
{
    d->data->clear();
    QString queryError;
    if (!d->report.queries().isEmpty() && !d->report.openQueries())
        queryError = d->report.errorString();

    QStringList queryNames;
    for (const QVariant &q : d->report.queries())
        queryNames << q.toMap().value(QStringLiteral("name")).toString();

    auto addTemplate = [](QTreeWidgetItem *item, const QJsonObject &json) {
        item->setData(0, Qt::UserRole, QJsonDocument(json).toJson(QJsonDocument::Compact));
    };

    for (const QString &name : d->report.dataSourceNames()) {
        auto *top = new QTreeWidgetItem(d->data);
        const bool isQuery = queryNames.contains(name, Qt::CaseInsensitive);
        top->setText(0, isQuery ? tr("%1 (SQL)").arg(name) : name);
        top->setData(0, Qt::UserRole + 1, name);
        BrDataSource *ds = d->report.findDataSource(name);
        QStringList fields;
        if (ds) {
            ds->first();
            fields = ds->fieldNames();
        }
        if (fields.isEmpty()) {
            auto *empty = new QTreeWidgetItem(top);
            empty->setText(0, queryError.isEmpty() ? tr("(no fields)") : tr("(not available: %1)").arg(queryError));
            empty->setDisabled(true);
        }
        for (const QString &field : fields) {
            auto *item = new QTreeWidgetItem(top);
            item->setText(0, field);
            item->setToolTip(0, tr("Drag into a band"));
            addTemplate(item, geometryTemplate("DBText", 30, 6, {{QStringLiteral("dataField"), field}}));
        }
    }

    QStringList parameterNames;
    if (!d->report.parameters().isEmpty()) {
        auto *top = new QTreeWidgetItem(d->data);
        top->setText(0, tr("Parameters"));
        for (const QVariant &p : d->report.parameters()) {
            const QVariantMap parameter = p.toMap();
            const QString name = parameter.value(QStringLiteral("name")).toString();
            parameterNames << name;
            auto *item = new QTreeWidgetItem(top);
            item->setText(0, name);
            item->setToolTip(0, tr("Drag into a band"));
            addTemplate(item, geometryTemplate("DBText", 30, 6, {{QStringLiteral("dataFormula"), name}}));
        }
    }

    QVariantMap vars = d->report.variables();
    for (const QString &name : parameterNames)
        vars.remove(name);
    if (!vars.isEmpty()) {
        auto *top = new QTreeWidgetItem(d->data);
        top->setText(0, tr("Variables"));
        for (auto it = vars.begin(); it != vars.end(); ++it) {
            auto *item = new QTreeWidgetItem(top);
            item->setText(0, it.key());
            addTemplate(item, geometryTemplate("DBText", 30, 6, {{QStringLiteral("dataFormula"), it.key()}}));
        }
    }

    auto *sys = new QTreeWidgetItem(d->data);
    sys->setText(0, tr("System"));
    const QList<QPair<QString, QJsonObject>> systemItems = {
        {tr("Page X of Y"), {{QStringLiteral("info"), QStringLiteral("PagePreview")}, {QStringLiteral("format"), tr("Page %1 of %2")}}},
        {tr("Page number"), {{QStringLiteral("info"), QStringLiteral("PageNumber")}}},
        {tr("Date"), {{QStringLiteral("info"), QStringLiteral("Date")}}},
        {tr("Date and time"), {{QStringLiteral("info"), QStringLiteral("DateTime")}}},
        {tr("Report title"), {{QStringLiteral("info"), QStringLiteral("Title")}}},
        {tr("Record number"), {{QStringLiteral("info"), QStringLiteral("RecNo")}}},
    };
    for (const auto &s : systemItems) {
        auto *item = new QTreeWidgetItem(sys);
        item->setText(0, s.first);
        addTemplate(item, geometryTemplate("SystemInfo", 40, 6, s.second));
    }
    d->data->expandAll();
}

void BrDesignerWidget::editQuery(const QString &name)
{
#ifdef BRREPORT_HAS_SQL
    if (!canEditReport("queries")) {
        refuse(tr("The queries of this report are protected."));
        return;
    }
    QVariantList queries = d->report.queries();
    int index = -1;
    for (int i = 0; i < queries.size(); ++i)
        if (queries.at(i).toMap().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0)
            index = i;
    if (!name.isEmpty() && index < 0) {
        QMessageBox::information(this, tr("Edit query"),
                                 tr("'%1' is provided by the application and cannot be edited here.").arg(name));
        return;
    }
    const QVariantMap current = index >= 0 ? queries.at(index).toMap() : QVariantMap();

    QDialog dialog(this);
    dialog.setWindowTitle(index >= 0 ? tr("Edit SQL query") : tr("New SQL query"));
    auto *nameEdit = new QLineEdit(current.value(QStringLiteral("name")).toString(), &dialog);
    auto *connCombo = new QComboBox(&dialog);
    connCombo->setEditable(true);
    connCombo->addItem(QString());
    for (const QString &c : QSqlDatabase::connectionNames())
        if (c != QLatin1String(QSqlDatabase::defaultConnection))
            connCombo->addItem(c);
    connCombo->setCurrentText(current.value(QStringLiteral("connection")).toString());
    connCombo->setToolTip(tr("Empty = the application's default database connection"));
    auto *sqlEdit = new QPlainTextEdit(current.value(QStringLiteral("sql")).toString(), &dialog);
    sqlEdit->setFont(QFont(QStringLiteral("monospace")));
    sqlEdit->setPlaceholderText(QStringLiteral("SELECT * FROM customers WHERE city = :city"));
    auto *testButton = new QPushButton(tr("Test"), &dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->addButton(testButton, QDialogButtonBox::ActionRole);
    auto *form = new QFormLayout;
    form->addRow(tr("Name"), nameEdit);
    form->addRow(tr("Connection"), connCombo);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("SQL (use :name for report parameters and variables):"), &dialog));
    layout->addWidget(sqlEdit, 1);
    layout->addWidget(buttons);
    dialog.resize(640, 420);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (nameEdit->text().trimmed().isEmpty() || sqlEdit->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, dialog.windowTitle(), tr("Name and SQL are required."));
            return;
        }
        dialog.accept();
    });
    connect(testButton, &QPushButton::clicked, &dialog, [&]() {
        const QString conn = connCombo->currentText();
        QSqlDatabase db = conn.isEmpty() ? QSqlDatabase::database() : QSqlDatabase::database(conn);
        QSqlQuery q(db);
        QString message;
        if (!db.isOpen())
            message = tr("The connection is not open.");
        else if (!q.prepare(sqlEdit->toPlainText()))
            message = q.lastError().text();
        else {
            const QVariantMap vars = d->report.variables();
            for (auto it = vars.begin(); it != vars.end(); ++it)
                q.bindValue(QLatin1Char(':') + it.key(), it.value());
            for (const QVariant &p : d->report.parameters()) {
                const QString parameter = p.toMap().value(QStringLiteral("name")).toString();
                q.bindValue(QLatin1Char(':') + parameter, d->report.variable(parameter));
            }
            if (!q.exec())
                message = q.lastError().text();
            else {
                QStringList fields;
                for (int i = 0; i < q.record().count(); ++i)
                    fields << q.record().fieldName(i);
                int rows = 0;
                while (q.next() && rows < 100000)
                    ++rows;
                message = tr("OK: %1 row(s).\nFields: %2").arg(rows).arg(fields.join(QStringLiteral(", ")));
            }
        }
        QMessageBox::information(&dialog, tr("Test query"), message);
    });
    if (dialog.exec() != QDialog::Accepted)
        return;

    QVariantMap q{{QStringLiteral("name"), nameEdit->text().trimmed()},
                  {QStringLiteral("sql"), sqlEdit->toPlainText().trimmed()}};
    if (!connCombo->currentText().isEmpty())
        q.insert(QStringLiteral("connection"), connCombo->currentText());
    if (index >= 0)
        queries[index] = q;
    else
        queries << q;
    d->report.setQueries(queries);
    commit(index >= 0 ? tr("Edit query") : tr("Add query"));
    rebuildDataPanel();
#else
    Q_UNUSED(name)
#endif
}

void BrDesignerWidget::editParameters()
{
    if (!canEditReport("parameters")) {
        refuse(tr("The parameters of this report are protected."));
        return;
    }
    BrParametersEditor editor(d->report.parameters(), &d->report, this);
    if (editor.exec() != QDialog::Accepted)
        return;
    d->report.setParameters(editor.parameters());
    d->lastParameters.clear();
    commit(tr("Edit parameters"));
    rebuildDataPanel();
}

void BrDesignerWidget::removeQuery(const QString &name)
{
    if (!canEditReport("queries")) {
        refuse(tr("The queries of this report are protected."));
        return;
    }
    QVariantList queries = d->report.queries();
    for (int i = 0; i < queries.size(); ++i) {
        if (queries.at(i).toMap().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) {
            queries.removeAt(i);
            d->report.setQueries(queries);
            d->report.addDataSource(name, d->dataSources.value(name)); // restore/unregister
            commit(tr("Remove query"));
            rebuildDataPanel();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

bool BrDesignerWidget::canMove(const QObject *object) const
{
    return d->developerMode || !BrProtection::of(object).move;
}

bool BrDesignerWidget::canDelete(const QObject *object) const
{
    if (d->developerMode)
        return true;
    if (BrProtection::of(object->parent()).contents)
        return false;
    // Deleting an object also deletes everything inside it.
    QList<QObject *> affected = object->findChildren<QObject *>();
    affected << const_cast<QObject *>(object);
    for (const QObject *o : std::as_const(affected)) {
        const BrProtection protection = BrProtection::of(o);
        if (protection.remove || protection.contents)
            return false;
    }
    return true;
}

bool BrDesignerWidget::canAddTo(const QObject *object) const
{
    return d->developerMode || !BrProtection::of(object).contents;
}

bool BrDesignerWidget::canRename(const QObject *object) const
{
    if (d->developerMode)
        return true;
    const BrProtection protection = BrProtection::of(object);
    return !protection.properties && !protection.hiddenProperties.contains(QStringLiteral("objectName"));
}

bool BrDesignerWidget::canEditReport(const char *property) const
{
    if (d->developerMode)
        return true;
    const BrProtection protection = d->report.protection();
    return !protection.properties && !protection.hiddenProperties.contains(QString::fromLatin1(property));
}

void BrDesignerWidget::refuse(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

void BrDesignerWidget::setSelectionProtected(bool on)
{
    QList<QObject *> affected;
    for (QObject *o : selection()) {
        affected << o;
        affected << o->findChildren<QObject *>();
    }
    for (QObject *o : std::as_const(affected)) {
        if (!isLayoutObject(o))
            continue;
        const BrProtection protection =
            on ? BrProtection::all(BrProtection::of(o).hiddenProperties) : BrProtection();
        o->setProperty("protection", QVariant::fromValue(protection));
    }
    commit(tr("Change protection"));
}

void BrDesignerWidget::insertObject(QObject *container, const QPointF &pos, const QJsonObject &templ)
{
    if (!canAddTo(container)) {
        d->pointerTool->setChecked(true);
        d->scene->setInsertTemplate(QJsonObject());
        refuse(tr("Nothing can be added to this object: it is protected."));
        return;
    }
    QJsonObject json = templ;
    QJsonArray g = json.value(QStringLiteral("geometry")).toArray();
    const double w = g.size() == 4 ? g.at(2).toDouble() : 30.0;
    const double h = g.size() == 4 ? g.at(3).toDouble() : 6.0;
    json.insert(QStringLiteral("geometry"), QJsonArray{pos.x(), pos.y(), w, h});
    QString error;
    QObject *o = BrReportFile::objectFromJson(json, container, &error);
    d->pointerTool->setChecked(true);
    d->scene->setInsertTemplate(QJsonObject());
    if (!o) {
        QMessageBox::warning(this, tr("Insert"), error);
        return;
    }
    commit(tr("Insert %1").arg(BrDesignerLabels::typeLabel(BrReportFile::typeName(o)).toLower()));
    d->scene->selectObjects({o});
}

void BrDesignerWidget::copySelection()
{
    copyObjects(selection());
}

void BrDesignerWidget::copyObjects(const QList<QObject *> &objects)
{
    QJsonArray array;
    for (QObject *o : objects)
        if (o != &d->report)
            array << BrReportFile::objectToJson(o);
    if (array.isEmpty())
        return;
    auto *mime = new QMimeData;
    const QByteArray json = QJsonDocument(array).toJson(QJsonDocument::Compact);
    mime->setData(QStringLiteral(BRREPORT_OBJECT_MIME), json);
    mime->setText(QString::fromUtf8(json));
    QApplication::clipboard()->setMimeData(mime);
}

void BrDesignerWidget::paste()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(QStringLiteral(BRREPORT_OBJECT_MIME)))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(mime->data(QStringLiteral(BRREPORT_OBJECT_MIME)));
    QJsonArray array = doc.isArray() ? doc.array() : QJsonArray{doc.object()};

    // Target: selected container, else the parent of the selected object, else the first detail band
    QObject *selected = selection().value(0);
    QList<QObject *> created;
    for (const QJsonValue &v : array) {
        QJsonObject json = d->developerMode ? v.toObject() : withoutProtection(v.toObject());
        const QString type = json.value(QStringLiteral("type")).toString();
        QObject *target = nullptr;
        for (QObject *o = selected; o; o = o->parent())
            if (BrReportFile::canContain(o, type) && canAddTo(o)) {
                target = o;
                break;
            }
        if (!target && BrReportFile::canContain(&d->report, type) && canAddTo(&d->report))
            target = &d->report;
        if (!target) {
            const QList<BrBand *> details = d->report.findChildren<BrBand *>();
            for (BrBand *b : details)
                if (b->bandType() == Br::BandType::Detail && canAddTo(b)) {
                    target = b;
                    break;
                }
        }
        if (!target)
            continue;
        QJsonArray g = json.value(QStringLiteral("geometry")).toArray();
        if (g.size() == 4 && target == (selected ? selected->parent() : nullptr)) {
            g[0] = g.at(0).toDouble() + 2.0;
            g[1] = g.at(1).toDouble() + 2.0;
            json.insert(QStringLiteral("geometry"), g);
        }
        if (QObject *o = BrReportFile::objectFromJson(json, target))
            created << o;
    }
    if (created.isEmpty()) {
        if (!array.isEmpty())
            refuse(tr("Nothing can be added here: the object is protected."));
        return;
    }
    commit(tr("Paste"));
    d->scene->selectObjects(created);
}

void BrDesignerWidget::duplicateSelection()
{
    copySelection();
    paste();
}

void BrDesignerWidget::deleteSelection()
{
    const QList<QObject *> selected = selection();
    QList<QObject *> objs;
    for (QObject *o : selected)
        if (o != &d->report && canDelete(o))
            objs << o;
    if (objs.size() < selected.size() - int(selected.contains(&d->report)))
        refuse(tr("Protected objects were not deleted."));
    if (objs.isEmpty())
        return;
    d->scene->clearSelection();
    d->tree->clearSelection();
    for (QObject *o : objs) {
        // Skip objects whose ancestor is also being deleted
        bool nested = false;
        for (QObject *p = o->parent(); p; p = p->parent())
            if (objs.contains(p))
                nested = true;
        if (!nested)
            delete o;
    }
    commit(tr("Delete"));
}

void BrDesignerWidget::moveInOrder(int delta)
{
    const QList<QObject *> objs = selection();
    if (objs.size() != 1 || objs.first() == &d->report)
        return;
    QObject *o = objs.first();
    if (!canMove(o)) {
        refuse(tr("This object is protected and cannot be moved."));
        return;
    }
    QObject *parent = o->parent();
    QList<QObject *> siblings = layoutChildren(parent);
    const int i = int(siblings.indexOf(o));
    const int j = i + delta;
    if (j < 0 || j >= siblings.size())
        return;
    siblings.swapItemsAt(i, j);
    setChildOrder(parent, siblings);
    commit(tr("Change order"));
    d->scene->selectObjects({o});
}

void BrDesignerWidget::moveInTree(const QList<QObject *> &objects, QObject *parent, int index)
{
    for (const QObject *o : objects)
        if (!canMove(o)) {
            refuse(tr("This object is protected and cannot be moved."));
            return;
        }
    setChildOrder(parent, childrenWithMoved(parent, objects, index));
    commit(tr("Change order"));
    d->scene->selectObjects(objects);
}

void BrDesignerWidget::renameObject(QObject *object, const QString &name)
{
    if (object->objectName() == name || !canRename(object))
        return;
    object->setObjectName(name);
    commit(tr("Rename"));
    d->scene->selectObjects({object});
}

void BrDesignerWidget::align(int how)
{
    QList<BrControl *> controls;
    for (QObject *o : selection())
        if (auto *c = qobject_cast<BrControl *>(o))
            if (!qobject_cast<BrBand *>(c))
                controls << c;
    if (controls.isEmpty())
        return;
    const QRectF ref = controls.first()->geometry(); // a protected object can be the reference
    for (BrControl *c : controls) {
        if (!canMove(c))
            continue;
        QRectF g = c->geometry();
        switch (how) {
        case 0: g.moveLeft(ref.left()); break;
        case 1: g.moveRight(ref.right()); break;
        case 2: g.moveTop(ref.top()); break;
        case 3: g.moveBottom(ref.bottom()); break;
        case 4:
            if (auto *container = c->parentContainer()) {
                double width = container->width();
                if (auto *b = qobject_cast<BrBand *>(container))
                    width = b->report() ? b->report()->contentRect().width() : width;
                g.moveLeft((width - g.width()) / 2.0);
            }
            break;
        case 5: g.setWidth(ref.width()); break;
        case 6: g.setHeight(ref.height()); break;
        }
        c->setGeometry(g);
    }
    commit(tr("Align"));
}

void BrDesignerWidget::distribute(Qt::Orientation orientation)
{
    // Positions are relative to the parent: only objects of the same one can be compared.
    QHash<BrContainer *, QList<BrControl *>> siblings;
    for (QObject *o : selection())
        if (auto *c = qobject_cast<BrControl *>(o))
            if (!qobject_cast<BrBand *>(c) && c->align() == Br::Align::None)
                siblings[c->parentContainer()] << c;

    const bool horizontal = orientation == Qt::Horizontal;
    const auto start = [horizontal](const BrControl *c) { return horizontal ? c->x() : c->y(); };
    const auto size = [horizontal](const BrControl *c) { return horizontal ? c->width() : c->height(); };

    // Equal gaps between the objects from \a first to \a last, which stay in place.
    const auto spread = [&](const QList<BrControl *> &list, int first, int last) {
        double free = start(list.at(last)) + size(list.at(last)) - start(list.at(first));
        for (int i = first; i <= last; ++i)
            free -= size(list.at(i));
        const double gap = free / (last - first);
        double position = start(list.at(first)) + size(list.at(first)) + gap;
        for (int i = first + 1; i < last; ++i) {
            BrControl *c = list.at(i);
            QRectF g = c->geometry();
            if (horizontal)
                g.moveLeft(position);
            else
                g.moveTop(position);
            c->setGeometry(g);
            position += size(c) + gap;
        }
    };

    bool enough = false;
    for (QList<BrControl *> &list : siblings) {
        if (list.size() < 3)
            continue;
        enough = true;
        std::stable_sort(list.begin(), list.end(),
                         [&](const BrControl *a, const BrControl *b) { return start(a) < start(b); });
        // A locked object stays where it is and the others are spread around it.
        int first = 0;
        for (int i = 1; i < list.size(); ++i) {
            if (i == list.size() - 1 || !canMove(list.at(i))) {
                spread(list, first, i);
                first = i;
            }
        }
    }
    if (!enough) {
        refuse(tr("Select at least three objects of the same band to distribute them."));
        return;
    }
    commit(tr("Distribute"));
}

void BrDesignerWidget::nudge(double dx, double dy)
{
    bool moved = false;
    for (QObject *o : selection()) {
        auto *c = qobject_cast<BrControl *>(o);
        if (!c || qobject_cast<BrBand *>(c) || c->align() != Br::Align::None || !canMove(c))
            continue;
        c->setGeometry(c->geometry().translated(dx, dy));
        moved = true;
    }
    if (moved)
        commit(tr("Move"));
}

QObject *BrDesignerWidget::targetPager() const
{
    for (QObject *o : selection()) {
        for (QObject *p = o; p; p = p->parent())
            if (qobject_cast<BrPager *>(p))
                return p;
    }
    return &d->report;
}

void BrDesignerWidget::addBand(int type)
{
    auto *pager = qobject_cast<BrPager *>(targetPager());
    if (!canAddTo(pager)) {
        refuse(tr("Nothing can be added here: the object is protected."));
        return;
    }
    BrBand *band = pager->addBand(Br::BandType(type), 8.0);
    commit(tr("Add band"));
    d->scene->selectObjects({band});
}

void BrDesignerWidget::addGroup()
{
    auto *pager = qobject_cast<BrPager *>(targetPager());
    while (BrGroup *g = pager->group())
        pager = g;
    if (!canAddTo(pager)) {
        refuse(tr("Nothing can be added here: the object is protected."));
        return;
    }

    // Field list from the pager's data source
    QStringList fields;
    BrDataSource *ds = nullptr;
    for (BrPager *p = pager; p && !ds; p = p->parentPager()) {
        if (auto *sub = qobject_cast<BrSubDetail *>(p))
            ds = sub->dataSource();
        else if (auto *r = qobject_cast<BrReport *>(p))
            ds = r->dataSource();
    }
    if (ds) {
        ds->first();
        fields = ds->fieldNames();
    }
    bool ok = false;
    const QString field = QInputDialog::getItem(this, tr("Add group"),
                                                tr("Group the records by (the data must be sorted by it):"),
                                                fields, 0, true, &ok);
    if (!ok || field.trimmed().isEmpty())
        return;

    auto *group = new BrGroup(pager);
    group->setDataFields(field.trimmed());
    // The records now belong to the group: move the detail bands and sub-reports into it.
    for (QObject *c : layoutChildren(pager)) {
        auto *b = qobject_cast<BrBand *>(c);
        if ((b && b->bandType() == Br::BandType::Detail) || qobject_cast<BrSubDetail *>(c)) {
            c->setParent(nullptr);
            c->setParent(group);
        }
    }
    QJsonObject headerJson{{QStringLiteral("type"), QStringLiteral("Band")},
                           {QStringLiteral("bandType"), QStringLiteral("Header")},
                           {QStringLiteral("height"), 8},
                           {QStringLiteral("background"), QStringLiteral("#dce6f2")},
                           {QStringLiteral("children"),
                            QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("DBText")},
                                                   {QStringLiteral("dataField"), field.trimmed()},
                                                   {QStringLiteral("geometry"), QJsonArray{1, 0, 80, 8}},
                                                   {QStringLiteral("font"),
                                                    QJsonObject{{QStringLiteral("family"), QStringLiteral("Arial")},
                                                                {QStringLiteral("size"), 10},
                                                                {QStringLiteral("bold"), true}}}}}}};
    QObject *h = BrReportFile::objectFromJson(headerJson, group);
    // Keep the header first among the group's children
    for (QObject *c : layoutChildren(group))
        if (c != h) {
            c->setParent(nullptr);
            c->setParent(group);
        }
    group->addBand(Br::BandType::Summary, 6.0);
    commit(tr("Add group"));
    d->scene->selectObjects({h});
}

void BrDesignerWidget::addTemplatePage(Br::BandType position)
{
    if (!canAddTo(&d->report)) {
        refuse(tr("Nothing can be added here: the object is protected."));
        return;
    }
    auto *page = new BrSubDetail(&d->report);
    page->setPositioning(position);
    page->setPageBreak(position == Br::BandType::Title ? Br::PageBreak::After : Br::PageBreak::Before);
    BrBand *band = page->addBand(Br::BandType::Detail, 60.0);
    commit(tr("Add page"));
    d->scene->selectObjects({band});
}

void BrDesignerWidget::addSubReport()
{
    auto *pager = qobject_cast<BrPager *>(targetPager());
    while (BrGroup *g = pager->group())
        pager = g;
    if (!canAddTo(pager)) {
        refuse(tr("Nothing can be added here: the object is protected."));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getItem(this, tr("Add sub-report"),
                                               tr("Data source of the sub-report (e.g. the items of each order):"),
                                               d->report.dataSourceNames(), 0, true, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    auto *sub = new BrSubDetail(pager);
    sub->setDataSourceName(name.trimmed());
    BrBand *band = sub->addBand(Br::BandType::Detail, 6.0);
    commit(tr("Add sub-report"));
    d->scene->selectObjects({band});
    QMessageBox::information(this, tr("Add sub-report"),
                             tr("Set 'Master fields' and 'Detail fields' of the sub-report (select it in the "
                                "Structure panel) to print only the records of each master record."));
}

// ---------------------------------------------------------------------------
// View
// ---------------------------------------------------------------------------

void BrDesignerWidget::setZoom(double zoom)
{
    d->zoom = qBound(0.25, zoom, 6.0);
    const double k = d->zoom * logicalDpiX() / 25.4;
    d->view->setTransform(QTransform::fromScale(k, k));
    d->view->updateRulers();
    statusBar()->showMessage(tr("Zoom %1%").arg(qRound(d->zoom * 100)), 1500);
}

bool BrDesignerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d->view->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            setZoom(wheel->angleDelta().y() > 0 ? d->zoom * 1.15 : d->zoom / 1.15);
            return true;
        }
    }
    if (watched == d->view && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        const double step = (key->modifiers() & Qt::ShiftModifier) ? 5.0
                            : (key->modifiers() & Qt::AltModifier) ? 0.1 : 1.0;
        switch (key->key()) {
        case Qt::Key_Left: nudge(-step, 0); return true;
        case Qt::Key_Right: nudge(step, 0); return true;
        case Qt::Key_Up: nudge(0, -step); return true;
        case Qt::Key_Down: nudge(0, step); return true;
        case Qt::Key_Escape:
            d->pointerTool->setChecked(true);
            d->scene->setInsertTemplate(QJsonObject());
            return true;
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
