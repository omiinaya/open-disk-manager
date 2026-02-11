#include "main_window.hpp"
#include "disk_tree_widget.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

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
    // TODO: Open preferences dialog
}

void MainWindow::onActionCreatePartition() {
    // TODO: Open create partition dialog
}

void MainWindow::onActionDeletePartition() {
    // TODO: Open delete partition dialog
}

void MainWindow::onActionResizePartition() {
    // TODO: Open resize partition dialog
}

void MainWindow::onActionFormatPartition() {
    // TODO: Open format partition dialog
}

void MainWindow::onActionCloneDisk() {
    // TODO: Open clone dialog
}

void MainWindow::onActionSecureErase() {
    // TODO: Open secure erase dialog
}

void MainWindow::onActionBenchmark() {
    // TODO: Open benchmark dialog
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
