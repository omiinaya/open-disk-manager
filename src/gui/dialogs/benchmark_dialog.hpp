#pragma once

#include <QDialog>
#include <memory>

class QLabel;
class QComboBox;
class QProgressBar;
class QCheckBox;
class QPushButton;
class QDialogButtonBox;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Dialog for disk/partition benchmarking
class BenchmarkDialog : public QDialog {
    Q_OBJECT

public:
    enum class TestMode {
        Quick,      // Quick test (100 MB)
        Standard, // Standard test (1 GB)
        Extended  // Extended test (10 GB)
    };

    enum class TestType {
        Sequential,
        Random,
        Full
    };

    struct Result {
        double seq_read_speed = 0;    // MB/s
        double seq_write_speed = 0;   // MB/s
        double random_read_iops = 0;  // IOPS
        double random_write_iops = 0; // IOPS
        double avg_read_latency = 0;  // ms
        double avg_write_latency = 0; // ms
    };

    struct Options {
        TestMode mode = TestMode::Standard;
        TestType type = TestType::Full;
        int block_size = 4096; // bytes
        int queue_depth = 1;
        bool read_only = false;
    };

    explicit BenchmarkDialog(std::shared_ptr<DiskIO> disk,
                             int partition_number = -1, // -1 = whole disk
                             QWidget* parent = nullptr);
    ~BenchmarkDialog();

    Options getOptions() const;
    Result getResults() const; // Call after benchmark completes

private slots:
    void onModeChanged(int index);
    void onTypeChanged(int index);
    void onStartClicked();
    void onBlockSizeChanged(int index);
    bool validateInput();

private:
    void setupUI();
    void setupConnections();
    void updateTestDescription();
    
// UI elements
    QLabel* device_label_;
    QComboBox* mode_combo_;
    QComboBox* type_combo_;
    QComboBox* block_size_combo_;
    QLabel* test_description_;
    QProgressBar* progress_bar_;
    QLabel* progress_label_;
    QCheckBox* read_only_checkbox_;
    QLabel* results_label_;
    QPushButton* start_button_;
    QPushButton* save_button_;
    QDialogButtonBox* button_box_;

    // Results display
    QLabel* seq_read_label_;
    QLabel* seq_write_label_;
    QLabel* random_read_label_;
    QLabel* random_write_label_;
    QLabel* latency_label_;
    
    // Data
    std::shared_ptr<DiskIO> disk_;
    int partition_number_;
    Result results_;
    bool test_running_;
};

} // namespace opm::gui
