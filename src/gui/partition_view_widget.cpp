#include "partition_view_widget.hpp"
#include "opm/types.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QLabel>

namespace opm::gui {

PartitionViewWidget::PartitionViewWidget(QWidget* parent)
    : QWidget(parent)
    , selected_partition_(-1) {
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

PartitionViewWidget::~PartitionViewWidget() = default;

void PartitionViewWidget::setPartitions(const std::vector<PartitionInfo>& partitions) {
    partitions_ = partitions;
    selected_partition_ = -1;
    update();
}

void PartitionViewWidget::setSelectedPartition(int partition_number) {
    selected_partition_ = partition_number;
    update();
}

int PartitionViewWidget::selectedPartition() const {
    return selected_partition_;
}

void PartitionViewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    painter.fillRect(rect(), QColor(240, 240, 240));
    
    // Draw border
    painter.setPen(QPen(Qt::gray, 2));
    painter.drawRect(rect().adjusted(1, 1, -2, -2));
    
    // Draw partitions
    if (partitions_.empty()) {
        // Draw "Unallocated" text
        painter.setPen(Qt::darkGray);
        painter.drawText(rect(), Qt::AlignCenter, "Unallocated Space");
    } else {
        // Calculate total size
        uint64_t total_size = 0;
        for (const auto& part : partitions_) {
            total_size += part.size_bytes;
        }
        
        // Draw partition bars
        int x = 10;
        int width = rect().width() - 20;
        int height = rect().height() - 20;
        int y = 10;
        
        for (size_t i = 0; i < partitions_.size(); ++i) {
            const auto& part = partitions_[i];
            int part_width = static_cast<int>(width * part.size_bytes / total_size);
            if (part_width < 5) part_width = 5;
            
            // Draw partition bar
            QColor color = getPartitionColor(part.fs_type);
            painter.fillRect(x, y, part_width - 2, height, color);
            
            // Draw border
            painter.setPen((static_cast<int>(i) == selected_partition_) 
                ? QPen(Qt::blue, 3) : QPen(Qt::black, 1));
            painter.drawRect(x, y, part_width - 2, height);
            
            // Draw label if enough space
            if (part_width > 50) {
                painter.setPen(Qt::white);
                QString label = QString("%1\n%2")
                    .arg(part.label.isEmpty() ? QString("Partition %1").arg(part.partition_number) : part.label)
                    .arg(formatSize(part.size_bytes));
                painter.drawText(x, y, part_width - 2, height, Qt::AlignCenter, label);
            }
            
            x += part_width;
        }
    }
}

void PartitionViewWidget::mousePressEvent(QMouseEvent* event) {
    // Calculate which partition was clicked
    if (partitions_.empty()) {
        selected_partition_ = -1;
        emit partitionSelected(selected_partition_);
        update();
        return;
    }
    
    uint64_t total_size = 0;
    for (const auto& part : partitions_) {
        total_size += part.size_bytes;
    }
    
    int x = 10;
    int width = rect().width() - 20;
    int click_x = event->pos().x();
    
    for (size_t i = 0; i < partitions_.size(); ++i) {
        const auto& part = partitions_[i];
        int part_width = static_cast<int>(width * part.size_bytes / total_size);
        if (part_width < 5) part_width = 5;
        
        if (click_x >= x && click_x < x + part_width) {
            selected_partition_ = part.partition_number;
            emit partitionSelected(selected_partition_);
            emit partitionDoubleClicked(selected_partition_);
            update();
            return;
        }
        
        x += part_width;
    }
    
    selected_partition_ = -1;
    emit partitionSelected(selected_partition_);
    update();
}

QColor PartitionViewWidget::getPartitionColor(FileSystemType type) {
    switch (type) {
        case FileSystemType::FAT32:
            return QColor(100, 149, 237); // Cornflower blue
        case FileSystemType::NTFS:
            return QColor(60, 179, 113); // Medium sea green
        case FileSystemType::EXT4:
            return QColor(255, 140, 0); // Dark orange
        case FileSystemType::exFAT:
            return QColor(147, 112, 219); // Medium purple
        case FileSystemType::Swap:
            return QColor(255, 99, 71); // Tomato
        case FileSystemType::EFI:
            return QColor(30, 144, 255); // Dodger blue
        default:
            return QColor(169, 169, 169); // Dark gray
    }
}

QString PartitionViewWidget::formatSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unit]);
}

} // namespace opm::gui
