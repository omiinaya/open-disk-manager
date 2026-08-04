#include "dialogs/schedules_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialogButtonBox>
#include "opm/schedule.hpp"

namespace opm::gui {

SchedulesDialog::SchedulesDialog(QWidget* parent)
    : QDialog(parent)
    , list_(nullptr)
    , hint_(nullptr)
    , add_button_(nullptr)
    , remove_button_(nullptr)
    , refresh_button_(nullptr)
    , close_button_(nullptr) {
    setWindowTitle("Backup Schedules");
    setMinimumSize(560, 360);
    setupUI();
    reloadList();
}

void SchedulesDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);

    hint_ = new QLabel(
        "Schedules are stored in the same registry the CLI uses\n"
        "(~/.config/opm/schedules.conf). Live install uses systemd user timers "
        "or crontab when available.", this);
    hint_->setWordWrap(true);
    main_layout->addWidget(hint_);

    list_ = new QListWidget(this);
    main_layout->addWidget(list_, 1);

    auto* button_layout = new QHBoxLayout();
    add_button_ = new QPushButton("Add...", this);
    remove_button_ = new QPushButton("Remove", this);
    refresh_button_ = new QPushButton("Refresh", this);
    button_layout->addWidget(add_button_);
    button_layout->addWidget(remove_button_);
    button_layout->addWidget(refresh_button_);
    button_layout->addStretch();

    close_button_ = new QPushButton("Close", this);
    button_layout->addWidget(close_button_);
    main_layout->addLayout(button_layout);

    connect(add_button_, &QPushButton::clicked, this, &SchedulesDialog::onAddSchedule);
    connect(remove_button_, &QPushButton::clicked, this, &SchedulesDialog::onRemoveSchedule);
    connect(refresh_button_, &QPushButton::clicked, this, &SchedulesDialog::onRefresh);
    connect(close_button_, &QPushButton::clicked, this, &SchedulesDialog::accept);
    connect(list_, &QListWidget::itemSelectionChanged, this, &SchedulesDialog::onSelectionChanged);
}

QListWidgetItem* SchedulesDialog::selectedItem() const {
    auto items = list_->selectedItems();
    return items.isEmpty() ? nullptr : items.first();
}

void SchedulesDialog::onSelectionChanged() {
    remove_button_->setEnabled(selectedItem() != nullptr);
}

void SchedulesDialog::reloadList() {
    list_->clear();
    entries_.clear();
    Result r = scheduleList(entries_);
    if (r.failed()) {
        hint_->setText(QString("Cannot read schedule registry: %1").arg(QString::fromStdString(r.message)));
        return;
    }
    if (entries_.empty()) {
        list_->addItem("(no schedules — click Add to create one)");
        return;
    }
    for (const auto& e : entries_) {
        list_->addItem(QString::fromStdString(e.name + "  [" + e.describe() + "]"));
    }
}

void SchedulesDialog::onRefresh() {
    reloadList();
}

bool SchedulesDialog::addEntry(const ScheduleEntry& e, QWidget* for_errors) {
    std::string err;
    if (!e.valid(err)) {
        QMessageBox::warning(for_errors, "Invalid schedule",
                             QString::fromStdString("Invalid schedule: " + err));
        return false;
    }
    Result r = scheduleAdd(e);
    if (r.failed()) {
        QMessageBox::warning(for_errors, "Add schedule",
                             QString::fromStdString("Cannot add schedule: " + r.message));
        return false;
    }
    reloadList();
    return true;
}

void SchedulesDialog::onAddSchedule() {
    // Collect name + cron fields via a small form dialog.
    QDialog dlg(this);
    dlg.setWindowTitle("Add Schedule");
    auto* layout = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout();
    auto* name = new QLineEdit(&dlg);
    name->setPlaceholderText("daily-backup");
    auto* minute = new QLineEdit("0", &dlg);
    auto* hour = new QLineEdit("2", &dlg);
    auto* dom = new QLineEdit("*", &dlg);
    auto* month = new QLineEdit("*", &dlg);
    auto* dow = new QLineEdit("*", &dlg);
    auto* command = new QLineEdit("opm backup create /dev/sda ~/backups/full.img --compress", &dlg);
    form->addRow("Name", name);
    form->addRow("Minute (0-59, */N, *)", minute);
    form->addRow("Hour (0-23, */N, *)", hour);
    form->addRow("Day of month (1-31, *)", dom);
    form->addRow("Month (1-12, *)", month);
    form->addRow("Day of week (0-7, *)", dow);
    form->addRow("Command", command);
    layout->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    ScheduleEntry e;
    e.name = name->text().toStdString();
    e.minute = minute->text().toStdString();
    e.hour = hour->text().toStdString();
    e.dom = dom->text().toStdString();
    e.month = month->text().toStdString();
    e.dow = dow->text().toStdString();
    e.command = command->text().toStdString();

    // Show the resulting cron line for confirmation.
    std::string err;
    if (!e.valid(err)) {
        QMessageBox::warning(this, "Invalid schedule",
                             QString::fromStdString("Invalid schedule: " + err));
        return;
    }
    auto confirm = QMessageBox::question(
        this, "Confirm schedule",
        QString("Cron line:\n%1\n\n%2\n\nAdd this schedule?")
            .arg(QString::fromStdString(e.cronLine()))
            .arg(QString::fromStdString(e.describe())));
    if (confirm != QMessageBox::Yes) return;

    addEntry(e, this);
}

void SchedulesDialog::onRemoveSchedule() {
    QListWidgetItem* item = selectedItem();
    if (!item) return;
    int row = list_->row(item);
    if (row < 0 || row >= static_cast<int>(entries_.size())) return;
    const std::string& name = entries_[static_cast<size_t>(row)].name;
    auto confirm = QMessageBox::question(this, "Remove schedule",
                                         QString("Remove schedule '%1'?").arg(QString::fromStdString(name)));
    if (confirm != QMessageBox::Yes) return;
    Result r = scheduleRemove(name);
    if (r.failed()) {
        QMessageBox::warning(this, "Remove schedule",
                             QString::fromStdString("Cannot remove schedule: " + r.message));
        return;
    }
    reloadList();
}

void SchedulesDialog::onAccepted() {
    accept();
}

} // namespace opm::gui