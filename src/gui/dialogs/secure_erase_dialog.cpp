#include "secure_erase_dialog.hpp"
#include "opm/disk_io.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QStyle>
#include <QApplication>
#include <QMessageBox>

namespace opm::gui {

SecureEraseDialog::SecureEraseDialog(std::shared_ptr<DiskIO> disk,
                                     int partition_number,
                                     QWidget* parent)
    : QDialog(parent)
    , disk_(disk)
    , partition_number_(partition_number) {
    setWindowTitle("Secure Erase");
    setMinimumWidth(500);
    setupUI();
    setupConnections();
    updateWarningText();
    validateInput();
}

SecureEraseDialog::~SecureEraseDialog() = default;

void SecureEraseDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
    // Warning
    warning_label_ = new QLabel(this);
    warning_label_->setWordWrap(true);
    warning_label_->setStyleSheet("color: #CC0000; padding: 10px; border: 2px solid #CC0000;");
    main_layout->addWidget(warning_label_);
    
    // Device info
    auto* info_group = new QGroupBox("Device Information", this);
    auto* info_layout = new QGridLayout(info_group);
    
    info_layout->addWidget(new QLabel("Device:", this), 0, 0);
    device_label_ = new QLabel(QString::fromStdString(disk_->devicePath()), this);
    info_layout->addWidget(device_label_, 0, 1);
    
    info_layout->addWidget(new QLabel("Target:", this), 1, 0);
    QString target_text = (partition_number_ >= 0) 
        ? QString("Partition %1").arg(partition_number_)
        : QString("Entire Disk");
    info_layout->addWidget(new QLabel(target_text, this), 1, 1);
    
    main_layout->addWidget(info_group);
    
    // Method group
    auto* method_group = new QGroupBox("Erase Method", this);
    auto* method_layout = new QGridLayout(method_group);
    
    method_layout->addWidget(new QLabel("Method:", this), 0, 0);
    method_combo_ = new QComboBox(this);
    method_combo_->addItem("Zeros (1 pass) - Fastest", static_cast<int>(EraseMethod::Zeros));
    method_combo_->addItem("Random Data (1 pass)", static_cast<int>(EraseMethod::Random));
    method_combo_->addItem("DoD 5220.22-M (3 passes)", static_cast<int>(EraseMethod::DoD));
    method_combo_->addItem("NIST 800-88 Clear (1 pass)", static_cast<int>(EraseMethod::NIST_Clear));
    method_combo_->addItem("NIST 800-88 Purge", static_cast<int>(EraseMethod::NIST_Purge));
    method_combo_->addItem("Gutmann (35 passes) - Most Secure", static_cast<int>(EraseMethod::Gutmann));
    method_layout->addWidget(method_combo_, 0, 1, 1, 2);
    
    method_description_ = new QLabel(this);
    method_description_->setWordWrap(true);
    method_description_->setStyleSheet("color: #666666; font-style: italic;");
    method_layout->addWidget(method_description_, 1, 0, 1, 3);
    
    main_layout->addWidget(method_group);
    
    // Options
    auto* options_group = new QGroupBox("Options", this);
    auto* options_layout = new QVBoxLayout(options_group);
    
    verify_checkbox_ = new QCheckBox("Verify erase (slower but more secure)", this);
    verify_checkbox_->setChecked(true);
    options_layout->addWidget(verify_checkbox_);
    
    skip_bad_checkbox_ = new QCheckBox("Skip bad sectors", this);
    skip_bad_checkbox_->setChecked(true);
    options_layout->addWidget(skip_bad_checkbox_);
    
    options_layout->addWidget(new QLabel("Estimated Time: ", this));
    time_estimate_ = new QLabel("Calculating...", this);
    time_estimate_->setStyleSheet("font-weight: bold;");
    options_layout->addWidget(time_estimate_);
    
    main_layout->addWidget(options_group);
    
    // Progress
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setVisible(false);
    main_layout->addWidget(progress_bar_);
    
    main_layout->addStretch();
    
    // Button box
    button_box_ = new QDialogButtonBox(
        QDialogButtonBox::Cancel, this);
    
    start_button_ = new QPushButton("Start Secure Erase", this);
    start_button_->setStyleSheet(
        "QPushButton { color: white; background-color: #CC0000; padding: 10px 20px; font-weight: bold; }");
    button_box_->addButton(start_button_, QDialogButtonBox::AcceptRole);
    
    main_layout->addWidget(button_box_);
}

void SecureEraseDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(start_button_, &QPushButton::clicked, this, &SecureEraseDialog::onStartClicked);
    connect(method_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SecureEraseDialog::onMethodChanged);
    connect(verify_checkbox_, &QCheckBox::stateChanged, this, &SecureEraseDialog::onVerifyChanged);
}

void SecureEraseDialog::updateWarningText() {
    QString target = (partition_number_ >= 0) 
        ? QString("partition %1 on %2").arg(partition_number_).arg(QString::fromStdString(disk_->devicePath()))
        : QString("entire disk %1").arg(QString::fromStdString(disk_->devicePath()));
    
    warning_label_->setText(
        QString("<b>WARNING:</b> This operation will <b>PERMANENTLY ERASE</b> all data on the %1. "
                "This action <b>CANNOT BE UNDONE</b>. All data will be lost forever. "
                "Make sure you have backed up any important data before proceeding.")
            .arg(target));
}

void SecureEraseDialog::onMethodChanged(int index) {
    (void)index;
    
    auto method = static_cast<EraseMethod>(method_combo_->currentData().toInt());
    
    QString description;
    QString time_text;
    
    switch (method) {
        case EraseMethod::Zeros:
            description = "Writes zeros to all sectors. Fast but recoverable with specialized tools.";
            time_text = "~5 minutes for 1TB";
            break;
        case EraseMethod::Random:
            description = "Writes random data to all sectors. Better than zeros but still recoverable.";
            time_text = "~10 minutes for 1TB";
            break;
        case EraseMethod::DoD:
            description = "US DoD 5220.22-M standard: 3 passes (0x00, 0xFF, random). Good security.";
            time_text = "~30 minutes for 1TB";
            break;
        case EraseMethod::NIST_Clear:
            description = "NIST 800-88 Clear: Single overwrite. Suitable for most situations.";
            time_text = "~5 minutes for 1TB";
            break;
        case EraseMethod::NIST_Purge:
            description = "NIST 800-88 Purge: Multiple overwrites. High security for HDDs.";
            time_text = "~15 minutes for 1TB";
            break;
        case EraseMethod::Gutmann:
            description = "Gutmann 35-pass method. Maximum security but extremely slow.";
            time_text = "~6 hours for 1TB";
            break;
    }
    
    method_description_->setText(description);
    time_estimate_->setText(time_text);
    validateInput();
}

void SecureEraseDialog::onVerifyChanged(int state) {
    (void)state;
    // Update time estimate
    onMethodChanged(method_combo_->currentIndex());
}

void SecureEraseDialog::onStartClicked() {
    // Show confirmation dialog
    auto method = static_cast<EraseMethod>(method_combo_->currentData().toInt());
    QString method_name = method_combo_->currentText().split(" - ").first();
    
    QString target = (partition_number_ >= 0) 
        ? QString("partition %1 on %2").arg(partition_number_).arg(QString::fromStdString(disk_->devicePath()))
        : QString("entire disk %1").arg(QString::fromStdString(disk_->devicePath()));
    
    QMessageBox::StandardButton reply = QMessageBox::warning(this, "Confirm Secure Erase",
        QString("Are you absolutely sure you want to securely erase the %1 using '%2'?\n\n"
                "This will PERMANENTLY DESTROY all data and cannot be undone.")
            .arg(target)
            .arg(method_name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        accept();
    }
}

bool SecureEraseDialog::validateInput() {
    bool valid = true;
    // Add validation logic here if needed
    return valid;
}

SecureEraseDialog::Options SecureEraseDialog::getOptions() const {
    Options options;
    
    options.method = static_cast<EraseMethod>(method_combo_->currentData().toInt());
    options.verify_after = verify_checkbox_->isChecked();
    options.skip_bad_sectors = skip_bad_checkbox_->isChecked();
    
    // Set passes based on method
    switch (options.method) {
        case EraseMethod::Zeros:
        case EraseMethod::Random:
        case EraseMethod::NIST_Clear:
            options.passes = 1;
            break;
        case EraseMethod::DoD:
            options.passes = 3;
            break;
        case EraseMethod::NIST_Purge:
            options.passes = 5;
            break;
        case EraseMethod::Gutmann:
            options.passes = 35;
            break;
    }
    
    return options;
}

} // namespace opm::gui
