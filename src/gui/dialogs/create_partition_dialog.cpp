#include "create_partition_dialog.hpp"
#include <QPushButton>
#include "opm/disk_io.hpp"
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QDoubleValidator>

namespace opm::gui {

CreatePartitionDialog::CreatePartitionDialog(std::shared_ptr<DiskIO> disk,
                                             std::shared_ptr<PartitionTable> table,
                                             QWidget* parent)
    : QDialog(parent)
    , disk_(disk)
    , table_(table)
    , available_space_(0) {
    setWindowTitle("Create Partition");
    setMinimumWidth(500);
    setupUI();
    setupConnections();
    populatePartitionTypes();
    
    available_space_ = getAvailableSpace();
    available_label_->setText(QString("Available space: %1")
        .arg(QString::fromStdString(utils::formatBytes(available_space_))));
    
    validateInput();
}

CreatePartitionDialog::~CreatePartitionDialog() = default;

void CreatePartitionDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
    // Info group
    auto* info_group = new QGroupBox("Device Information", this);
    auto* info_layout = new QGridLayout(info_group);
    
    info_layout->addWidget(new QLabel("Device:", this), 0, 0);
    info_layout->addWidget(new QLabel(QString::fromStdString(disk_->devicePath()), this), 0, 1);
    
    available_label_ = new QLabel("Available space: ", this);
    info_layout->addWidget(available_label_, 1, 0, 1, 2);
    
    main_layout->addWidget(info_group);
    
    // Partition settings group
    auto* settings_group = new QGroupBox("Partition Settings", this);
    auto* settings_layout = new QGridLayout(settings_group);
    settings_layout->setSpacing(10);
    
    // Partition type
    settings_layout->addWidget(new QLabel("Partition Type:", this), 0, 0);
    type_combo_ = new QComboBox(this);
    settings_layout->addWidget(type_combo_, 0, 1, 1, 2);
    
    // Size
    settings_layout->addWidget(new QLabel("Size:", this), 1, 0);
    auto* size_layout = new QHBoxLayout();
    size_edit_ = new QLineEdit(this);
    size_edit_->setPlaceholderText("100");
    auto* validator = new QDoubleValidator(0.01, 999999.99, 2, this);
    size_edit_->setValidator(validator);
    size_layout->addWidget(size_edit_);
    
    size_unit_combo_ = new QComboBox(this);
    size_unit_combo_->addItem("MB", 1ULL * 1024 * 1024);
    size_unit_combo_->addItem("GB", 1ULL * 1024 * 1024 * 1024);
    size_unit_combo_->addItem("TB", 1ULL * 1024 * 1024 * 1024 * 1024);
    size_unit_combo_->setCurrentIndex(1); // GB
    size_layout->addWidget(size_unit_combo_);
    
    max_size_button_ = new QPushButton("Max", this);
    max_size_button_->setToolTip("Use maximum available space");
    size_layout->addWidget(max_size_button_);
    
    settings_layout->addLayout(size_layout, 1, 1, 1, 2);
    
    // Label
    settings_layout->addWidget(new QLabel("Label:", this), 2, 0);
    label_edit_ = new QLineEdit(this);
    label_edit_->setPlaceholderText("New Volume");
    label_edit_->setMaxLength(11); // FAT32 8.3 limitation for volume label
    settings_layout->addWidget(label_edit_, 2, 1, 1, 2);
    
    // Alignment
    align_checkbox_ = new QCheckBox("Align to 1 MiB boundary (recommended)", this);
    align_checkbox_->setChecked(true);
    settings_layout->addWidget(align_checkbox_, 3, 0, 1, 3);
    
    main_layout->addWidget(settings_group);
    
    // Button box
    button_box_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    button_box_->button(QDialogButtonBox::Ok)->setText("Create");
    main_layout->addWidget(button_box_);
}

void CreatePartitionDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::accepted, this, [this]() {
        if (validateInput()) {
            accept();
        }
    });
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(size_edit_, &QLineEdit::textChanged, this, &CreatePartitionDialog::onSizeChanged);
    connect(max_size_button_, &QPushButton::clicked, this, &CreatePartitionDialog::onUseMaximumSize);
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CreatePartitionDialog::onPartitionTypeChanged);
}

void CreatePartitionDialog::populatePartitionTypes() {
    type_combo_->addItem("Linux (0x83)", 0x83);
    type_combo_->addItem("Linux Swap (0x82)", 0x82);
    type_combo_->addItem("Linux LVM (0x8E)", 0x8E);
    type_combo_->addItem("Linux RAID (0xFD)", 0xFD);
    type_combo_->addItem("FAT32 (0x0C)", 0x0C);
    type_combo_->addItem("NTFS (0x07)", 0x07);
    type_combo_->addItem("EFI System (0xEF)", 0xEF);
    type_combo_->addItem("Extended (0x0F)", 0x0F);
}

void CreatePartitionDialog::onSizeChanged() {
    validateInput();
}

void CreatePartitionDialog::onUseMaximumSize() {
    double available_gb = static_cast<double>(available_space_) / (1024 * 1024 * 1024);
    size_edit_->setText(QString::number(available_gb, 'f', 2));
    size_unit_combo_->setCurrentIndex(1); // GB
}

void CreatePartitionDialog::onPartitionTypeChanged(int index) {
    (void)index;
    validateInput();
}

bool CreatePartitionDialog::validateInput() {
    bool valid = true;
    QString error_message;
    
    // Validate size
    if (size_edit_->text().isEmpty()) {
        valid = false;
        error_message = "Please enter a size.";
    } else {
        double size_val = size_edit_->text().toDouble();
        if (size_val <= 0) {
            valid = false;
            error_message = "Size must be greater than 0.";
        } else {
            uint64_t multiplier = size_unit_combo_->currentData().toULongLong();
            uint64_t size_bytes = static_cast<uint64_t>(size_val * multiplier);
            if (size_bytes > available_space_) {
                valid = false;
                error_message = "Size exceeds available space.";
            }
        }
    }
    
    button_box_->button(QDialogButtonBox::Ok)->setEnabled(valid);
    
    if (!valid && !error_message.isEmpty()) {
        // Could show tooltip or status message
    }
    
    return valid;
}

CreatePartitionDialog::Options CreatePartitionDialog::getOptions() const {
    Options options;
    
    // Get size
    double size_val = size_edit_->text().toDouble();
    uint64_t multiplier = size_unit_combo_->currentData().toULongLong();
    options.size_bytes = static_cast<uint64_t>(size_val * multiplier);
    
    // Get partition type
    options.partition_type = type_combo_->currentData().toUInt();
    
    // Get label
    options.label = label_edit_->text();
    
    // Get alignment
    options.align_to_1mb = align_checkbox_->isChecked();
    
    return options;
}

uint64_t CreatePartitionDialog::getAvailableSpace() const {
    if (!disk_) return 0;
    
    auto geometry = disk_->geometry();
    uint64_t total_size = geometry.total_sectors * geometry.bytes_per_sector;
    
    // Subtract space used by existing partitions
    if (table_) {
        for (const auto& part : table_->getPartitions()) {
            total_size -= part.sizeBytes();
        }
    }
    
    // Leave some safety margin
    if (total_size > 10ULL * 1024 * 1024 * 1024) { // > 10GB
        total_size -= 10ULL * 1024 * 1024 * 1024; // Leave 10GB safety
    } else if (total_size > 1024 * 1024 * 1024) { // > 1GB
        total_size -= 100ULL * 1024 * 1024; // Leave 100MB
    }
    
    return total_size > 0 ? total_size : 0;
}

} // namespace opm::gui
