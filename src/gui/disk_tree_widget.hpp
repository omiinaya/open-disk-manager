#pragma once

#include <QTreeWidget>
#include <memory>
#include <vector>

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Widget to display disk tree
class DiskTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit DiskTreeWidget(QWidget* parent = nullptr);
    ~DiskTreeWidget();

    // Set disks to display
    void setDisks(const std::vector<std::shared_ptr<DiskIO>>& disks);
    
    // Get selected disk index
    int selectedDiskIndex() const;
    
    // Refresh display
    void refresh();

signals:
    void diskSelected(int index);
    void diskDoubleClicked(int index);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void populateTree();
    
    std::vector<std::shared_ptr<DiskIO>> disks_;
};

} // namespace opm::gui
