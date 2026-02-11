#pragma once

#include <QDialog>
#include <QString>
#include <memory>

class QLabel;
class QCheckBox;
class QPushButton;
class QDialogButtonBox;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Dialog for deleting a partition with confirmation
class DeletePartitionDialog : public QDialog {
    Q_OBJECT

public:
    explicit DeletePartitionDialog(std::shared_ptr<DiskIO> disk,
                                    int partition_number,
                                    uint64_t partition_size,
                                    const QString& partition_type,
                                    QWidget* parent = nullptr);
    ~DeletePartitionDialog();

    bool confirmed() const { return confirmed_; }
    bool eraseData() const { return erase_data_; }

private slots:
    void onConfirmChanged(int state);
    void onEraseChanged(int state);

private:
    void setupUI();
    void setupConnections();

    // UI elements
    QLabel* warning_icon_;
    QLabel* message_label_;
    QCheckBox* confirm_checkbox_;
    QCheckBox* erase_checkbox_;
    QDialogButtonBox* button_box_;

    // Data
    bool confirmed_ = false;
    bool erase_data_ = false;
    int partition_number_;
};

} // namespace opm::gui
