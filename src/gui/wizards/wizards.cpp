#include "wizards.hpp"
#include "opm/disk_io.hpp"
#include "opm/clone.hpp"
#include "opm/boot.hpp"
#include "opm/recovery.hpp"
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>

namespace opm::gui {

namespace {

// Populate a combo box with device paths (and human-readable sizes).
void fillDiskCombo(QComboBox* combo,
                   const std::vector<std::shared_ptr<DiskIO>>& disks) {
    combo->clear();
    for (const auto& d : disks) {
        if (!d) continue;
        combo->addItem(QString("%1  (%2)")
                           .arg(QString::fromStdString(d->devicePath()))
                           .arg(QString::fromStdString(
                               utils::formatBytes(d->size()))),
                       QString::fromStdString(d->devicePath()));
    }
}

} // namespace

// ===========================================================================
// OpWizard base
// ===========================================================================

OpWizard::OpWizard(QWidget* parent)
    : QWizard(parent) {
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoCancelButtonOnLastPage, true);
}

bool OpWizard::isComplete() const {
    return operation_done_;
}

void OpWizard::initializePage(int id) {
    QWizard::initializePage(id);
    // The final page is the one with no next page (Qt 6 removed pageCount()).
    if (nextId() == -1 && !operation_done_) {
        // Trigger the operation when the final page appears.
        QMetaObject::invokeMethod(this, [this]() { runOperation(); },
                                  Qt::QueuedConnection);
    }
}

// After an operation completes, refresh the wizard's button state.
void OpWizard::finishOperation(const QString& message) {
    result_message_ = message;
    if (status_label_) status_label_->setText(message);
    if (progress_bar_) progress_bar_->setValue(100);
    operation_done_ = true;
    auto* finish = button(QWizard::FinishButton);
    if (finish) finish->setEnabled(true);
    update();
}

// ===========================================================================
// CloneDiskWizard
// ===========================================================================

CloneDiskWizard::CloneDiskWizard(
    const std::vector<std::shared_ptr<DiskIO>>& disks, QWidget* parent)
    : OpWizard(parent) {
    setWindowTitle(tr("Clone Disk Wizard"));

    // Page 1: source/target.
    auto* page1 = new QWizardPage(this);
    page1->setTitle(tr("Choose disks"));
    auto* form = new QFormLayout(page1);
    source_combo_ = new QComboBox(page1);
    target_combo_ = new QComboBox(page1);
    fillDiskCombo(source_combo_, disks);
    fillDiskCombo(target_combo_, disks);
    form->addRow(tr("Source disk:"), source_combo_);
    form->addRow(tr("Target disk:"), target_combo_);
    addPage(page1);

    // Page 2: run.
    auto* page2 = new QWizardPage(this);
    page2->setTitle(tr("Cloning"));
    auto* v = new QVBoxLayout(page2);
    status_label_ = new QLabel(tr("Cloning in progress..."), page2);
    progress_bar_ = new QProgressBar(page2);
    progress_bar_->setRange(0, 100);
    v->addWidget(status_label_);
    v->addWidget(progress_bar_);
    addPage(page2);
}

void CloneDiskWizard::runOperation() {
    std::string src = source_combo_->currentData().toString().toStdString();
    std::string dst = target_combo_->currentData().toString().toStdString();
    auto source = DiskIO::openReadWrite(src);
    auto target = DiskIO::openReadWrite(dst);
    if (!source || !target) {
        result_message_ = tr("Cannot open source or target read-write (are you root?)");
        status_label_->setText(result_message_);
        finishOperation(result_message_);
        return;
    }
    CloneOptions opts;
    opts.progress_callback = [this](uint64_t done, uint64_t total) {
        if (total > 0) {
            int pct = static_cast<int>((done * 100) / total);
            progress_bar_->setValue(pct > 100 ? 100 : pct);
        }
    };
    Result r = cloneDisk(source, target, opts);
    if (r.failed()) {
        result_message_ = tr("Clone failed: %1").arg(r.message.c_str());
    } else {
        result_message_ = tr("Clone complete: %1 -> %2")
                              .arg(QString::fromStdString(src),
                                   QString::fromStdString(dst));
    }
    status_label_->setText(result_message_);
    progress_bar_->setValue(100);
    finishOperation(result_message_);
}

// ===========================================================================
// MigrateOSWizard
// ===========================================================================

MigrateOSWizard::MigrateOSWizard(
    const std::vector<std::shared_ptr<DiskIO>>& disks, QWidget* parent)
    : OpWizard(parent) {
    setWindowTitle(tr("Migrate OS Wizard"));

    auto* page1 = new QWizardPage(this);
    page1->setTitle(tr("Choose source and target"));
    auto* form = new QFormLayout(page1);
    source_combo_ = new QComboBox(page1);
    target_combo_ = new QComboBox(page1);
    fillDiskCombo(source_combo_, disks);
    fillDiskCombo(target_combo_, disks);
    form->addRow(tr("Source (current system disk):"), source_combo_);
    form->addRow(tr("Target (new disk):"), target_combo_);
    page1->setSubTitle(
        tr("The source disk is copied to the target; partitions are resized "
           "to fill the target where possible."));
    addPage(page1);

    auto* page2 = new QWizardPage(this);
    page2->setTitle(tr("Migrating"));
    auto* v = new QVBoxLayout(page2);
    status_label_ = new QLabel(tr("Migrating..."), page2);
    progress_bar_ = new QProgressBar(page2);
    progress_bar_->setRange(0, 100);
    v->addWidget(status_label_);
    v->addWidget(progress_bar_);
    addPage(page2);
}

void MigrateOSWizard::runOperation() {
    std::string src = source_combo_->currentData().toString().toStdString();
    std::string dst = target_combo_->currentData().toString().toStdString();
    auto source = DiskIO::openReadWrite(src);
    auto target = DiskIO::openReadWrite(dst);
    if (!source || !target) {
        result_message_ = tr("Cannot open source or target read-write (are you root?)");
        status_label_->setText(result_message_);
        finishOperation(result_message_);
        return;
    }
    CloneOptions opts;
    opts.progress_callback = [this](uint64_t done, uint64_t total) {
        if (total > 0) {
            int pct = static_cast<int>((done * 100) / total);
            progress_bar_->setValue(pct > 100 ? 100 : pct);
        }
    };
    Result r = cloneDiskWithResize(source, target, opts);
    if (r.failed()) {
        result_message_ = tr("Migration failed: %1").arg(r.message.c_str());
    } else {
        result_message_ = tr("Migration complete. The target disk is bootable-ready.");
    }
    status_label_->setText(result_message_);
    progress_bar_->setValue(100);
    finishOperation(result_message_);
}

// ===========================================================================
// BootableMediaWizard
// ===========================================================================

BootableMediaWizard::BootableMediaWizard(
    const std::vector<std::shared_ptr<DiskIO>>& disks, QWidget* parent)
    : OpWizard(parent) {
    setWindowTitle(tr("Bootable Media Wizard"));

    auto* page1 = new QWizardPage(this);
    page1->setTitle(tr("Select ISO and USB device"));
    auto* form = new QFormLayout(page1);
    iso_edit_ = new QLineEdit(page1);
    auto* browse = new QPushButton(tr("Browse..."), page1);
    usb_combo_ = new QComboBox(page1);
    fillDiskCombo(usb_combo_, disks);
    auto* iso_row = new QHBoxLayout();
    iso_row->addWidget(iso_edit_);
    iso_row->addWidget(browse);
    form->addRow(tr("ISO image:"), iso_row);
    form->addRow(tr("USB device:"), usb_combo_);
    connect(browse, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(
            this, tr("Select ISO"), QString(), tr("ISO images (*.iso)"));
        if (!f.isEmpty()) iso_edit_->setText(f);
    });
    addPage(page1);

    auto* page2 = new QWizardPage(this);
    page2->setTitle(tr("Writing"));
    auto* v = new QVBoxLayout(page2);
    status_label_ = new QLabel(tr("Writing ISO..."), page2);
    progress_bar_ = new QProgressBar(page2);
    progress_bar_->setRange(0, 100);
    v->addWidget(status_label_);
    v->addWidget(progress_bar_);
    addPage(page2);
}

void BootableMediaWizard::runOperation() {
    std::string iso = iso_edit_->text().trimmed().toStdString();
    std::string usb = usb_combo_->currentData().toString().toStdString();
    if (iso.empty()) {
        result_message_ = tr("No ISO selected");
        status_label_->setText(result_message_);
        finishOperation(result_message_);
        return;
    }
    auto target = DiskIO::openReadWrite(usb);
    if (!target) {
        result_message_ = tr("Cannot open USB device read-write (are you root?)");
        status_label_->setText(result_message_);
        finishOperation(result_message_);
        return;
    }
    LiveUSBOptions opts;
    opts.iso_path = iso;
    opts.verify_after_write = true;
    opts.progress_callback = [this](uint64_t done, uint64_t total) {
        if (total > 0) {
            int pct = static_cast<int>((done * 100) / total);
            progress_bar_->setValue(pct > 100 ? 100 : pct);
        }
    };
    Result r = createLiveUSB(target, opts);
    if (r.failed()) {
        result_message_ = tr("Write failed: %1").arg(r.message.c_str());
    } else {
        result_message_ = tr("Bootable USB created successfully.");
    }
    status_label_->setText(result_message_);
    progress_bar_->setValue(100);
    finishOperation(result_message_);
}

// ===========================================================================
// RecoveryWizard
// ===========================================================================

RecoveryWizard::RecoveryWizard(
    const std::vector<std::shared_ptr<DiskIO>>& disks, QWidget* parent)
    : OpWizard(parent) {
    setWindowTitle(tr("Partition Recovery Wizard"));

    auto* page1 = new QWizardPage(this);
    page1->setTitle(tr("Choose device"));
    auto* form = new QFormLayout(page1);
    device_combo_ = new QComboBox(page1);
    fillDiskCombo(device_combo_, disks);
    form->addRow(tr("Device:"), device_combo_);
    page1->setSubTitle(
        tr("Scans the device for partition-table entries and filesystem "
           "signatures, then rebuilds an MBR table from what it finds. "
           "Partition data is not modified."));
    addPage(page1);

    auto* page2 = new QWizardPage(this);
    page2->setTitle(tr("Scanning and rebuilding"));
    auto* v = new QVBoxLayout(page2);
    status_label_ = new QLabel(tr("Scanning..."), page2);
    progress_bar_ = new QProgressBar(page2);
    progress_bar_->setRange(0, 100);
    v->addWidget(status_label_);
    v->addWidget(progress_bar_);
    addPage(page2);
}

void RecoveryWizard::runOperation() {
    std::string dev = device_combo_->currentData().toString().toStdString();
    auto disk = DiskIO::openReadWrite(dev);
    if (!disk) {
        result_message_ = tr("Cannot open device read-write (are you root?)");
        status_label_->setText(result_message_);
        finishOperation(result_message_);
        return;
    }
    auto candidates = scanForPartitions(disk, 2048,
        [this](uint64_t cur, uint64_t total, const std::string&) {
            if (total > 0) {
                int pct = static_cast<int>((cur * 100) / total);
                progress_bar_->setValue(pct > 100 ? 100 : pct);
            }
        });
    if (candidates.empty()) {
        result_message_ = tr("No partitions or filesystem signatures found.");
    } else {
        status_label_->setText(
            tr("Found %1 candidate(s); rebuilding MBR table...")
                .arg(candidates.size()));
        Result r = rebuildPartitionTable(disk, candidates);
        if (r.failed()) {
            result_message_ = tr("Rebuild failed: %1").arg(r.message.c_str());
        } else {
            auto table = PartitionTable::load(disk);
            result_message_ = tr("Rebuilt MBR table with %1 partition(s).")
                                  .arg(table ? table->getPartitionCount() : 0);
        }
    }
    status_label_->setText(result_message_);
    progress_bar_->setValue(100);
    finishOperation(result_message_);
}

} // namespace opm::gui