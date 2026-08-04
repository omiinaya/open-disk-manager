#include "dialogs/partition_properties_dialog.hpp"
#include "opm/partition.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>

namespace opm::gui {

PartitionPropertiesDialog::PartitionPropertiesDialog(std::shared_ptr<opm::Partition> partition, QWidget* parent)
    : QDialog(parent)
    , partition_(partition)
    , partition_number_label_(nullptr)
    , start_sector_label_(nullptr)
    , end_sector_label_(nullptr)
    , size_label_(nullptr)
    , name_edit_(nullptr)
    , type_combo_(nullptr)
    , details_text_(nullptr)
    , apply_button_(nullptr)
    , button_box_(nullptr) {
    setWindowTitle("Partition Properties");
    setMinimumWidth(500);
    loadPartitionInfo();
    setupUI();
    updateUI();
}

void PartitionPropertiesDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);

    // General properties
    auto* general_group = new QGroupBox("General", this);
    auto* general_layout = new QGridLayout(general_group);

    general_layout->addWidget(new QLabel("Number:", general_group), 0, 0);
    partition_number_label_ = new QLabel(QString::number(partition_->number()), general_group);
    general_layout->addWidget(partition_number_label_, 0, 1);

    general_layout->addWidget(new QLabel("Start Sector:", general_group), 1, 0);
    start_sector_label_ = new QLabel(QString::number(partition_->startSector()), general_group);
    general_layout->addWidget(start_sector_label_, 1, 1);

    general_layout->addWidget(new QLabel("Size (sectors):", general_group), 2, 0);
    size_label_ = new QLabel(QString::number(partition_->sectorCount()) + QString(" sectors"), general_group);
    general_layout->addWidget(size_label_, 2, 1);

    general_layout->addWidget(new QLabel("Name:", general_group), 3, 0);
    name_edit_ = new QLineEdit(QString::fromStdString(partition_->name()), general_group);
    general_layout->addWidget(name_edit_, 3, 1);

    general_layout->addWidget(new QLabel("Type:", general_group), 4, 0);
    type_combo_ = new QComboBox(general_group);
    // Populate with common partition types
    type_combo_->addItem("Linux (0x83)", static_cast<int>(opm::PartitionType::Linux));
    type_combo_->addItem("Linux Swap (0x82)", static_cast<int>(opm::PartitionType::LinuxSwap));
    type_combo_->addItem("Linux LVM (0x8E)", static_cast<int>(opm::PartitionType::LinuxLVM));
    type_combo_->addItem("NTFS (0x07)", static_cast<int>(opm::PartitionType::NTFS));
    type_combo_->addItem("FAT32 (0x0C)", static_cast<int>(opm::PartitionType::FAT32LBA));
    type_combo_->addItem("EFI System (0xEF)", static_cast<int>(opm::PartitionType::EFI));
    general_layout->addWidget(type_combo_, 4, 1);

    main_layout->addWidget(general_group);

    // Details
    auto* details_group = new QGroupBox("Details", this);
    auto* details_layout = new QVBoxLayout(details_group);
    details_text_ = new QTextEdit(details_group);
    details_text_->setReadOnly(true);
    details_layout->addWidget(details_text_);
    main_layout->addWidget(details_group);

    // Buttons
    button_box_ = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
    apply_button_ = button_box_->button(QDialogButtonBox::Apply);
    main_layout->addWidget(button_box_);

    // Connections
    connect(name_edit_, &QLineEdit::textChanged, this, &PartitionPropertiesDialog::onNameChanged);
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PartitionPropertiesDialog::onTypeChanged);
    connect(button_box_, &QDialogButtonBox::clicked, [this](QAbstractButton* button) {
        if (button_box_->buttonRole(button) == QDialogButtonBox::ApplyRole) {
            onApplyClicked();
        } else if (button_box_->buttonRole(button) == QDialogButtonBox::RejectRole) {
            onCloseClicked();
        }
    });
}

void PartitionPropertiesDialog::loadPartitionInfo() {
    original_start_ = partition_->startSector();
    original_size_ = partition_->sectorCount();
    original_name_ = QString::fromStdString(partition_->name());
    current_type_ = partition_->type();
}

void PartitionPropertiesDialog::updateUI() {
    name_edit_->setText(original_name_);
    type_combo_->setCurrentIndex(0); // Default; in real code would map correctly
    apply_button_->setEnabled(false);
}

void PartitionPropertiesDialog::onNameChanged(const QString& text) {
    apply_button_->setEnabled(text != original_name_);
}

void PartitionPropertiesDialog::onTypeChanged(int index) {
    apply_button_->setEnabled(true);
}

void PartitionPropertiesDialog::onApplyClicked() {
    // Apply changes to partition
    // Note: Real implementation would modify partition object and disk
    QString new_name = name_edit_->text();
    if (new_name != original_name_) {
        original_name_ = new_name;
        apply_button_->setEnabled(false);
        emit partitionModified();
    }
}

void PartitionPropertiesDialog::onCloseClicked() {
    reject();
}

bool PartitionPropertiesDialog::validateInput() {
    // Basic validation
    if (name_edit_->text().isEmpty()) {
        return false;
    }
    return true;
}

} // namespace opm::gui
