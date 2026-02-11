#include <gtest/gtest.h>
#include "opm/boot.hpp"

using namespace opm;

// Test boot config defaults
TEST(BootTest, BootConfigDefaults) {
    BootConfig config;
    EXPECT_EQ(config.distribution_name, "OPM");
    EXPECT_TRUE(config.include_firmware);
    EXPECT_TRUE(config.include_tools);
    EXPECT_FALSE(config.persistent_mode);
    EXPECT_EQ(config.persistence_size, 0);
}

// Test Live USB options defaults
TEST(BootTest, LiveUSBOptionsDefaults) {
    LiveUSBOptions options;
    EXPECT_EQ(options.label, "OPM-LIVE");
    EXPECT_TRUE(options.verify_after_write);
    EXPECT_FALSE(options.compress);
}

// Test boot repair options defaults
TEST(BootTest, BootRepairOptionsDefaults) {
    BootRepairOptions options;
    EXPECT_EQ(options.target, BootRepairTarget::MBR);
    EXPECT_FALSE(options.reinstall_bootloader);
    EXPECT_FALSE(options.fix_partition_table);
    EXPECT_TRUE(options.check_filesystem);
    EXPECT_TRUE(options.backup_before_repair);
}

// Test bootloader options defaults
TEST(BootTest, BootloaderOptionsDefaults) {
    BootloaderOptions options;
    EXPECT_EQ(options.bootloader, "syslinux");
    EXPECT_EQ(options.target_arch, "x86_64");
    EXPECT_TRUE(options.uefi_support);
    EXPECT_TRUE(options.legacy_bios_support);
}

// Test boot mode detection
TEST(BootTest, BootModeDetection) {
    BootMode mode = detectBootMode();
    // Should return either BIOS or UEFI
    EXPECT_TRUE(mode == BootMode::BIOS || mode == BootMode::UEFI || mode == BootMode::Unknown);
}

// Test USB device info structure
TEST(BootTest, USBDeviceInfoStructure) {
    USBDeviceInfo info;
    EXPECT_TRUE(info.device_path.empty());
    EXPECT_TRUE(info.vendor.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_EQ(info.size, 0);
    EXPECT_EQ(info.sector_size, 0);
    EXPECT_FALSE(info.is_removable);
    EXPECT_FALSE(info.is_usb);
}

// Note: Full integration tests for boot operations require actual hardware access
// and are performed manually or in integration test environment.
