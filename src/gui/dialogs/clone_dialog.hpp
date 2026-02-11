#pragma once

#include <QDialog>
#include <QString>
#include <memory>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QProgressBar;
class QDialogButtonBox;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Dialog for cloning disk/partition
class CloneDialog : public QDialog {
    Q_OBJECT

public:
    enum class CloneMode {
        DiskToDisk,
        PartitionToPartition,
        PartitionToFile,
        FileToPartition
    };

    enum class VerificationMode {
        None,
        Quick,
        Full
    };

    struct Options {
        CloneMode mode = CloneMode::DiskToDisk;
        QString source_path;
        QString target_path;
        bool verify_after = true;
        VerificationMode verification = VerificationMode::Quick;
        bool resize_partitions = true;
    };

    explicit CloneDialog(std::shared_ptr<DiskIO> source_disk,
                         QWidget* parent = nullptr);
    ~CloneDialog();

    Options getOptions() const;

private slots:
    void onModeChanged(int index);
    void onBrowseSource();
    void onBrowseTarget();
    void onVerifyChanged(int state);
    bool validateInput();

private:
    void setupUI();
    void setupConnections();
    
    // UI elements
    QComboBox* mode_combo_;
    QLineEdit* source_edit_;
    QLineEdit* target_edit_;
    QPushButton* source_browse_button_;
    QPushButton* target_browse_button_;
    QCheckBox* verify_checkbox_;
    QComboBox* verify_mode_combo_;
    QCheckBox* resize_checkbox_;
    QLabel* warning_label_;
    QProgressBar* progress_bar_;
    QDialogButtonBox* button_box_;
    
    // Data
    std::shared_ptr<DiskIO> source_disk_;
};

} // namespace opm::gui
