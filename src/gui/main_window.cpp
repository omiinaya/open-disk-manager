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
#include "opm/partition_table.hpp"
#include "opm/utils.hpp"
#include "opm/disk_io.hpp"
#include "opm/operation.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QApplication>
#include <QInputDialog>
#include <QSettings>

namespace opm::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , main_splitter_(nullptr)
    , disk_tree_(nullptr)
    , partition_view_(nullptr)
    , operation_panel_(nullptr)
    , status_bar_(nullptr) {
    setupUI();
}

MainWindow::~MainWindow() = default;

void MainWindow::initialize() {
    // Load initial disk list
    refreshDisks();
    updateWindowTitle();
}

void MainWindow::refreshDisks() {
    // Placeholder: In real implementation, enumerate actual disks
    // For now, just clear and show empty state
    disks_.clear();
    
    if (disk_tree_) {
        disk_tree_->setDisks(disks_);
    }
    
    updateActionStates();
}

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
    
    // Create placeholder for partition view (center)
    auto* right_panel = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setSpacing(10);
    right_layout->setContentsMargins(10, 10, 10, 10);
    
    // Add placeholder label
    auto* placeholder = new QLabel("Select a disk to view partitions", right_panel);
    placeholder->setAlignment(Qt::AlignCenter);
    right_layout->addWidget(placeholder);
    
    main_splitter_->addWidget(right_panel);
    main_splitter_->setStretchFactor(1, 1);
    
    // Setup menus and toolbars
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupConnections();
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
    statusBar()->showMessage("Ready");
}

void MainWindow::setupConnections() {
    // Connect disk tree signals
    if (disk_tree_) {
        connect(disk_tree_, &DiskTreeWidget::diskSelected,
                this, &MainWindow::onDiskSelected);
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
        "<h2>Open Partition Manager 0.1.0</h2>"
        "<p>An open-source partition management tool for Linux.</p>"
        "<p>Built with Qt and modern C++.</p>");
}

void MainWindow::onActionPreferences() {
    PreferencesDialog dialog(this);
    connect(&dialog, &PreferencesDialog::settingsChanged, [this]() {
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
    
    auto table = PartitionTable::load(disk);
    if (!table) {
        QMessageBox::warning(this, "Error", "Failed to load partition table.");
        return;
    }
    
    auto table_ptr = std::shared_ptr<PartitionTable>(table.release());
    CreatePartitionDialog dialog(disk, table_ptr, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        // Execute create partition operation
        statusBar()->showMessage("Creating partition...");
        // TODO: Implement actual partition creation
        QMessageBox::information(this, "Success", 
            QString("Partition created with type 0x%1 and size %2 MB")
                .arg(options.partition_type, 2, 16, QLatin1Char('0'))
                .arg(options.size_bytes / (1024 * 1024)));
    }
}

void MainWindow::onActionDeletePartition() {
    auto disk = selectedDisk();
    if (!disk || selected_partition_number_ < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a partition first.");
        return;
    }
    
    // Placeholder values - get actual partition info
    uint64_t partition_size = 100ULL * 1024 * 1024 * 1024; // 100GB
    QString partition_type = "Linux (0x83)";
    
    DeletePartitionDialog dialog(disk, selected_partition_number_, 
                                 partition_size, partition_type, this);
    
    if (dialog.exec() == QDialog::Accepted && dialog.confirmed()) {
        // Execute delete partition operation
        statusBar()->showMessage("Deleting partition...");
        // TODO: Implement actual partition deletion
        QMessageBox::information(this, "Success", 
            QString("Partition %1 deleted successfully.").arg(selected_partition_number_));
        
        if (dialog.eraseData()) {
            statusBar()->showMessage("Secure erase in progress...");
            // TODO: Implement secure erase during delete
        }
    }
}

void MainWindow::onActionResizePartition() {
    auto disk = selectedDisk();
    if (!disk || selected_partition_number_ < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a partition first.");
        return;
    }
    
    auto table = PartitionTable::load(disk);
    if (!table) {
        QMessageBox::warning(this, "Error", "Failed to load partition table.");
        return;
    }
    
    auto table_ptr = std::shared_ptr<PartitionTable>(table.release());
    ResizePartitionDialog dialog(disk, table_ptr, selected_partition_number_, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        // Execute resize partition operation
        statusBar()->showMessage("Resizing partition...");
        // TODO: Implement actual partition resize
        QMessageBox::information(this, "Success", 
            QString("Partition %1 resized to %2 GB")
                .arg(selected_partition_number_)
                .arg(options.new_size_bytes / (1024 * 1024 * 1024)));
    }
}

void MainWindow::onActionFormatPartition() {
    auto disk = selectedDisk();
    if (!disk || selected_partition_number_ < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a partition first.");
        return;
    }
    
    FormatDialog dialog(disk, selected_partition_number_, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto options = dialog.getOptions();
        // Execute format operation
        statusBar()->showMessage("Formatting partition...");
        // TODO: Implement actual format
        QMessageBox::information(this, "Success", 
            QString("Partition %1 formatted as %2")
                .arg(selected_partition_number_)
                .arg(options.label));
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
        // Execute clone operation
        statusBar()->showMessage("Cloning...");
        // TODO: Implement actual clone
        QMessageBox::information(this, "Success", 
            QString("Clone operation started from %1 to %2")
                .arg(options.source_path)
                .arg(options.target_path));
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
        // Execute secure erase operation
        statusBar()->showMessage("Secure erase in progress...");
        // TODO: Implement actual secure erase
        QMessageBox::information(this, "Success", 
            QString("Secure erase started using %1 pass(es)")
                .arg(options.passes));
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
    updateActionStates();
}

void MainWindow::onPartitionSelected(int partition_number) {
    selected_partition_number_ = partition_number;
    updateActionStates();
}

void MainWindow::onPartitionDoubleClicked(int /*partition_number*/) {
    // TODO: Open partition properties
}

} // namespace opm::gui
