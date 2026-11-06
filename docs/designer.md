# Report designer

BrReport includes a visual report designer for developers **and end users**. It edits report files
(`.brt`), so a report can be created or changed without recompiling the application.

![Report designer](images/designer.png)

It is available in two forms:

- **`brdesigner`**, a standalone application (built in `tools/`).
- **`BrDesignerWidget`**, a widget in the `BrReport::Designer` library, to embed the designer in
  your application (an ERP that lets its users adjust their reports, for example).

## What users can do

| Area | Features |
|---|---|
| New report | *File > New from wizard*: choose the data and the fields, and get a list report ready to adjust (see [New report wizard](#new-report-wizard)) |
| Structure | Drag objects and bands to change their order among their siblings; double-click or F2 renames (see [Structure panel](#structure-panel)) |
| Page | Paper size, orientation, margins, continuous page, watermark, title, locale |
| Bands | Add Header, Title, Column header, Detail, Column footer, Summary and Footer bands; resize them by dragging the bottom edge; zebra rows, columns, page breaks... in the properties |
| Groups | *Report > Add group*: choose the field; the detail bands move into the group, which gets a header with the field and a summary band |
| Sub-reports | *Report > Add sub-report*: choose the data source, then set the master/detail fields |
| Objects | Toolbox: text, field, memo, total, page number, image, rectangle, line, barcode, QR Code, chart, panel. Click the tool, then click inside a band |
| Fields | Drag fields from the *Data* panel into a band (or double-click them with a band selected) |
| Editing | Move and resize with the mouse (1 mm grid snapping, *View > Snap to grid*), arrow keys (Shift = 5 mm, Alt = 0.1 mm), copy/cut/paste, duplicate (Ctrl+D), delete, change order, align left/right/top/bottom, center, same width/height |
| Properties | Every property of the selected objects, with editors for colors, fonts, borders, images, choices of fields and data sources, formulas (*Print if*, *Highlight if*, *Formula*) |
| Rulers and guides | Rulers in millimeters on the top and left (*View > Rulers*). Drag from a ruler to create a guide; drag a guide to move it, or out of the view to remove it (*View > Clear guides* removes all). Guides are saved with the report and never printed. The vertical ruler measures the layout on the screen, which includes the band captions |
| Alignment | Moved and resized objects snap to the edges and centers of other objects, the band edges and the guides, within 5 pixels, and a line shows what they aligned with. Columns align across bands; vertical positions only within the same band. A selection snaps as a whole. Turn it off in *View > Snap to objects and guides* |
| Formulas | The `…` button next to a formula opens the expression builder: double-click fields, parameters, variables and functions to insert them, or use the operator buttons. A syntax error blocks OK; a name that is not found is only a warning, because the application may provide it when the report runs |
| Data | SQL queries stored in the report (*Data > Add SQL query*), with parameters taken from report variables (`:name`), and a *Test* button |
| Parameters | Values asked for before the report runs (*Data > Report parameters*), dragged into bands from the data panel |
| Protection | Objects locked by the developer show a padlock and cannot be changed (see [Protected objects](#protected-objects)) |
| Undo | Unlimited undo/redo (Ctrl+Z / Ctrl+Y) |
| Preview | F5 shows the report with real data |
| Zoom | Ctrl + mouse wheel, Ctrl+0 fits the page to the window |

## New report wizard

*File > New from wizard* builds a list report in three steps:

1. **Report and data**: the title, and a data source of the application or a SQL query.
2. **Fields**: which fields to print and in what order, their captions and their totals (sum, count,
   average, minimum or maximum). Text and dates only offer a count.
3. **Layout**: group by a field, landscape page, alternate row colors.

The report gets a page header with the title and the date, column headers, the detail, the totals
and the page numbers. With a group, each group has a header and its totals, followed by the grand
totals of the report. The group field leaves the columns, because it is printed above each group,
and the data must be sorted by it (`ORDER BY` in a SQL query).

The first 50 records decide how each field is shown: numbers on the right, two decimals when they
have decimals, and wider columns for longer values. The columns fill the page, and landscape is
suggested when they do not fit in portrait. A query without records still lists its fields, but
without values they are aligned to the left and have no number format: adjust them in the designer
if needed.

The result replaces the current report, after asking to save it, and is an ordinary report: edit it
as any other. A query that uses `:name` works too, but the parameter must be declared afterwards in
*Data > Report parameters*. Canceling the wizard leaves the current report as it was.

## Structure panel

The *Structure* panel lists the bands and objects of the report as they are stored.

**Drag** one or more rows to another place among their siblings. A line shows where they go, and
*Undo* restores the order. The order of objects in a band is the order they are drawn, so the last
one stays on top. Rows only move within the same parent: to move an object to another band, use cut
and paste.

Bands print by type, so a detail cannot go above a header: a drop only works when it changes what is
printed, such as swapping two detail bands or the order of a detail and a sub-report.

**Double-click** a row or press **F2** to rename the object. Objects with *Read-only properties*, or
with the name hidden, cannot be renamed; objects with *Lock position and size* cannot be dragged.

## Language

The designer is translated into Brazilian Portuguese. Call `BrTranslations::install()` after creating
the `QApplication` to use the system language (`brdesigner` does it automatically).

## Embedding the designer

```cpp
#include <brdesignerwidget.h>

auto *designer = new BrDesignerWidget(parentWindow);

// Data the reports can use: data sources and/or open Qt SQL connections.
designer->addDataSource("customers", customersDataSource);
QSqlDatabase::database();   // the application's default connection, used by SQL queries in reports

designer->openFile("reports/customers.brt");
designer->show();
```

Reports stored in a database instead of files:

```cpp
designer->loadFromJson(loadReportFromDatabase(id));
designer->setSaveHandler([id](const QByteArray &json) {
    return saveReportToDatabase(id, json);     // return false to keep the report marked as modified
});
```

With a save handler, *Save* calls the handler, and *Open* and *Save as* are hidden. The
`modifiedChanged()`, `saved()` and `fileNameChanged()` signals let the application follow the
state, `maybeSave()` asks the user to save pending changes, and `fileMenu()` / `dataMenu()` accept
extra actions.

Printing a designed report in the application:

```cpp
BrReport report;
report.addDataSource("customers", customersDataSource);
report.loadFromJson(loadReportFromDatabase(id));   // or report.load("customers.brt")
report.setVariable("city", selectedCity);          // values used by the report's SQL queries
BrPrinting::preview(&report);                       // asks for the parameters declared in the report
```

## Protected objects

The developer can lock parts of a report, so that end users change only the rest: a tax layout whose
fields must stay in place, the logo and the header of the company, the SQL of a report.

Protection is edited in **developer mode**, and obeyed everywhere else:

```cpp
designer->setDeveloperMode(true);   // off by default: an embedded designer obeys the protection
```

```sh
brdesigner --developer report.brt
```

In developer mode, the *Properties* panel has a *Protection for end users* section for the selected
objects, and *Report > Protect selection* / *Remove protection* apply to the selection and everything
inside it. The padlock in the page and in the *Structure* panel shows what is protected. To see the
report as a user does, open it without developer mode.

| Protection | What the end user cannot do |
|---|---|
| Lock position and size | Move or resize with the mouse, the arrow keys, the alignment commands or the panel; change the order; for bands, drag the bottom edge or change the type |
| Cannot be deleted | Delete or cut the object, or a band, panel, group or sub-report that contains it |
| Read-only properties | Change any property, including the name. On the report, this also blocks queries and parameters in the *Data* menu |
| Lock contents | Add objects to a band or panel, add bands, groups or sub-reports to a report, group or sub-report, or remove anything from them. An object with this lock cannot be deleted either |
| Hidden properties | See the chosen properties. On the report, the list includes the SQL queries and the parameters |

What to keep in mind:

- *Cannot be deleted* does not stop a user from unchecking *Visible* or filling in *Print if*. To
  really keep an object on the page, also use *Read-only properties*, or hide those two.
- Locks are not inherited: each object has its own. *Protect selection* locks an object and its
  contents at once, which is what a fixed band usually needs.
- When several objects are selected, what is locked or hidden in one is locked or hidden for all.
- A copy pasted by an end user has no protection, so that the user can change and delete it. A copy
  made in developer mode keeps it.
- Protection is a restriction of the designer, **not a security feature**: printing ignores it, and a
  report file is plain text that can be edited outside the designer. Hidden properties can still be
  read in the file and in the clipboard. Keep what must really be secret out of the report, and
  store reports where users cannot change them if that matters.

In code:

```cpp
header->setProtection(BrProtection::all());                                  // everything locked
total->setProtection(BrProtection{true, true, false, false, {"dataFormula"}}); // move, delete, hidden
```

## Data for end users

End users usually cannot write code, so a report file can carry its own SQL queries:

```json
"queries": [
    { "name": "customers", "connection": "erp",
      "sql": "SELECT * FROM customer WHERE city = :city ORDER BY name" }
]
```

- `connection` is the name of a Qt SQL connection opened by the application (empty = default
  connection). **Passwords are never stored in reports**; the application owns the connections.
- `:city` is bound to the report variable `city`, which the application sets before printing
  (`report.setVariable("city", ...)`) or the user sets in the report properties.
- A data source registered by the application with the same name takes precedence over the query.

### Report parameters

Parameters let the user choose what the report shows, such as a period or a status, without editing
it. Declare them in *Data > Report parameters*:

```json
"parameters": [
    { "name": "start", "caption": "From", "type": "date", "default": "today" },
    { "name": "status", "caption": "Status", "type": "integer", "default": 1,
      "choices": [ { "value": 1, "caption": "Active" }, { "value": 2, "caption": "Inactive" } ] }
]
```

- `type` is `string` (default), `integer`, `number`, `date` or `boolean`. Integers shown in the
  dialog are limited to 32 bits.
- `default` is used until a variable with the same name is set. Dates are `yyyy-MM-dd` or `today`;
  a date without a default is today.
- `choices` is a fixed list, each item a plain value or `{ "value", "caption" }`. In the designer it
  is typed as `1=Active; 2=Inactive`. An empty value (`=All`) allows queries such as
  `WHERE (:city = '' OR city = :city)`.
- `choicesSql` fills the list from a query (*Choices from SQL* in the designer), and
  `choicesConnection` names its connection (empty = default connection). The first column is the
  value and the second, optional, is the caption:
  `SELECT id, name FROM customer ORDER BY name`. It replaces `choices`. Placeholders use the values
  known when the dialog opens, so one list does not change as another parameter is edited. If the
  query fails, the dialog shows the error and lets the user type the value.
- Queries use `:name` and formulas use `name`. A data field with the same name takes precedence in
  formulas, so avoid names that match fields.

`BrPrinting::preview()` and `print()` show the dialog before preparing the report, filled with the
variable that the application set or else the default. Canceling stops the print. A report already
prepared is not asked again; call `report.clearDocument()` first to ask again. Without a window
(`exportToPdf()`, `prepare()`), the variables and defaults are used directly, and
`BrParametersDialog` can be used to build another flow. Values typed in the designer preview are
kept only for the next preview and are not saved in the report.

The standalone `brdesigner` has *Data > Database connection* to open a connection for designing and
previewing (the password is asked for, never saved).
