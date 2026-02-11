#include "delete_partition_dialog.hpp"
#include <QPushButton>
#include "opm/disk_io.hpp"
#include "opm/utils.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QStyle>
#include <QApplication>
#include <QFrame>

namespace opm::gui {

DeletePartitionDialog::DeletePartitionDialog(std::shared_ptr<DiskIO> disk,
                                               int partition_number,
                                               uint64_t partition_size,
                                               const QString& partition_type,
                                               QWidget* parent)
    : QDialog(parent)
    , partition_number_(partition_number)
    , confirmed_(false)
    , erase_data_(false) {
    setWindowTitle("Delete Partition");
    setMinimumWidth(450);
    setMinimumHeight(300);
    
    setupUI();
    setupConnections();
    
    // Set warning message
    QString device_path = QString::fromStdString(disk->devicePath());
    message_label_->setText(
        QString("<b>Warning:</b> You are about to delete partition #%1 on %2.<br><br>"
                "<b>Partition Type:</b> %3<br>"
                "<b>Size:</b> %4<br><br>"
                "This action cannot be undone. All data on this partition will be lost.")
            .arg(partition_number)
            .arg(device_path)
            .arg(partition_type)
            .arg(QString::fromStdString(
                utils::formatBytes(partition_size))));
    
    // Disable OK button initially
    button_box_->button(QDialogButtonBox::Ok)->setEnabled(false);
}

DeletePartitionDialog::~DeletePartitionDialog() = default;

void DeletePartitionDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
    // Warning section
    auto* warning_layout = new QHBoxLayout();
    
    // Warning icon (using standard icon)
    warning_icon_ = new QLabel(this);
    QIcon warning_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    warning_icon_->setPixmap(warning_icon.pixmap(48, 48));
    warning_layout->addWidget(warning_icon_);
    
    // Warning message
    message_label_ = new QLabel(this);
    message_label_->setWordWrap(true);
    message_label_->setStyleSheet("color: #CC0000;");
    warning_layout->addWidget(message_label_, 1);
    
    main_layout->addLayout(warning_layout);
    
    // Separator
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    main_layout->addWidget(separator);
    
    // Confirmation checkbox
    confirm_checkbox_ = new QCheckBox(
        "I understand that this will permanently delete the partition and all its data.", this);
    main_layout->addWidget(confirm_checkbox_);
    
    // Secure erase option
    erase_checkbox_ = new QCheckBox(
        "Securely erase data before deleting (slower but more secure)", this);
    erase_checkbox_->setEnabled(false);
    main_layout->addWidget(erase_checkbox_);
    
    main_layout->addStretch();
    
    // Button box
    button_box_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    button_box_->button(QDialogButtonBox::Ok)->setText("Delete");
    button_box_->button(QDialogButtonBox::Ok)->setStyleSheet(
        "QPushButton { color: white; background-color: #CC0000; }");
    main_layout->addWidget(button_box_);
}

void DeletePartitionDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::accepted, this, [this]() {
        if (confirmed_) {
            accept();
        }
    });
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(confirm_checkbox_, &QCheckBox::stateChanged,
            this, &DeletePartitionDialog::onConfirmChanged);
    connect(erase_checkbox_, &QCheckBox::stateChanged,
            this, &DeletePartitionDialog::onEraseChanged);
}

void DeletePartitionDialog::onConfirmChanged(int state) {
    confirmed_ = (state == Qt::Checked);
    button_box_->button(QDialogButtonBox::Ok)->setEnabled(confirmed_);
    erase_checkbox_->setEnabled(confirmed_);
}

void DeletePartitionDialog::onEraseChanged(int state) {
    erase_data_ = (state == Qt::Checked);
}

} // namespace opm::gui
