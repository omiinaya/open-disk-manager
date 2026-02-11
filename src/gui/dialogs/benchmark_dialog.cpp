#include "benchmark_dialog.hpp"
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
#include <QMessageBox>
#include <QFileDialog>

namespace opm::gui {

BenchmarkDialog::BenchmarkDialog(std::shared_ptr<DiskIO> disk,
                                   int partition_number,
                                   QWidget* parent)
    : QDialog(parent)
    , disk_(disk)
    , partition_number_(partition_number)
    , test_running_(false) {
    setWindowTitle("Disk Benchmark");
    setMinimumWidth(550);
    setupUI();
    setupConnections();
    validateInput();
}

BenchmarkDialog::~BenchmarkDialog() = default;

void BenchmarkDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(15);
    
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
    
    // Test settings
    auto* settings_group = new QGroupBox("Test Settings", this);
    auto* settings_layout = new QGridLayout(settings_group);
    
    settings_layout->addWidget(new QLabel("Test Mode:", this), 0, 0);
    mode_combo_ = new QComboBox(this);
    mode_combo_->addItem("Quick (100 MB)", static_cast<int>(TestMode::Quick));
    mode_combo_->addItem("Standard (1 GB)", static_cast<int>(TestMode::Standard));
    mode_combo_->addItem("Extended (10 GB)", static_cast<int>(TestMode::Extended));
    mode_combo_->setCurrentIndex(1);
    settings_layout->addWidget(mode_combo_, 0, 1);
    
    settings_layout->addWidget(new QLabel("Test Type:", this), 1, 0);
    type_combo_ = new QComboBox(this);
    type_combo_->addItem("Sequential Only", static_cast<int>(TestType::Sequential));
    type_combo_->addItem("Random Access Only", static_cast<int>(TestType::Random));
    type_combo_->addItem("Full Test (Sequential + Random)", static_cast<int>(TestType::Full));
    type_combo_->setCurrentIndex(2);
    settings_layout->addWidget(type_combo_, 1, 1);
    
    settings_layout->addWidget(new QLabel("Block Size:", this), 2, 0);
    block_size_combo_ = new QComboBox(this);
    block_size_combo_->addItem("512 bytes", 512);
    block_size_combo_->addItem("4 KB", 4096);
    block_size_combo_->addItem("64 KB", 65536);
    block_size_combo_->addItem("1 MB", 1048576);
    block_size_combo_->setCurrentIndex(1);
    settings_layout->addWidget(block_size_combo_, 2, 1);
    
    read_only_checkbox_ = new QCheckBox("Read-only test (safer)", this);
    read_only_checkbox_->setChecked(false);
    settings_layout->addWidget(read_only_checkbox_, 3, 0, 1, 2);
    
    test_description_ = new QLabel(this);
    test_description_->setWordWrap(true);
    test_description_->setStyleSheet("color: #666666; font-style: italic;");
    settings_layout->addWidget(test_description_, 4, 0, 1, 2);
    
    main_layout->addWidget(settings_group);
    
    // Progress
    auto* progress_layout = new QVBoxLayout();
    progress_label_ = new QLabel("Ready to start", this);
    progress_layout->addWidget(progress_label_);
    
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_layout->addWidget(progress_bar_);
    
    main_layout->addLayout(progress_layout);
    
    // Results
    auto* results_group = new QGroupBox("Results", this);
    auto* results_layout = new QGridLayout(results_group);
    
    results_layout->addWidget(new QLabel("Sequential Read:", this), 0, 0);
    seq_read_label_ = new QLabel("--", this);
    seq_read_label_->setStyleSheet("font-weight: bold;");
    results_layout->addWidget(seq_read_label_, 0, 1);
    results_layout->addWidget(new QLabel("MB/s", this), 0, 2);
    
    results_layout->addWidget(new QLabel("Sequential Write:", this), 1, 0);
    seq_write_label_ = new QLabel("--", this);
    seq_write_label_->setStyleSheet("font-weight: bold;");
    results_layout->addWidget(seq_write_label_, 1, 1);
    results_layout->addWidget(new QLabel("MB/s", this), 1, 2);
    
    results_layout->addWidget(new QLabel("Random Read:", this), 2, 0);
    random_read_label_ = new QLabel("--", this);
    random_read_label_->setStyleSheet("font-weight: bold;");
    results_layout->addWidget(random_read_label_, 2, 1);
    results_layout->addWidget(new QLabel("IOPS", this), 2, 2);
    
    results_layout->addWidget(new QLabel("Random Write:", this), 3, 0);
    random_write_label_ = new QLabel("--", this);
    random_write_label_->setStyleSheet("font-weight: bold;");
    results_layout->addWidget(random_write_label_, 3, 1);
    results_layout->addWidget(new QLabel("IOPS", this), 3, 2);
    
    results_layout->addWidget(new QLabel("Avg Latency:", this), 4, 0);
    latency_label_ = new QLabel("--", this);
    latency_label_->setStyleSheet("font-weight: bold;");
    results_layout->addWidget(latency_label_, 4, 1);
    results_layout->addWidget(new QLabel("ms", this), 4, 2);
    
    main_layout->addWidget(results_group);
    
    // Buttons
    button_box_ = new QDialogButtonBox(QDialogButtonBox::Close, this);
    
    start_button_ = new QPushButton("Start Benchmark", this);
    start_button_->setStyleSheet(
        "QPushButton { color: white; background-color: #0066CC; padding: 8px 16px; }");
    button_box_->addButton(start_button_, QDialogButtonBox::ActionRole);
    
    save_button_ = new QPushButton("Save Results...", this);
    save_button_->setEnabled(false);
    button_box_->addButton(save_button_, QDialogButtonBox::ActionRole);
    
    main_layout->addWidget(button_box_);
}

void BenchmarkDialog::setupConnections() {
    connect(button_box_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(start_button_, &QPushButton::clicked, this, &BenchmarkDialog::onStartClicked);
    connect(save_button_, &QPushButton::clicked, this, [this]() {
        QString file_path = QFileDialog::getSaveFileName(this, "Save Benchmark Results", 
            "benchmark_results.txt", "Text Files (*.txt);;All Files (*)");
        if (!file_path.isEmpty()) {
            // Save results to file
        }
    });
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BenchmarkDialog::onModeChanged);
    connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BenchmarkDialog::onTypeChanged);
    connect(block_size_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BenchmarkDialog::onBlockSizeChanged);
    connect(read_only_checkbox_, &QCheckBox::stateChanged,
            this, &BenchmarkDialog::validateInput);
}

void BenchmarkDialog::updateTestDescription() {
    auto mode = static_cast<TestMode>(mode_combo_->currentData().toInt());
    auto type = static_cast<TestType>(type_combo_->currentData().toInt());
    
    QString description;
    
    switch (mode) {
        case TestMode::Quick:
            description = "Quick test using 100 MB sample. ";
            break;
        case TestMode::Standard:
            description = "Standard test using 1 GB sample. ";
            break;
        case TestMode::Extended:
            description = "Extended test using 10 GB sample. More accurate but takes longer. ";
            break;
    }
    
    switch (type) {
        case TestType::Sequential:
            description += "Tests sequential read/write performance.";
            break;
        case TestType::Random:
            description += "Tests random access performance (IOPS and latency).";
            break;
        case TestType::Full:
            description += "Comprehensive test covering both sequential and random performance.";
            break;
    }
    
    if (read_only_checkbox_->isChecked()) {
        description += " (Read-only mode - safer but no write results)";
    }
    
    test_description_->setText(description);
}

void BenchmarkDialog::onModeChanged(int index) {
    (void)index;
    updateTestDescription();
}

void BenchmarkDialog::onTypeChanged(int index) {
    (void)index;
    updateTestDescription();
}

void BenchmarkDialog::onBlockSizeChanged(int index) {
    (void)index;
    updateTestDescription();
}

void BenchmarkDialog::onStartClicked() {
    if (test_running_) {
        return;
    }
    
    // Show warning if not read-only
    if (!read_only_checkbox_->isChecked()) {
        QMessageBox::StandardButton reply = QMessageBox::warning(this, "Warning",
            "This benchmark will write test data to the disk. "
            "While the test data is temporary, there is a small risk of data loss.\n\n"
            "Consider using read-only mode for important drives.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    test_running_ = true;
    start_button_->setEnabled(false);
    start_button_->setText("Running...");
    progress_label_->setText("Benchmark in progress...");
    
    // Simulate benchmark (in real implementation, this would run the actual benchmark)
    // For now, just show mock results after a delay
    
    // Update UI with mock results
    results_.seq_read_speed = 245.6;
    results_.seq_write_speed = 230.4;
    results_.random_read_iops = 8500;
    results_.random_write_iops = 7200;
    results_.avg_read_latency = 2.3;
    results_.avg_write_latency = 3.1;
    
    seq_read_label_->setText(QString::number(results_.seq_read_speed, 'f', 1));
    seq_write_label_->setText(QString::number(results_.seq_write_speed, 'f', 1));
    random_read_label_->setText(QString::number(results_.random_read_iops));
    random_write_label_->setText(QString::number(results_.random_write_iops));
    latency_label_->setText(QString::number(results_.avg_read_latency, 'f', 1));
    
    progress_bar_->setValue(100);
    progress_label_->setText("Benchmark complete!");
    
    test_running_ = false;
    start_button_->setEnabled(true);
    start_button_->setText("Start Benchmark");
    save_button_->setEnabled(true);
}

bool BenchmarkDialog::validateInput() {
    // Validation logic
    return true;
}

BenchmarkDialog::Options BenchmarkDialog::getOptions() const {
    Options options;
    
    options.mode = static_cast<TestMode>(mode_combo_->currentData().toInt());
    options.type = static_cast<TestType>(type_combo_->currentData().toInt());
    options.block_size = block_size_combo_->currentData().toInt();
    options.read_only = read_only_checkbox_->isChecked();
    
    return options;
}

BenchmarkDialog::Result BenchmarkDialog::getResults() const {
    return results_;
}

} // namespace opm::gui
