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

#ifndef BRDESIGNERWIDGET_H
#define BRDESIGNERWIDGET_H

#include "brdesignerglobal.h"

#include <QMainWindow>

#include <functional>
#include <memory>

class BrDataSource;
class BrReport;
class QJsonObject;
class QMenu;

/*!
 * Visual report designer: page with bands and objects, toolbox, structure tree,
 * data panel (drag fields into bands), property editor, undo/redo, clipboard,
 * alignment, groups, sub-reports, SQL queries and preview.
 *
 * It is a QMainWindow, so it can be shown on its own or embedded in another
 * window as a regular widget. The application provides the data:
 *
 * \code
 * auto *designer = new BrDesignerWidget;
 * designer->addDataSource("customers", &customers);   // and/or open QSqlDatabase connections
 * designer->openFile("customers.brt");
 * designer->show();
 * \endcode
 *
 * To store reports somewhere else (e.g. in a database), install a save handler
 * and load reports with loadFromJson().
 */
class BRREPORT_DESIGNER_EXPORT BrDesignerWidget : public QMainWindow
{
    Q_OBJECT
public:
    using SaveHandler = std::function<bool(const QByteArray &json)>;

    explicit BrDesignerWidget(QWidget *parent = nullptr);
    ~BrDesignerWidget() override;

    /*! The report being edited (owned by the designer). */
    BrReport *report() const;

    /*! Makes a data source available to the reports (kept across New/Open). */
    void addDataSource(const QString &name, BrDataSource *dataSource);

    QString fileName() const { return m_fileName; }
    bool isModified() const;

    /*! Layout as JSON (report file contents). */
    QByteArray toJson() const;
    /*! Loads a layout from JSON (e.g. read from a database). Clears the undo history. */
    bool loadFromJson(const QByteArray &json, QString *error = nullptr);

    /*!
     * When set, Save calls \a handler with the JSON instead of writing a file
     * (and Save As/Open file actions are hidden). Return true on success.
     */
    void setSaveHandler(const SaveHandler &handler);

    /*! Menus, so applications can add their own actions. */
    QMenu *fileMenu() const { return m_fileMenu; }
    QMenu *dataMenu() const { return m_dataMenu; }

public slots:
    void newReport();
    bool openFile(const QString &fileName = QString());
    bool save();
    bool saveAs();
    /*! Asks to save pending changes. Returns false if the user cancels. */
    bool maybeSave();
    void preview();
    void undo();
    void redo();

signals:
    void modifiedChanged(bool modified);
    void fileNameChanged(const QString &fileName);
    void saved();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
    QString m_fileName;
    QMenu *m_fileMenu = nullptr;
    QMenu *m_dataMenu = nullptr;

    friend class BrSnapshotCommand;
    void restoreSnapshot(const QByteArray &json);
    void commit(const QString &text);
    void refreshAll();
    void rebuildTree();
    void rebuildDataPanel();
    void updateTitle();
    void syncFromScene();
    void syncFromTree();
    void showProperties();
    void insertObject(QObject *container, const QPointF &pos, const QJsonObject &json);

    QList<QObject *> selection() const;
    void copySelection();
    void paste();
    void deleteSelection();
    void duplicateSelection();
    void moveInOrder(int delta);
    void align(int how);
    void nudge(double dx, double dy);
    void addBand(int type);
    void addGroup();
    void addSubReport();
    void editQuery(const QString &name);
    void removeQuery(const QString &name);
    void setZoom(double zoom);
    QObject *targetPager() const;
};

#endif // BRDESIGNERWIDGET_H
