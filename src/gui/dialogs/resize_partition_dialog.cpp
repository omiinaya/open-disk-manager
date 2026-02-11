#include "resize_partition_dialog.hpp"
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
#include <QSlider>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QDoubleValidator>

namespace opm::gui {

ResizePartitionDialog::ResizePartitionDialog(std::shared_ptr<DiskIO> disk,
                                               std::shared_ptr<PartitionTable> table,
                                               int partition_number,
                                               QWidget* parent)
    : QDialog(parent)
    , disk_(disk)
    , table_(table)
    , partition_number_(partition_number)
    , current_size_(0)
    , min_size_(100ULL * 1024 * 1024) // 100MB minimum
    , max_size_(0) {
    setWindowTitle("Resize/Move Partition");
    setMinimumWidth(500);
    setupUI();
    setupConnections();
    loadPartitionInfo();
    validateInput();
}

ResizePartitionDialog::~ResizePartitionDialog() = default;

void ResizePartitionDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
    // Current info group
    auto* info_group = new QGroupBox("Current Partition", this);
    auto* info_layout = new QGridLayout(info_group);
    
    info_layout->addWidget(new QLabel("Device:", this), 0, 0);
    info_layout->addWidget(new QLabel(QString::fromStdString(disk_->devicePath()), this), 0, 1);
    
    info_layout->addWidget(new QLabel("Partition:", this), 1, 0);
    info_layout->addWidget(new QLabel(QString("Partition %1").arg(partition_number_), this), 1, 1);
    
    current_size_label_ = new QLabel("Current Size: ", this);
    info_layout->addWidget(current_size_label_, 2, 0, 1, 2);
    
    main_layout->addWidget(info_group);
    
    // New size group
    auto* size_group = new QGroupBox("New Size", this);
    auto* size_layout = new QGridLayout(size_group);
    size_layout->setSpacing(10);
    
    // Size slider
    size_slider_ = new QSlider(Qt::Horizontal, this);
    size_slider_->setRange(0, 100);
    size_layout->addWidget(size_slider_, 0, 0, 1, 3);
    
    // Min/Max labels
    auto* minmax_layout = new QHBoxLayout();
    min_size_label_ = new QLabel("Min: 100 MB", this);
    max_size_label_ = new QLabel("Max: --", this);
    minmax_layout->addWidget(min_size_label_);
    minmax_layout->addStretch();
    minmax_layout->addWidget(max_size_label_);
    size_layout->addLayout(minmax_layout, 1, 0, 1, 3);
    
    // Size input
    size_layout->addWidget(new QLabel("New Size:", this), 2, 0);
    auto* size_input_layout = new QHBoxLayout();
    new_size_edit_ = new QLineEdit(this);
    new_size_edit_->setPlaceholderText("100");
    auto* validator = new QDoubleValidator(0.01, 999999.99, 2, this);
    new_size_edit_->setValidator(validator);
    size_input_layout->addWidget(new_size_edit_);
    
    size_unit_combo_ = new QComboBox(this);
    size_unit_combo_->addItem("MB", 1ULL * 1024 * 1024);
    size_unit_combo_->addItem("GB", 1ULL * 1024 * 1024 * 1024);
    size_unit_combo_->addItem("TB", 1ULL * 1024 * 1024 * 1024 * 1024);
    size_unit_combo_->setCurrentIndex(1); // GB
    size_input_layout->addWidget(size_unit_combo_);
    
    size_layout->addLayout(size_input_layout, 2, 1, 1, 2);
    
    main_layout->addWidget(size_group);
    
    // Alignment
    align_checkbox_ = new QCheckBox("Align to 1 MiB boundary", this);
    align_checkbox_->setChecked(true);
    main_layout->addWidget(align_checkbox_);
    
    main_layout->addStretch();
    
    // Button box
    button_box_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    button_box_->button(QDialogButtonBox::Ok)->setText("Resize");
    main_layout->addWidget(button_box_);
}

void ResizePartitionDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::accepted, this, [this]() {
        if (validateInput()) {
            accept();
        }
    });
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(new_size_edit_, &QLineEdit::textChanged, this, &ResizePartitionDialog::onSizeChanged);
    connect(size_slider_, &QSlider::valueChanged, this, &ResizePartitionDialog::onSliderChanged);
    connect(size_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { validateInput(); });
}

void ResizePartitionDialog::loadPartitionInfo() {
    if (!table_ || partition_number_ < 0) return;
    
    auto partitions = table_->getPartitions();
    if (partition_number_ >= static_cast<int>(partitions.size())) return;
    
    const auto& partition = partitions[partition_number_];
    current_size_ = partition.sizeBytes();
    
    current_size_label_->setText(
        QString("Current Size: %1")
        .arg(QString::fromStdString(utils::formatBytes(current_size_))));
    
    // Calculate max size (current + available space)
    max_size_ = current_size_ + 10ULL * 1024 * 1024 * 1024; // +10GB for demo
    
    // Update slider and labels
    size_slider_->setRange(static_cast<int>(min_size_ / (1024 * 1024)), 
                           static_cast<int>(max_size_ / (1024 * 1024)));
    size_slider_->setValue(static_cast<int>(current_size_ / (1024 * 1024)));
    
    min_size_label_->setText(
        QString("Min: %1").arg(QString::fromStdString(utils::formatBytes(min_size_))));
    max_size_label_->setText(
        QString("Max: %1").arg(QString::fromStdString(utils::formatBytes(max_size_))));
    
    // Set current size in edit
    double current_gb = static_cast<double>(current_size_) / (1024 * 1024 * 1024);
    new_size_edit_->setText(QString::number(current_gb, 'f', 2));
}

void ResizePartitionDialog::onSizeChanged() {
    double size_val = new_size_edit_->text().toDouble();
    uint64_t multiplier = size_unit_combo_->currentData().toULongLong();
    uint64_t size_bytes = static_cast<uint64_t>(size_val * multiplier);
    
    // Update slider
    int slider_val = static_cast<int>(size_bytes / (1024 * 1024));
    size_slider_->setValue(slider_val);
    
    validateInput();
}

void ResizePartitionDialog::onSliderChanged(int value) {
    uint64_t size_bytes = static_cast<uint64_t>(value) * 1024 * 1024;
    
    // Update edit based on current unit
    int unit_idx = size_unit_combo_->currentIndex();
    double display_val;
    switch (unit_idx) {
        case 0: display_val = static_cast<double>(size_bytes) / (1024 * 1024); break;
        case 1: display_val = static_cast<double>(size_bytes) / (1024 * 1024 * 1024); break;
        case 2: display_val = static_cast<double>(size_bytes) / (1024ULL * 1024 * 1024 * 1024); break;
        default: display_val = static_cast<double>(size_bytes) / (1024 * 1024 * 1024);
    }
    
    new_size_edit_->setText(QString::number(display_val, 'f', 2));
    validateInput();
}

bool ResizePartitionDialog::validateInput() {
    bool valid = true;
    
    if (new_size_edit_->text().isEmpty()) {
        valid = false;
    } else {
        double size_val = new_size_edit_->text().toDouble();
        if (size_val <= 0) {
            valid = false;
        } else {
            uint64_t multiplier = size_unit_combo_->currentData().toULongLong();
            uint64_t size_bytes = static_cast<uint64_t>(size_val * multiplier);
            
            if (size_bytes < min_size_ || size_bytes > max_size_) {
                valid = false;
            }
        }
    }
    
    button_box_->button(QDialogButtonBox::Ok)->setEnabled(valid);
    return valid;
}

ResizePartitionDialog::Options ResizePartitionDialog::getOptions() const {
    Options options;
    
    double size_val = new_size_edit_->text().toDouble();
    uint64_t multiplier = size_unit_combo_->currentData().toULongLong();
    options.new_size_bytes = static_cast<uint64_t>(size_val * multiplier);
    options.align_to_1mb = align_checkbox_->isChecked();
    
    return options;
}

} // namespace opm::gui
