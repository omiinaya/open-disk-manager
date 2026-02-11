#pragma once

#include <QDialog>
#include <memory>

class QLabel;
class QLineEdit;
class QComboBox;
class QSlider;
class QCheckBox;
class QPushButton;
class QDialogButtonBox;

namespace opm {
class DiskIO;
class PartitionTable;
}

namespace opm::gui {

// Dialog for resizing/moving a partition
class ResizePartitionDialog : public QDialog {
    Q_OBJECT

public:
    struct Options {
        uint64_t new_size_bytes = 0;
        uint64_t new_start_sector = 0;
        bool align_to_1mb = true;
    };

    explicit ResizePartitionDialog(std::shared_ptr<DiskIO> disk,
                                     std::shared_ptr<PartitionTable> table,
                                     int partition_number,
                                     QWidget* parent = nullptr);
    ~ResizePartitionDialog();

    Options getOptions() const;

private slots:
    void onSizeChanged();
    void onSliderChanged(int value);
    bool validateInput();

private:
    void setupUI();
    void setupConnections();
    void loadPartitionInfo();
    
    // UI elements
    QLabel* current_size_label_;
    QLineEdit* new_size_edit_;
    QComboBox* size_unit_combo_;
    QSlider* size_slider_;
    QLabel* min_size_label_;
    QLabel* max_size_label_;
    QCheckBox* align_checkbox_;
    QDialogButtonBox* button_box_;
    
    // Data
    std::shared_ptr<DiskIO> disk_;
    std::shared_ptr<PartitionTable> table_;
    int partition_number_;
    uint64_t current_size_;
    uint64_t min_size_;
    uint64_t max_size_;
};

} // namespace opm::gui
