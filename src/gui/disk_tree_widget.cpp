#include "disk_tree_widget.hpp"
#include "opm/utils.hpp"
#include "opm/disk_io.hpp"
#include <QHeaderView>
#include <QTreeWidgetItem>

namespace opm::gui {

DiskTreeWidget::DiskTreeWidget(QWidget* parent)
    : QTreeWidget(parent) {
    setupUI();
}

DiskTreeWidget::~DiskTreeWidget() = default;

void DiskTreeWidget::setupUI() {
    setColumnCount(3);
    setHeaderLabels(QStringList() << "Device" << "Size" << "Type");
    header()->setStretchLastSection(true);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSortingEnabled(true);
    
    connect(this, &QTreeWidget::itemSelectionChanged,
            this, &DiskTreeWidget::onItemSelectionChanged);
    connect(this, &QTreeWidget::itemDoubleClicked,
            this, &DiskTreeWidget::onItemDoubleClicked);
}

void DiskTreeWidget::setDisks(const std::vector<std::shared_ptr<DiskIO>>& disks) {
    disks_ = disks;
    populateTree();
}

int DiskTreeWidget::selectedDiskIndex() const {
    auto* item = currentItem();
    if (!item) return -1;
    
    QVariant data = item->data(0, Qt::UserRole);
    return data.toInt();
}

void DiskTreeWidget::refresh() {
    populateTree();
}

void DiskTreeWidget::populateTree() {
    clear();
    
    for (size_t i = 0; i < disks_.size(); ++i) {
        const auto& disk = disks_[i];
        auto* item = new QTreeWidgetItem(this);
        item->setText(0, QString::fromStdString(disk->devicePath()));
        item->setText(1, QString::fromStdString(utils::formatBytes(disk->size())));
        item->setText(2, "HDD");
        item->setData(0, Qt::UserRole, static_cast<int>(i));
    }
}

void DiskTreeWidget::onItemSelectionChanged() {
    int index = selectedDiskIndex();
    if (index >= 0) {
        emit diskSelected(index);
    }
}

void DiskTreeWidget::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    
    QVariant data = item->data(0, Qt::UserRole);
    int index = data.toInt();
    if (index >= 0) {
        emit diskDoubleClicked(index);
    }
}

} // namespace opm::gui
