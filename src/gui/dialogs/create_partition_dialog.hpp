#pragma once

#include <QDialog>
#include <QString>
#include <memory>

class QComboBox;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QDialogButtonBox;

namespace opm {
class DiskIO;
class PartitionTable;
}

namespace opm::gui {

// Dialog for creating a new partition
class CreatePartitionDialog : public QDialog {
    Q_OBJECT

public:
    struct Options {
        uint64_t start_sector = 0;
        uint64_t size_bytes = 0;
        uint8_t partition_type = 0x83; // Linux
        QString label;
        bool align_to_1mb = true;
    };

    explicit CreatePartitionDialog(std::shared_ptr<DiskIO> disk,
                                    std::shared_ptr<PartitionTable> table,
                                    QWidget* parent = nullptr);
    ~CreatePartitionDialog();

    // Get the options set by the user
    Options getOptions() const;

private slots:
    void onSizeChanged();
    void onUseMaximumSize();
    void onPartitionTypeChanged(int index);
    bool validateInput();

private:
    void setupUI();
    void setupConnections();
    void populatePartitionTypes();
    uint64_t getAvailableSpace() const;

    // UI elements
    QComboBox* type_combo_;
    QLineEdit* size_edit_;
    QComboBox* size_unit_combo_;
    QPushButton* max_size_button_;
    QLineEdit* label_edit_;
    QCheckBox* align_checkbox_;
    QLabel* available_label_;
    QDialogButtonBox* button_box_;

    // Data
    std::shared_ptr<DiskIO> disk_;
    std::shared_ptr<PartitionTable> table_;
    uint64_t available_space_;
};

} // namespace opm::gui
