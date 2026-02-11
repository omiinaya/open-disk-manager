#pragma once

#include "opm/types.hpp"
#include <QWidget>
#include <vector>
#include <QString>

class QPaintEvent;
class QMouseEvent;

namespace opm::gui {

// Information about a partition for display
struct PartitionInfo {
    int partition_number;
    uint64_t size_bytes;
    uint64_t start_sector;
    QString label;
    FileSystemType fs_type;
    bool mounted;
    QString mount_point;
};

class PartitionViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PartitionViewWidget(QWidget* parent = nullptr);
    ~PartitionViewWidget() override;

    void setPartitions(const std::vector<PartitionInfo>& partitions);
    void setSelectedPartition(int partition_number);
    int selectedPartition() const;

signals:
    void partitionSelected(int partition_number);
    void partitionDoubleClicked(int partition_number);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QColor getPartitionColor(FileSystemType type);
    QString formatSize(uint64_t bytes);

    std::vector<PartitionInfo> partitions_;
    int selected_partition_;
};

} // namespace opm::gui
