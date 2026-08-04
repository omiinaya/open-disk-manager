#pragma once

#include <QDialog>
#include <vector>
#include <string>
#include "opm/schedule.hpp"

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;

namespace opm::gui {

// Dialog to view / add / remove backup schedules (wraps the opm::schedule
// module — the same plain-text registry the CLI uses).
class SchedulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit SchedulesDialog(QWidget* parent = nullptr);
    ~SchedulesDialog() override = default;

private slots:
    void onAddSchedule();
    void onRemoveSchedule();
    void onSelectionChanged();
    void onRefresh();
    void onAccepted();

private:
    void setupUI();
    void reloadList();
    QListWidgetItem* selectedItem() const;
    bool addEntry(const ScheduleEntry& e, QWidget* for_errors);

    // UI
    QListWidget* list_;
    QLabel* hint_;
    QPushButton* add_button_;
    QPushButton* remove_button_;
    QPushButton* refresh_button_;
    QPushButton* close_button_;

    std::vector<opm::ScheduleEntry> entries_;
};

} // namespace opm::gui