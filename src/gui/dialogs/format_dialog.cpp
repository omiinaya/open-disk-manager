#include "format_dialog.hpp"
#include <QPushButton>
#include "opm/disk_io.hpp"
#include "opm/types.hpp"
#include "opm/filesystem.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QStyle>
#include <QApplication>

namespace opm::gui {

FormatDialog::FormatDialog(std::shared_ptr<DiskIO> disk,
                            int partition_number,
                            QWidget* parent)
    : QDialog(parent)
    , disk_(disk)
    , partition_number_(partition_number) {
    setWindowTitle("Format Partition");
    setMinimumWidth(500);
    setupUI();
    setupConnections();
    populateFileSystems();
    validateInput();
}

FormatDialog::~FormatDialog() = default;

void FormatDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
    // Warning
    warning_label_ = new QLabel(this);
    QIcon warning_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    warning_label_->setText(QString("<b>Warning:</b> Formatting will erase all data on this partition."));
    warning_label_->setStyleSheet("color: #CC0000; padding: 10px;");
    main_layout->addWidget(warning_label_);
    
    // Partition info
    partition_info_label_ = new QLabel(this);
    partition_info_label_->setText(
        QString("Device: %1<br>Partition: %2")
        .arg(QString::fromStdString(disk_->devicePath()))
        .arg(partition_number_));
    main_layout->addWidget(partition_info_label_);
    
    // Format options group
    auto* format_group = new QGroupBox("Format Options", this);
    auto* format_layout = new QGridLayout(format_group);
    format_layout->setSpacing(10);
    
    // File system
    format_layout->addWidget(new QLabel("File System:", this), 0, 0);
    fs_combo_ = new QComboBox(this);
    format_layout->addWidget(fs_combo_, 0, 1, 1, 2);
    
    // Label
    format_layout->addWidget(new QLabel("Volume Label:", this), 1, 0);
    label_edit_ = new QLineEdit(this);
    label_edit_->setPlaceholderText("New Volume");
    label_edit_->setMaxLength(11);
    format_layout->addWidget(label_edit_, 1, 1, 1, 2);
    
    // Cluster size
    format_layout->addWidget(new QLabel("Cluster Size:", this), 2, 0);
    cluster_combo_ = new QComboBox(this);
    cluster_combo_->addItem("Default / Auto", 0);
    format_layout->addWidget(cluster_combo_, 2, 1, 1, 2);
    
    main_layout->addWidget(format_group);
    
    // Checkboxes
    quick_format_checkbox_ = new QCheckBox("Quick Format (skip bad sector check)", this);
    quick_format_checkbox_->setChecked(true);
    main_layout->addWidget(quick_format_checkbox_);
    
    check_after_checkbox_ = new QCheckBox("Check partition after format", this);
    check_after_checkbox_->setChecked(false);
    main_layout->addWidget(check_after_checkbox_);
    
    main_layout->addStretch();
    
    // Button box
    button_box_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    button_box_->button(QDialogButtonBox::Ok)->setText("Format");
    button_box_->button(QDialogButtonBox::Ok)->setStyleSheet(
        "QPushButton { color: white; background-color: #CC0000; }");
    main_layout->addWidget(button_box_);
}

void FormatDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::accepted, this, [this]() {
        if (validateInput()) {
            accept();
        }
    });
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(fs_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormatDialog::onFileSystemChanged);
    connect(quick_format_checkbox_, &QCheckBox::stateChanged,
            this, &FormatDialog::onQuickFormatChanged);
}

void FormatDialog::populateFileSystems() {
    fs_combo_->addItem("FAT32", static_cast<int>(FileSystemType::FAT32));
    fs_combo_->addItem("NTFS", static_cast<int>(FileSystemType::NTFS));
    fs_combo_->addItem("ext4", static_cast<int>(FileSystemType::EXT4));
    fs_combo_->addItem("exFAT", static_cast<int>(FileSystemType::exFAT));
    
    updateClusterSizes();
}

void FormatDialog::updateClusterSizes() {
    cluster_combo_->clear();
    cluster_combo_->addItem("Default / Auto", 0);
    
    auto fs_type = static_cast<FileSystemType>(fs_combo_->currentData().toInt());
    
    switch (fs_type) {
        case FileSystemType::FAT32:
            cluster_combo_->addItem("512 bytes", 512);
            cluster_combo_->addItem("1 KB", 1024);
            cluster_combo_->addItem("2 KB", 2048);
            cluster_combo_->addItem("4 KB", 4096);
            cluster_combo_->addItem("8 KB", 8192);
            cluster_combo_->addItem("16 KB", 16384);
            cluster_combo_->addItem("32 KB", 32768);
            break;
        case FileSystemType::NTFS:
            cluster_combo_->addItem("512 bytes", 512);
            cluster_combo_->addItem("1 KB", 1024);
            cluster_combo_->addItem("2 KB", 2048);
            cluster_combo_->addItem("4 KB", 4096);
            cluster_combo_->addItem("8 KB", 8192);
            cluster_combo_->addItem("16 KB", 16384);
            cluster_combo_->addItem("32 KB", 32768);
            cluster_combo_->addItem("64 KB", 65536);
            break;
        case FileSystemType::EXT4:
            cluster_combo_->addItem("1 KB", 1024);
            cluster_combo_->addItem("2 KB", 2048);
            cluster_combo_->addItem("4 KB", 4096);
            break;
        case FileSystemType::exFAT:
            cluster_combo_->addItem("512 bytes", 512);
            cluster_combo_->addItem("1 KB", 1024);
            cluster_combo_->addItem("2 KB", 2048);
            cluster_combo_->addItem("4 KB", 4096);
            cluster_combo_->addItem("8 KB", 8192);
            cluster_combo_->addItem("16 KB", 16384);
            cluster_combo_->addItem("32 KB", 32768);
            break;
        default:
            break;
    }
}

void FormatDialog::onFileSystemChanged(int index) {
    (void)index;
    updateClusterSizes();
    validateInput();
}

void FormatDialog::onQuickFormatChanged(int state) {
    (void)state;
    validateInput();
}

void FormatDialog::onClusterSizeChanged(int index) {
    (void)index;
    validateInput();
}

bool FormatDialog::validateInput() {
    bool valid = true;
    
    if (fs_combo_->currentIndex() < 0) {
        valid = false;
    }
    
    button_box_->button(QDialogButtonBox::Ok)->setEnabled(valid);
    return valid;
}

FormatDialog::Options FormatDialog::getOptions() const {
    Options options;
    
    options.fs_type = static_cast<FileSystemType>(fs_combo_->currentData().toInt());
    options.label = label_edit_->text();
    options.quick_format = quick_format_checkbox_->isChecked();
    options.check_after = check_after_checkbox_->isChecked();
    options.cluster_size = cluster_combo_->currentData().toUInt();
    
    return options;
}

} // namespace opm::gui
