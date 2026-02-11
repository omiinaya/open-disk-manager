#pragma once

#include "opm/types.hpp"
#include <QDialog>
#include <QString>
#include <memory>

class QLabel;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QDialogButtonBox;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Dialog for formatting a partition
class FormatDialog : public QDialog {
    Q_OBJECT

public:
    struct Options {
        FileSystemType fs_type = FileSystemType::Unknown;
        QString label;
        bool quick_format = true;
        bool check_after = false;
        uint32_t cluster_size = 0; // 0 = auto
    };

    explicit FormatDialog(std::shared_ptr<DiskIO> disk,
                          int partition_number,
                          QWidget* parent = nullptr);
    ~FormatDialog();

    Options getOptions() const;

private slots:
    void onFileSystemChanged(int index);
    void onQuickFormatChanged(int state);
    void onClusterSizeChanged(int index);
    bool validateInput();

private:
    void setupUI();
    void setupConnections();
    void populateFileSystems();
    void updateClusterSizes();
    
    // UI elements
    QLabel* partition_info_label_;
    QComboBox* fs_combo_;
    QLineEdit* label_edit_;
    QCheckBox* quick_format_checkbox_;
    QCheckBox* check_after_checkbox_;
    QComboBox* cluster_combo_;
    QLabel* warning_label_;
    QDialogButtonBox* button_box_;
    
    // Data
    std::shared_ptr<DiskIO> disk_;
    int partition_number_;
};

} // namespace opm::gui
