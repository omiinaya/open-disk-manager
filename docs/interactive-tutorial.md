# Interactive Tutorial: Getting Started with Open Disk Manager

## Overview
Open Disk Manager is a comprehensive tool for disk management and monitoring.

## Quick Start

### Installation
```bash
npm install open-disk-manager
```

### Basic Usage
```javascript
import { DiskManager } from 'open-disk-manager';

const manager = new DiskManager();
await manager.initialize();
```

### Features
- **Disk Monitoring**: Real-time disk usage statistics
- **Partition Management**: Create, resize, and delete partitions
- **Health Checks**: SMART monitoring and alerts
- **Visual Reports**: Interactive charts and graphs

## Tutorial Steps

### Step 1: Initialize the Manager
```javascript
const config = {
  autoRefresh: true,
  refreshInterval: 5000
};
const manager = new DiskManager(config);
```

### Step 2: Get Disk Information
```javascript
const disks = await manager.getDisks();
console.log(disks);
```

### Step 3: Monitor Disk Health
```javascript
manager.on('healthChange', (event) => {
  console.log(`Disk health changed: ${event.status}`);
});
```

## Advanced Topics
See the full documentation for advanced configuration options.
