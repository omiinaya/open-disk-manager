#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QSplitter>
#include <memory>
#include <vector>

class QDockWidget;
class QPlainTextEdit;

namespace opm {
class PartitionTable;
class DiskIO;
}

namespace opm::gui {

// Forward declarations
class DiskTreeWidget;
class PartitionViewWidget;
class OperationPanelWidget;
class StatusBarWidget;

// Main application window
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    // Initialize the application
    void initialize();

    // Append a timestamped line to the log dock (and mirror it in the status bar).
    void logMessage(const QString& message);

    // Refresh disk information
    void refreshDisks();

    // Get selected disk/partition
    std::shared_ptr<DiskIO> selectedDisk() const;
    int selectedPartition() const;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Menu actions
    void onActionRefresh();
    void onActionQuit();
    void onActionAbout();
    void onActionPreferences();
    
    // Disk operations
    void onActionCreatePartition();
    void onActionDeletePartition();
    void onActionResizePartition();
    void onActionFormatPartition();
    void onActionCloneDisk();
    void onActionSecureErase();
    void onActionBenchmark();
    
    // View operations
    void onDiskSelected(int index);
    void onPartitionSelected(int partition_number);
    void onPartitionDoubleClicked(int partition_number);

    // Wizards
    void onActionCloneWizard();
    void onActionMigrateOSWizard();
    void onActionBootableMediaWizard();
    void onActionRecoveryWizard();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    void setupLogDock();
    
    void updateWindowTitle();
    void updateActionStates();
    void applyTheme(bool dark);

    // Report operation progress through the status bar widget.
    void setProgress(uint64_t done, uint64_t total);
    void setProgressDone();
    
    // Widgets
    QSplitter* main_splitter_;
    DiskTreeWidget* disk_tree_;
    PartitionViewWidget* partition_view_;
    OperationPanelWidget* operation_panel_;
    StatusBarWidget* status_bar_;
    QDockWidget* log_dock_;
    QPlainTextEdit* log_view_;
    
    // Data
    std::vector<std::shared_ptr<DiskIO>> disks_;
    int selected_disk_index_ = -1;
    int selected_partition_number_ = -1;
    
    // Actions
    QAction* action_refresh_;
    QAction* action_quit_;
    QAction* action_about_;
    QAction* action_preferences_;
    QAction* action_view_log_;
    QAction* action_create_partition_;
    QAction* action_delete_partition_;
    QAction* action_resize_partition_;
    QAction* action_format_partition_;
    QAction* action_clone_disk_;
    QAction* action_secure_erase_;
    QAction* action_benchmark_;
    QAction* action_wizard_clone_;
    QAction* action_wizard_migrate_;
    QAction* action_wizard_bootable_;
    QAction* action_wizard_recovery_;
};

} // namespace opm::gui
