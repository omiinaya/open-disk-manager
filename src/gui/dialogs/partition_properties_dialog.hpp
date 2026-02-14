#pragma once

#include <QDialog>
#include <memory>

namespace opm {
class Partition;
enum class PartitionType;
}

namespace opm::gui {

// Forward declarations
class QLabel;
class QPushButton;
class QDialogButtonBox;
class QTextEdit;
class QLineEdit;
class QComboBox;

// Dialog for viewing and editing partition properties
class PartitionPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PartitionPropertiesDialog(std::shared_ptr<opm::Partition> partition, QWidget* parent = nullptr);
    ~PartitionPropertiesDialog() override = default;

signals:
    void partitionModified();

private slots:
    void onNameChanged(const QString& text);
    void onTypeChanged(int index);
    void onApplyClicked();
    void onCloseClicked();

private:
    void setupUI();
    void loadPartitionInfo();
    void updateUI();
    bool validateInput();

    // UI elements
    QLabel* partition_number_label_;
    QLabel* start_sector_label_;
    QLabel* end_sector_label_;
    QLabel* size_label_;
    QLineEdit* name_edit_;
    QComboBox* type_combo_;
    QTextEdit* details_text_;
    QPushButton* apply_button_;
    QDialogButtonBox* button_box_;

    // Data
    std::shared_ptr<opm::Partition> partition_;
    opm::PartitionType current_type_;
    uint64_t original_start_;
    uint64_t original_size_;
    QString original_name_;
};

} // namespace opm::gui
