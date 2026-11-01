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

// brdesigner: standalone visual report designer.
//
//   brdesigner [report.brt]
//
// Data comes from SQL queries stored in the report; configure the database in
// Data > Database connection (the password is never saved).

#include "brdesignerwidget.h"
#include "brtranslations.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>

#ifdef BRREPORT_HAS_SQL
#include <QSqlDatabase>
#include <QSqlError>

namespace {

bool openConnection(const QString &driver, const QString &database, const QString &host, int port,
                    const QString &user, const QString &password, QString *error)
{
    if (QSqlDatabase::contains(QLatin1String(QSqlDatabase::defaultConnection)))
        QSqlDatabase::removeDatabase(QLatin1String(QSqlDatabase::defaultConnection));
    QSqlDatabase db = QSqlDatabase::addDatabase(driver);
    db.setDatabaseName(database);
    db.setHostName(host);
    if (port > 0)
        db.setPort(port);
    db.setUserName(user);
    db.setPassword(password);
    if (!db.open()) {
        *error = db.lastError().text();
        return false;
    }
    return true;
}

void connectionDialog(QWidget *parent)
{
    QSettings settings;
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Database connection"));
    auto *driver = new QComboBox(&dialog);
    driver->addItems(QSqlDatabase::drivers());
    driver->setCurrentText(settings.value(QStringLiteral("db/driver"), QStringLiteral("QSQLITE")).toString());
    auto *database = new QLineEdit(settings.value(QStringLiteral("db/database")).toString(), &dialog);
    database->setPlaceholderText(QObject::tr("Database name or SQLite file"));
    auto *host = new QLineEdit(settings.value(QStringLiteral("db/host")).toString(), &dialog);
    auto *port = new QSpinBox(&dialog);
    port->setRange(0, 65535);
    port->setValue(settings.value(QStringLiteral("db/port"), 0).toInt());
    port->setSpecialValueText(QObject::tr("default"));
    auto *user = new QLineEdit(settings.value(QStringLiteral("db/user")).toString(), &dialog);
    auto *password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto *form = new QFormLayout(&dialog);
    form->addRow(QObject::tr("Driver"), driver);
    form->addRow(QObject::tr("Database"), database);
    form->addRow(QObject::tr("Host"), host);
    form->addRow(QObject::tr("Port"), port);
    form->addRow(QObject::tr("User"), user);
    form->addRow(QObject::tr("Password"), password);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QString error;
        if (!openConnection(driver->currentText(), database->text(), host->text(), port->value(), user->text(),
                            password->text(), &error)) {
            QMessageBox::warning(&dialog, dialog.windowTitle(), error);
            return;
        }
        settings.setValue(QStringLiteral("db/driver"), driver->currentText());
        settings.setValue(QStringLiteral("db/database"), database->text());
        settings.setValue(QStringLiteral("db/host"), host->text());
        settings.setValue(QStringLiteral("db/port"), port->value());
        settings.setValue(QStringLiteral("db/user"), user->text());
        dialog.accept();
    });
    dialog.exec();
}

} // namespace
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("BrReport"));
    QApplication::setApplicationName(QStringLiteral("brdesigner"));
    QApplication::setApplicationVersion(QStringLiteral(BRREPORT_VERSION_STR));
    BrTranslations::install(); // user interface in the system language (e.g. pt_BR)

    BrDesignerWidget designer;

#ifdef BRREPORT_HAS_SQL
    // Reconnect to SQLite databases automatically (no password needed).
    QSettings settings;
    if (settings.value(QStringLiteral("db/driver")).toString() == QLatin1String("QSQLITE")
        && !settings.value(QStringLiteral("db/database")).toString().isEmpty()) {
        QString error;
        openConnection(QStringLiteral("QSQLITE"), settings.value(QStringLiteral("db/database")).toString(),
                       QString(), 0, QString(), QString(), &error);
    }
    designer.dataMenu()->addSeparator();
    designer.dataMenu()->addAction(QObject::tr("Database &connection..."), &designer, [&designer]() {
        connectionDialog(&designer);
    });
#endif

    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        designer.openFile(args.at(1));
    designer.show();
    return app.exec();
}
