#pragma once

#include <QWizard>
#include <QString>
#include <memory>
#include <vector>

class QComboBox;
class QLineEdit;
class QProgressBar;
class QLabel;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// ---------------------------------------------------------------------------
// Base wizard: runs a core operation on the final page with a progress bar.
// Subclasses implement runOperation() and expose their inputs.
// ---------------------------------------------------------------------------
class OpWizard : public QWizard {
    Q_OBJECT

public:
    explicit OpWizard(QWidget* parent = nullptr);

    bool isComplete() const;   // only complete when the op ran

protected:
    void initializePage(int id) override;
    virtual void runOperation() = 0;
    void finishOperation(const QString& message);

    QProgressBar* progress_bar_ = nullptr;
    QLabel* status_label_ = nullptr;
    bool operation_done_ = false;
    QString result_message_;
};

// Clone a whole disk (sector copy + optional verify).
class CloneDiskWizard : public OpWizard {
    Q_OBJECT
public:
    explicit CloneDiskWizard(const std::vector<std::shared_ptr<DiskIO>>& disks,
                             QWidget* parent = nullptr);
protected:
    void runOperation() override;
private:
    QComboBox* source_combo_ = nullptr;
    QComboBox* target_combo_ = nullptr;
};

// Migrate an OS: clone source disk to a target with partition resize.
class MigrateOSWizard : public OpWizard {
    Q_OBJECT
public:
    explicit MigrateOSWizard(const std::vector<std::shared_ptr<DiskIO>>& disks,
                             QWidget* parent = nullptr);
protected:
    void runOperation() override;
private:
    QComboBox* source_combo_ = nullptr;
    QComboBox* target_combo_ = nullptr;
};

// Create bootable media from an ISO.
class BootableMediaWizard : public OpWizard {
    Q_OBJECT
public:
    explicit BootableMediaWizard(const std::vector<std::shared_ptr<DiskIO>>& disks,
                                 QWidget* parent = nullptr);
protected:
    void runOperation() override;
private:
    QLineEdit* iso_edit_ = nullptr;
    QComboBox* usb_combo_ = nullptr;
};

// Recover a lost partition table (signature scan + MBR rebuild).
class RecoveryWizard : public OpWizard {
    Q_OBJECT
public:
    explicit RecoveryWizard(const std::vector<std::shared_ptr<DiskIO>>& disks,
                            QWidget* parent = nullptr);
protected:
    void runOperation() override;
private:
    QComboBox* device_combo_ = nullptr;
};

} // namespace opm::gui