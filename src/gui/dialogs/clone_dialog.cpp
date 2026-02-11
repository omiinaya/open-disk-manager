#include "clone_dialog.hpp"
#include <QPushButton>
#include "opm/disk_io.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFileDialog>
#include <QStyle>
#include <QApplication>

namespace opm::gui {

CloneDialog::CloneDialog(std::shared_ptr<DiskIO> source_disk,
                         QWidget* parent)
    : QDialog(parent)
    , source_disk_(source_disk) {
    setWindowTitle("Clone Disk/Partition");
    setMinimumWidth(550);
    setupUI();
    setupConnections();
    validateInput();
}

CloneDialog::~CloneDialog() = default;

void CloneDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
    // Warning
    warning_label_ = new QLabel(this);
    warning_label_->setText(
        QString("<b>Warning:</b> All data on the target will be overwritten."));
    warning_label_->setStyleSheet("color: #CC0000; padding: 10px;");
    main_layout->addWidget(warning_label_);
    
    // Clone mode
    auto* mode_layout = new QHBoxLayout();
    mode_layout->addWidget(new QLabel("Clone Mode:", this));
    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("Disk to Disk", static_cast<int>(CloneMode::DiskToDisk));
    mode_combo_->addItem("Partition to Partition", static_cast<int>(CloneMode::PartitionToPartition));
    mode_combo_->addItem("Partition to Image File", static_cast<int>(CloneMode::PartitionToFile));
    mode_combo_->addItem("Image File to Partition", static_cast<int>(CloneMode::FileToPartition));
    mode_layout->addWidget(mode_combo_);
    mode_layout->addStretch();
    main_layout->addLayout(mode_layout);
    
    // Source group
    auto* source_group = new QGroupBox("Source", this);
    auto* source_layout = new QGridLayout(source_group);
    source_layout->addWidget(new QLabel("Source:", this), 0, 0);
    
    auto* source_input_layout = new QHBoxLayout();
    source_edit_ = new QLineEdit(this);
    if (source_disk_) {
        source_edit_->setText(QString::fromStdString(source_disk_->devicePath()));
    }
    source_input_layout->addWidget(source_edit_);
    
    source_browse_button_ = new QPushButton("Browse...", this);
    source_input_layout->addWidget(source_browse_button_);
    source_layout->addLayout(source_input_layout, 0, 1);
    main_layout->addWidget(source_group);
    
    // Target group
    auto* target_group = new QGroupBox("Target", this);
    auto* target_layout = new QGridLayout(target_group);
    target_layout->addWidget(new QLabel("Target:", this), 0, 0);
    
    auto* target_input_layout = new QHBoxLayout();
    target_edit_ = new QLineEdit(this);
    target_input_layout->addWidget(target_edit_);
    
    target_browse_button_ = new QPushButton("Browse...", this);
    target_input_layout->addWidget(target_browse_button_);
    target_layout->addLayout(target_input_layout, 0, 1);
    main_layout->addWidget(target_group);
    
    // Options group
    auto* options_group = new QGroupBox("Options", this);
    auto* options_layout = new QVBoxLayout(options_group);
    
    verify_checkbox_ = new QCheckBox("Verify after clone", this);
    verify_checkbox_->setChecked(true);
    options_layout->addWidget(verify_checkbox_);
    
    auto* verify_mode_layout = new QHBoxLayout();
    verify_mode_layout->addWidget(new QLabel("Verification Mode:", this));
    verify_mode_combo_ = new QComboBox(this);
    verify_mode_combo_->addItem("Quick (checksum sample)", static_cast<int>(VerificationMode::Quick));
    verify_mode_combo_->addItem("Full (every sector)", static_cast<int>(VerificationMode::Full));
    verify_mode_combo_->addItem("None", static_cast<int>(VerificationMode::None));
    verify_mode_layout->addWidget(verify_mode_combo_);
    verify_mode_layout->addStretch();
    options_layout->addLayout(verify_mode_layout);
    
    resize_checkbox_ = new QCheckBox("Resize partitions to fit target (if smaller)", this);
    resize_checkbox_->setChecked(true);
    options_layout->addWidget(resize_checkbox_);
    
    main_layout->addWidget(options_group);
    
    // Progress (hidden initially)
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setVisible(false);
    main_layout->addWidget(progress_bar_);
    
    main_layout->addStretch();
    
    // Button box
    button_box_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    button_box_->button(QDialogButtonBox::Ok)->setText("Clone");
    button_box_->button(QDialogButtonBox::Ok)->setStyleSheet(
        "QPushButton { color: white; background-color: #0066CC; }");
    main_layout->addWidget(button_box_);
}

void CloneDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::accepted, this, [this]() {
        if (validateInput()) {
            accept();
        }
    });
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CloneDialog::onModeChanged);
    connect(source_browse_button_, &QPushButton::clicked, this, &CloneDialog::onBrowseSource);
    connect(target_browse_button_, &QPushButton::clicked, this, &CloneDialog::onBrowseTarget);
    connect(verify_checkbox_, &QCheckBox::stateChanged, this, &CloneDialog::onVerifyChanged);
}

void CloneDialog::onModeChanged(int index) {
    (void)index;
    validateInput();
}

void CloneDialog::onBrowseSource() {
    auto mode = static_cast<CloneMode>(mode_combo_->currentData().toInt());
    
    QString file_path;
    if (mode == CloneMode::FileToPartition) {
        file_path = QFileDialog::getOpenFileName(this, "Select Image File", "",
                                                  "Image Files (*.img *.bin);;All Files (*)");
    } else {
        file_path = QFileDialog::getExistingDirectory(this, "Select Source Device", "/dev");
    }
    
    if (!file_path.isEmpty()) {
        source_edit_->setText(file_path);
    }
}

void CloneDialog::onBrowseTarget() {
    auto mode = static_cast<CloneMode>(mode_combo_->currentData().toInt());
    
    QString file_path;
    if (mode == CloneMode::PartitionToFile) {
        file_path = QFileDialog::getSaveFileName(this, "Save Image File", "",
                                                  "Image Files (*.img);;All Files (*)");
    } else {
        file_path = QFileDialog::getExistingDirectory(this, "Select Target Device", "/dev");
    }
    
    if (!file_path.isEmpty()) {
        target_edit_->setText(file_path);
    }
}

void CloneDialog::onVerifyChanged(int state) {
    verify_mode_combo_->setEnabled(state == Qt::Checked);
}

bool CloneDialog::validateInput() {
    bool valid = true;
    
    if (source_edit_->text().isEmpty()) {
        valid = false;
    }
    
    if (target_edit_->text().isEmpty()) {
        valid = false;
    }
    
    if (source_edit_->text() == target_edit_->text()) {
        valid = false;
    }
    
    button_box_->button(QDialogButtonBox::Ok)->setEnabled(valid);
    return valid;
}

CloneDialog::Options CloneDialog::getOptions() const {
    Options options;
    
    options.mode = static_cast<CloneMode>(mode_combo_->currentData().toInt());
    options.source_path = source_edit_->text();
    options.target_path = target_edit_->text();
    options.verify_after = verify_checkbox_->isChecked();
    options.verification = static_cast<VerificationMode>(verify_mode_combo_->currentData().toInt());
    options.resize_partitions = resize_checkbox_->isChecked();
    
    return options;
}

} // namespace opm::gui
