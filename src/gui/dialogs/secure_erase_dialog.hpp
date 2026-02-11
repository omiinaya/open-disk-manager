#pragma once

#include <QDialog>
#include <memory>

class QLabel;
class QComboBox;
class QProgressBar;
class QCheckBox;
class QPushButton;
class QDialogButtonBox;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Dialog for secure erase operations
class SecureEraseDialog : public QDialog {
    Q_OBJECT

public:
    enum class EraseMethod {
        Zeros,           // Single pass zeros
        Random,          // Random data
        DoD,            // DoD 5220.22-M (3 passes)
        Gutmann,        // Gutmann 35-pass
        NIST_Clear,     // NIST 800-88 Clear
        NIST_Purge      // NIST 800-88 Purge
    };

    struct Options {
        EraseMethod method = EraseMethod::Zeros;
        bool verify_after = true;
        int passes = 1;
        bool skip_bad_sectors = true;
    };

    explicit SecureEraseDialog(std::shared_ptr<DiskIO> disk,
                                int partition_number = -1, // -1 = whole disk
                                QWidget* parent = nullptr);
    ~SecureEraseDialog();

    Options getOptions() const;

private slots:
    void onMethodChanged(int index);
    void onVerifyChanged(int state);
    void onStartClicked();
    bool validateInput();

private:
    void setupUI();
    void setupConnections();
    void updateWarningText();
    
    // UI elements
    QLabel* device_label_;
    QComboBox* method_combo_;
    QLabel* method_description_;
    QProgressBar* progress_bar_;
    QCheckBox* verify_checkbox_;
    QCheckBox* skip_bad_checkbox_;
    QLabel* time_estimate_;
    QLabel* warning_label_;
    QPushButton* start_button_;
    QDialogButtonBox* button_box_;
    
    // Data
    std::shared_ptr<DiskIO> disk_;
    int partition_number_;
};

} // namespace opm::gui
