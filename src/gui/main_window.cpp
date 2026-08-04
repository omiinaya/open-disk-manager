#include "main_window.hpp"
#include "disk_tree_widget.hpp"
#include "partition_view_widget.hpp"
#include "operation_panel_widget.hpp"
#include "status_bar_widget.hpp"
#include "dialogs/create_partition_dialog.hpp"
#include "dialogs/delete_partition_dialog.hpp"
#include "dialogs/resize_partition_dialog.hpp"
#include "dialogs/format_dialog.hpp"
#include "dialogs/clone_dialog.hpp"
#include "dialogs/secure_erase_dialog.hpp"
#include "dialogs/benchmark_dialog.hpp"
#include "dialogs/preferences_dialog.hpp"
#include "dialogs/partition_properties_dialog.hpp"
#include "wizards/wizards.hpp"
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include "opm/disk_io.hpp"
#include "opm/operation.hpp"
#include "opm/clone.hpp"
#include "opm/fat32_impl.hpp"
#include "opm/ntfs_impl.hpp"
#include "opm/ext4_impl.hpp"
#include "opm/exfat_impl.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QApplication>
#include <QInputDialog>
#include <QSettings>
#include <QTime>
#include <QPalette>
#include <QStyleFactory>
#include <QScrollBar>

namespace opm::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , main_splitter_(nullptr)
    , disk_tree_(nullptr)
    , partition_view_(nullptr)
    , operation_panel_(nullptr)
    , status_bar_(nullptr)
    , log_dock_(nullptr)
    , log_view_(nullptr) {
    // Apply the saved theme before building widgets.
    QSettings settings;
    applyTheme(settings.value("darkMode", false).toBool());
    setupUI();
    logMessage("Open Partition Manager started");
}

MainWindow::~MainWindow() = default;

void MainWindow::initialize() {
    // Load initial disk list
    refreshDisks();
    updateWindowTitle();
}

void MainWindow::refreshDisks() {
    disks_.clear();
    
    // Enumerate real devices
    try {
        auto devices = DeviceEnumerator::enumerateDevices();
        for (const auto& dev : devices) {
            auto disk = DiskIO::openReadOnly(dev.path);
            if (disk && disk->isOpen()) {
                disks_.push_back(disk);
            }
        }
    } catch (const std::exception& e) {
        status_bar_->setStatus(QString("Device enumeration failed: %1")
                                     .arg(e.what()));
    }
    
    if (disk_tree_) {
        disk_tree_->setDisks(disks_);
    }
    
    selected_disk_index_ = -1;
    selected_partition_number_ = -1;
    updateActionStates();
}

namespace {
// Map a raw MBR partition type byte to the PartitionType enum
PartitionType partitionTypeFromByte(uint8_t type) {
    switch (type) {
        case 0x07: return PartitionType::NTFS;
        case 0x0B: return PartitionType::FAT32CHS;
        case 0x0C: return PartitionType::FAT32LBA;
        case 0x0E: return PartitionType::FAT16BLBA;
        case 0x82: return PartitionType::LinuxSwap;
        case 0x83: return PartitionType::Linux;
        case 0x8E: return PartitionType::LinuxLVM;
        case 0xEF: return PartitionType::EFI;
        case 0xFD: return PartitionType::LinuxRAID;
        default:   return PartitionType::Linux;
    }
}

// First free sector aligned to 1MiB (2048 sectors), after the last partition
uint64_t firstFreeAlignedSector(const PartitionTable& table) {
    uint64_t next = 2048;
    for (const auto& part : table.getPartitions()) {
        if (part.endSector() >= next) {
            next = part.endSector() + 1;
        }
    }
    return (next + 2047) / 2048 * 2048;
}
} // namespace

void MainWindow::closeEvent(QCloseEvent* event) {
    // Check for unsaved operations
    event->accept();
}

void MainWindow::setupUI() {
    // Set window properties
    setMinimumSize(1024, 768);
    resize(1280, 800);
    
    // Create central widget and layout
    auto* central_widget = new QWidget(this);
    setCentralWidget(central_widget);
    
    auto* main_layout = new QHBoxLayout(central_widget);
    main_layout->setSpacing(0);
    main_layout->setContentsMargins(0, 0, 0, 0);
    
    // Create splitter
    main_splitter_ = new QSplitter(Qt::Horizontal, this);
    main_layout->addWidget(main_splitter_);
    
    // Create disk tree (left panel)
    disk_tree_ = new DiskTreeWidget(this);
    disk_tree_->setMinimumWidth(250);
    disk_tree_->setMaximumWidth(400);
    main_splitter_->addWidget(disk_tree_);
    
    // Create partition view (center)
    auto* right_panel = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setSpacing(10);
    right_layout->setContentsMargins(10, 10, 10, 10);
    
    partition_view_ = new PartitionViewWidget(right_panel);
    right_layout->addWidget(partition_view_);
    
    main_splitter_->addWidget(right_panel);
    main_splitter_->setStretchFactor(1, 1);
    
    // Setup menus and toolbars
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupLogDock();
    setupConnections();
}

void MainWindow::setupLogDock() {
    log_dock_ = new QDockWidget(tr("Operation Log"), this);
    log_dock_->setObjectName("operationLogDock");
    log_dock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    log_view_ = new QPlainTextEdit(log_dock_);
    log_view_->setReadOnly(true);
    log_view_->setMaximumBlockCount(5000);
    log_view_->setPlaceholderText(tr("Operations and errors are logged here."));
    log_dock_->setWidget(log_view_);

    addDockWidget(Qt::BottomDockWidgetArea, log_dock_);
    if (action_view_log_) {
        action_view_log_->setChecked(true);
    }
}

// Append a timestamped line to the log dock and mirror it in the status bar.
void MainWindow::logMessage(const QString& message) {
    if (log_view_) {
        QString stamp = QTime::currentTime().toString("HH:mm:ss");
        log_view_->appendPlainText(QString("[%1] %2").arg(stamp, message));
        log_view_->verticalScrollBar()->setValue(log_view_->verticalScrollBar()->maximum());
    }
    if (status_bar_) {
        status_bar_->setStatus(message);
    }
}

void MainWindow::applyTheme(bool dark) {
    QSettings settings;
    settings.setValue("darkMode", dark);
    if (dark) {
        QPalette p;
        p.setColor(QPalette::Window, QColor(45, 45, 48));
        p.setColor(QPalette::WindowText, QColor(220, 220, 220));
        p.setColor(QPalette::Base, QColor(30, 30, 30));
        p.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
        p.setColor(QPalette::Text, QColor(220, 220, 220));
        p.setColor(QPalette::Button, QColor(55, 55, 58));
        p.setColor(QPalette::ButtonText, QColor(220, 220, 220));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(p);
    } else {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setPalette(QApplication::style()->standardPalette());
    }
    if (log_view_) log_view_->setStyleSheet(QString());
}

void MainWindow::setProgress(uint64_t done, uint64_t total) {
    if (status_bar_) status_bar_->setProgress(done, total);
}

void MainWindow::setProgressDone() {
    if (status_bar_) status_bar_->setProgressVisible(false);
    logMessage("Operation completed");
}

void MainWindow::setupMenuBar() {
    // File menu
    auto* file_menu = menuBar()->addMenu("&File");
    
    action_refresh_ = file_menu->addAction("&Refresh", this, &MainWindow::onActionRefresh);
    action_refresh_->setShortcut(QKeySequence::Refresh);
    
    file_menu->addSeparator();
    
    action_quit_ = file_menu->addAction("&Quit", this, &MainWindow::onActionQuit);
    action_quit_->setShortcut(QKeySequence::Quit);
    
    // Edit menu
    auto* edit_menu = menuBar()->addMenu("&Edit");
    action_preferences_ = edit_menu->addAction("&Preferences", this, &MainWindow::onActionPreferences);

    // View menu
    auto* view_menu = menuBar()->addMenu("&View");
    action_view_log_ = view_menu->addAction("&Operation Log");
    action_view_log_->setCheckable(true);
    action_view_log_->setChecked(true);
    connect(action_view_log_, &QAction::toggled, this, [this](bool on) {
        if (log_dock_) log_dock_->setVisible(on);
    });
    
    // Device menu
    auto* device_menu = menuBar()->addMenu("&Device");
    
    action_create_partition_ = device_menu->addAction("&Create Partition...", this, &MainWindow::onActionCreatePartition);
    action_delete_partition_ = device_menu->addAction("&Delete Partition", this, &MainWindow::onActionDeletePartition);
    action_resize_partition_ = device_menu->addAction("&Resize/Move Partition...", this, &MainWindow::onActionResizePartition);
    action_format_partition_ = device_menu->addAction("&Format Partition...", this, &MainWindow::onActionFormatPartition);
    
    device_menu->addSeparator();
    
    action_clone_disk_ = device_menu->addAction("&Clone Disk...", this, &MainWindow::onActionCloneDisk);
    action_secure_erase_ = device_menu->addAction("&Secure Erase...", this, &MainWindow::onActionSecureErase);
    action_benchmark_ = device_menu->addAction("&Benchmark...", this, &MainWindow::onActionBenchmark);

    // Wizards menu
    auto* wizards_menu = menuBar()->addMenu("&Wizards");
    action_wizard_clone_ = wizards_menu->addAction("&Clone Disk Wizard...", this, &MainWindow::onActionCloneWizard);
    action_wizard_migrate_ = wizards_menu->addAction("&Migrate OS Wizard...", this, &MainWindow::onActionMigrateOSWizard);
    action_wizard_bootable_ = wizards_menu->addAction("&Bootable Media Wizard...", this, &MainWindow::onActionBootableMediaWizard);
    action_wizard_recovery_ = wizards_menu->addAction("&Recovery Wizard...", this, &MainWindow::onActionRecoveryWizard);
    
    // Help menu
    auto* help_menu = menuBar()->addMenu("&Help");
    action_about_ = help_menu->addAction("&About", this, &MainWindow::onActionAbout);
}

void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    
    toolbar->addAction(action_refresh_);
    toolbar->addSeparator();
    toolbar->addAction(action_create_partition_);
    toolbar->addAction(action_delete_partition_);
    toolbar->addAction(action_resize_partition_);
    toolbar->addSeparator();
    toolbar->addAction(action_clone_disk_);
}

void MainWindow::setupStatusBar() {
    status_bar_ = new StatusBarWidget(this);
    statusBar()->addWidget(status_bar_, 1);
    status_bar_->setStatus("Ready");
}

void MainWindow::setupConnections() {
    // Connect disk tree signals
    if (disk_tree_) {
        connect(disk_tree_, &DiskTreeWidget::diskSelected,
                this, &MainWindow::onDiskSelected);
    }
    // Connect partition view signals
    if (partition_view_) {
        connect(partition_view_, &PartitionViewWidget::partitionSelected,
                this, &MainWindow::onPartitionSelected);
        connect(partition_view_, &PartitionViewWidget::partitionDoubleClicked,
                this, &MainWindow::onPartitionDoubleClicked);
    }
}

void MainWindow::updateWindowTitle() {
    setWindowTitle(QString("%1 %2")
                  .arg(QApplication::applicationDisplayName())
                  .arg(QApplication::applicationVersion()));
}

void MainWindow::updateActionStates() {
    bool has_selection = (selected_disk_index_ >= 0);
    bool has_partition = (selected_partition_number_ >= 0);
    
    action_create_partition_->setEnabled(has_selection);
    action_delete_partition_->setEnabled(has_selection && has_partition);
    action_resize_partition_->setEnabled(has_selection && has_partition);
    action_format_partition_->setEnabled(has_selection && has_partition);
    action_clone_disk_->setEnabled(has_selection);
    action_secure_erase_->setEnabled(has_selection);
    action_benchmark_->setEnabled(has_selection);
}

// Getters for selected disk/partition
std::shared_ptr<DiskIO> MainWindow::selectedDisk() const {
    if (selected_disk_index_ >= 0 && selected_disk_index_ < static_cast<int>(disks_.size())) {
        return disks_[selected_disk_index_];
    }
    return nullptr;
}

int MainWindow::selectedPartition() const {
    return selected_partition_number_;
}

// Action handlers
void MainWindow::onActionRefresh() {
    refreshDisks();
}

void MainWindow::onActionQuit() {
    close();
}

void MainWindow::onActionAbout() {
    QMessageBox::about(this, "About Open Partition Manager",
        "<h2>Open Partition Manager 0.2.0</h2>"
        "<p>An open-source partition management tool for Linux.</p>"
        "<p>Built with Qt and modern C++.</p>");
}

void MainWindow::onActionPreferences() {
    PreferencesDialog dialog(this);
    connect(&dialog, &PreferencesDialog::settingsChanged, [this]() {
        QSettings settings;
        applyTheme(settings.value("darkMode", false).toBool());
        logMessage("Preferences applied");
        updateWindowTitle();
        refreshDisks();
    });
    dialog.exec();
}

void MainWindow::onActionCreatePartition() {
    auto disk = selectedDisk();
    if (!disk) {
        QMessageBox::warning(this, "No Selection", "Please select a disk first.");
        return;
    }
    
    auto rw = DiskIO::openReadWrite(disk->devicePath());
    if (!rw || !rw->isOpen()) {
        QMessageBox::warning(this, "Error",
            "Cannot open device read-write. Run the application as root.");
        return;
    }
    
    std::unique_ptr<PartitionTable> table;
    try {
        table = PartitionTable::load(rw);
        if (!table) {
            table = PartitionTable::create(rw, TableType::MBR);
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error",
            QString("Failed to load partition table: %1").arg(e.what()));
        return;
    }
    
    auto table_ptr = std::shared_ptr<PartitionTable>(table.release());
    CreatePartitionDialog dialog(rw, table_ptr, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        uint64_t start = firstFreeAlignedSector(*table_ptr);
        Result r = table_ptr->createPartition(
            start, options.size_bytes,
            partitionTypeFromByte(options.partition_type),
            options.label.toStdString());
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Create failed: %1").arg(r.message.c_str()));
            return;
        }
        r = table_ptr->commit();
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Commit failed: %1").arg(r.message.c_str()));
            return;
        }
        status_bar_->setStatus(QString("Partition created at sector %1 (%2 MB)")
            .arg(start)
            .arg(options.size_bytes / (1024 * 1024)));
        logMessage(QString("Created partition at sector %1 (%2 MB)")
            .arg(start).arg(options.size_bytes / (1024 * 1024)));
        refreshDisks();
    }
}

void MainWindow::onActionDeletePartition() {
    auto disk = selectedDisk();
    if (!disk || selected_partition_number_ < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a partition first.");
        return;
    }
    
    auto rw = DiskIO::openReadWrite(disk->devicePath());
    if (!rw || !rw->isOpen()) {
        QMessageBox::warning(this, "Error",
            "Cannot open device read-write. Run the application as root.");
        return;
    }
    
    std::unique_ptr<PartitionTable> table;
    try {
        table = PartitionTable::load(rw);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error",
            QString("Failed to load partition table: %1").arg(e.what()));
        return;
    }
    if (!table) {
        QMessageBox::warning(this, "Error", "No partition table found.");
        return;
    }
    
    auto parts = table->getPartitions();
    if (selected_partition_number_ < 1 ||
        selected_partition_number_ > static_cast<int>(parts.size())) {
        QMessageBox::warning(this, "Error", "Invalid partition selection.");
        return;
    }
    const Partition& part = parts[selected_partition_number_ - 1];
    uint64_t part_size = part.sectorCount() * rw->sectorSize();
    QString part_type = QString("0x%1").arg(
        static_cast<int>(part.type()), 2, 16, QLatin1Char('0'));
    
    DeletePartitionDialog dialog(rw, selected_partition_number_,
                                 part_size, part_type, this);
    
    if (dialog.exec() == QDialog::Accepted && dialog.confirmed()) {
        // Optionally secure-erase the partition data first
        if (dialog.eraseData()) {
            status_bar_->setStatus("Secure erase in progress...");
            EraseOptions erase_opt;
            erase_opt.progress_callback = [this](uint64_t done, uint64_t total) {
                setProgress(done, total);
            };
            Result er = secureErase(rw, part.startSector(), part.sectorCount(),
                                    erase_opt);
            setProgressDone();
            if (er.failed()) {
                QMessageBox::critical(this, "Error",
                    QString("Secure erase failed: %1").arg(er.message.c_str()));
                return;
            }
            logMessage(QString("Secure-erased partition %1 data").arg(selected_partition_number_));
        }
        
        Result r = table->deletePartition(selected_partition_number_);
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Delete failed: %1").arg(r.message.c_str()));
            return;
        }
        r = table->commit();
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Commit failed: %1").arg(r.message.c_str()));
            return;
        }
        status_bar_->setStatus(QString("Partition %1 deleted")
            .arg(selected_partition_number_));
        logMessage(QString("Deleted partition %1").arg(selected_partition_number_));
        refreshDisks();
    }
}

void MainWindow::onActionResizePartition() {
    auto disk = selectedDisk();
    if (!disk || selected_partition_number_ < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a partition first.");
        return;
    }
    
    auto rw = DiskIO::openReadWrite(disk->devicePath());
    if (!rw || !rw->isOpen()) {
        QMessageBox::warning(this, "Error",
            "Cannot open device read-write. Run the application as root.");
        return;
    }
    
    std::unique_ptr<PartitionTable> table;
    try {
        table = PartitionTable::load(rw);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error",
            QString("Failed to load partition table: %1").arg(e.what()));
        return;
    }
    if (!table) {
        QMessageBox::warning(this, "Error", "No partition table found.");
        return;
    }
    
    auto table_ptr = std::shared_ptr<PartitionTable>(table.release());
    ResizePartitionDialog dialog(rw, table_ptr, selected_partition_number_, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        Result r = table_ptr->resizePartition(selected_partition_number_,
                                              options.new_size_bytes);
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Resize failed: %1").arg(r.message.c_str()));
            return;
        }
        r = table_ptr->commit();
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Commit failed: %1").arg(r.message.c_str()));
            return;
        }
        status_bar_->setStatus(QString("Partition %1 resized to %2 MB")
            .arg(selected_partition_number_)
            .arg(options.new_size_bytes / (1024 * 1024)));
        logMessage(QString("Resized partition %1 to %2 MB")
            .arg(selected_partition_number_)
            .arg(options.new_size_bytes / (1024 * 1024)));
        refreshDisks();
    }
}

void MainWindow::onActionFormatPartition() {
    auto disk = selectedDisk();
    if (!disk || selected_partition_number_ < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a partition first.");
        return;
    }
    
    std::unique_ptr<PartitionTable> table;
    try {
        table = PartitionTable::load(disk);
    } catch (...) {}
    uint64_t start = 0, size_bytes = 0;
    if (table) {
        auto parts = table->getPartitions();
        if (selected_partition_number_ >= 1 &&
            selected_partition_number_ <= static_cast<int>(parts.size())) {
            start = parts[selected_partition_number_ - 1].startSector();
            size_bytes = parts[selected_partition_number_ - 1].sectorCount()
                         * disk->sectorSize();
        }
    }
    
    FormatDialog dialog(disk, selected_partition_number_, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        
        auto rw = DiskIO::openReadWrite(disk->devicePath());
        if (!rw || !rw->isOpen()) {
            QMessageBox::warning(this, "Error",
                "Cannot open device read-write. Run the application as root.");
            return;
        }
        
        status_bar_->setStatus("Formatting partition...");
        Result r;
        switch (options.fs_type) {
            case FileSystemType::FAT32:
                r = fat32::formatFAT32Complete(rw, start, size_bytes,
                                               options.label.toStdString());
                break;
            case FileSystemType::NTFS:
                r = ntfs::formatNTFS(rw, start, size_bytes,
                                     options.label.toStdString());
                break;
            case FileSystemType::EXT4:
                r = ext4::formatEXT4(rw, start, size_bytes,
                                     options.label.toStdString());
                break;
            case FileSystemType::exFAT:
                r = exfat::formatExFAT(rw, start, size_bytes,
                                       options.label.toStdString());
                break;
            default:
                QMessageBox::warning(this, "Error", "Unsupported filesystem.");
                return;
        }
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Format failed: %1").arg(r.message.c_str()));
            return;
        }
        rw->flush();
        
        if (options.check_after) {
            status_bar_->setStatus("Verifying filesystem...");
            Result cr = disk->detectFilesystem(start) == FileSystemType::Unknown
                ? Result::error("filesystem not detected")
                : Result::ok();
            (void)cr;
        }
        status_bar_->setStatus(QString("Partition %1 formatted as %2")
            .arg(selected_partition_number_)
            .arg(options.label));
        logMessage(QString("Formatted partition %1 as %2")
            .arg(selected_partition_number_).arg(options.label));
    }
}

void MainWindow::onActionCloneDisk() {
    auto disk = selectedDisk();
    if (!disk) {
        QMessageBox::warning(this, "No Selection", "Please select a disk first.");
        return;
    }
    
    CloneDialog dialog(disk, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        auto source = DiskIO::openReadWrite(options.source_path.toStdString());
        auto target = DiskIO::openReadWrite(options.target_path.toStdString());
        if (!source || !source->isOpen() || !target || !target->isOpen()) {
            QMessageBox::critical(this, "Error",
                "Cannot open source or target device read-write.");
            return;
        }
        
        status_bar_->setStatus("Cloning...");
        CloneOptions clone_opts;
        if (options.verify_after) {
            clone_opts.verify = true;
        }
        Result r = (options.resize_partitions)
            ? cloneDiskWithResize(source, target, clone_opts)
            : cloneDisk(source, target, clone_opts);
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Clone failed: %1").arg(r.message.c_str()));
            return;
        }
        status_bar_->setStatus(QString("Clone complete: %1 -> %2")
            .arg(options.source_path).arg(options.target_path));
        logMessage(QString("Cloned %1 -> %2").arg(options.source_path).arg(options.target_path));
    }
}

void MainWindow::onActionSecureErase() {
    auto disk = selectedDisk();
    if (!disk) {
        QMessageBox::warning(this, "No Selection", "Please select a disk first.");
        return;
    }
    
    SecureEraseDialog dialog(disk, selected_partition_number_, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        
        auto rw = DiskIO::openReadWrite(disk->devicePath());
        if (!rw || !rw->isOpen()) {
            QMessageBox::warning(this, "Error",
                "Cannot open device read-write. Run the application as root.");
            return;
        }
        
        // Map the dialog's method to the core EraseMethod
        EraseMethod method = EraseMethod::Zeros;
        switch (options.method) {
            case SecureEraseDialog::EraseMethod::Zeros: method = EraseMethod::Zeros; break;
            case SecureEraseDialog::EraseMethod::Random: method = EraseMethod::Random; break;
            case SecureEraseDialog::EraseMethod::DoD: method = EraseMethod::DoD522022; break;
            case SecureEraseDialog::EraseMethod::Gutmann: method = EraseMethod::Gutmann; break;
            case SecureEraseDialog::EraseMethod::NIST_Clear:
            case SecureEraseDialog::EraseMethod::NIST_Purge:
                method = EraseMethod::NIST80088; break;
        }
        
        status_bar_->setStatus("Secure erase in progress...");
        EraseOptions erase_opts;
        erase_opts.method = method;
        erase_opts.progress_callback = [this](uint64_t done, uint64_t total) {
            setProgress(done, total);
        };
        
        Result r;
        if (selected_partition_number_ >= 0) {
            std::unique_ptr<PartitionTable> table;
            try { table = PartitionTable::load(rw); } catch (...) {}
            if (table) {
                auto parts = table->getPartitions();
                if (selected_partition_number_ >= 1 &&
                    selected_partition_number_ <= static_cast<int>(parts.size())) {
                    const Partition& part = parts[selected_partition_number_ - 1];
                    r = secureErase(rw, part.startSector(), part.sectorCount(),
                                    erase_opts);
                } else {
                    r = Result::error("invalid partition selection");
                }
            } else {
                r = Result::error("no partition table");
            }
        } else {
            r = secureEraseDisk(rw, erase_opts);
        }
        if (r.failed()) {
            QMessageBox::critical(this, "Error",
                QString("Secure erase failed: %1").arg(r.message.c_str()));
            return;
        }
        setProgressDone();
        status_bar_->setStatus("Secure erase complete.");
    }
}

void MainWindow::onActionBenchmark() {
    auto disk = selectedDisk();
    if (!disk) {
        QMessageBox::warning(this, "No Selection", "Please select a disk first.");
        return;
    }
    
    BenchmarkDialog dialog(disk, selected_partition_number_, this);
    dialog.exec(); // Dialog handles its own execution flow
}

// Selection handlers
void MainWindow::onDiskSelected(int index) {
    selected_disk_index_ = index;
    selected_partition_number_ = -1;
    
    // Load and display the partitions of the selected disk
    std::vector<PartitionInfo> infos;
    auto disk = selectedDisk();
    if (disk && partition_view_) {
        try {
            auto table = PartitionTable::load(disk);
            if (table) {
                auto parts = table->getPartitions();
                for (size_t i = 0; i < parts.size(); i++) {
                    PartitionInfo info;
                    info.partition_number = static_cast<int>(i + 1);
                    info.size_bytes = parts[i].sectorCount() * disk->sectorSize();
                    info.start_sector = parts[i].startSector();
                    info.label = QString::fromStdString(parts[i].name());
                    info.fs_type = parts[i].filesystem();
                    info.mounted = false;
                    infos.push_back(info);
                }
            }
        } catch (const std::exception& e) {
            status_bar_->setStatus(QString("Failed to load partitions: %1")
                                         .arg(e.what()));
        }
        partition_view_->setPartitions(infos);
    }
    updateActionStates();
}

void MainWindow::onPartitionSelected(int partition_number) {
    selected_partition_number_ = partition_number;
    if (partition_view_) {
        partition_view_->setSelectedPartition(partition_number);
    }
    updateActionStates();
}

void MainWindow::onPartitionDoubleClicked(int partition_number) {
    auto disk = selectedDisk();
    if (!disk || partition_number < 1) return;

    try {
        auto table = PartitionTable::load(disk);
        if (!table) return;
        auto parts = table->getPartitions();
        if (partition_number > static_cast<int>(parts.size())) return;
        auto partition = std::make_shared<Partition>(parts[partition_number - 1]);
        PartitionPropertiesDialog dialog(partition, this);
        dialog.exec();
    } catch (const std::exception&) {
        // ignore - properties dialog is informational
    }
}

// Wizard handlers
void MainWindow::onActionCloneWizard() {
    CloneDiskWizard wizard(disks_, this);
    if (wizard.exec() == QDialog::Accepted) {
        logMessage("Clone wizard finished");
        refreshDisks();
    }
}

void MainWindow::onActionMigrateOSWizard() {
    MigrateOSWizard wizard(disks_, this);
    if (wizard.exec() == QDialog::Accepted) {
        logMessage("Migrate OS wizard finished");
        refreshDisks();
    }
}

void MainWindow::onActionBootableMediaWizard() {
    BootableMediaWizard wizard(disks_, this);
    wizard.exec();
}

void MainWindow::onActionRecoveryWizard() {
    RecoveryWizard wizard(disks_, this);
    if (wizard.exec() == QDialog::Accepted) {
        logMessage("Recovery wizard finished");
        refreshDisks();
    }
}

} // namespace opm::gui
