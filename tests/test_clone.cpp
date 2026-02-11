#include <gtest/gtest.h>
#include "opm/clone.hpp"
#include <vector>
#include <cstring>

using namespace opm;

// Test that CloneOptions has default values
TEST(CloneTest, CloneOptionsDefaults) {
    CloneOptions options;
    EXPECT_TRUE(options.verify);
    EXPECT_FALSE(options.compress);
    EXPECT_TRUE(options.sparse);
    EXPECT_FALSE(options.ignore_errors);
    EXPECT_EQ(options.buffer_size, 64 * 1024 * 1024);
}

// Test erase method pass counts
TEST(CloneTest, EraseMethodPasses) {
    // Note: These are internal implementation details
    // Just verifying the erase methods exist
    EraseOptions options;
    options.method = EraseMethod::Zeros;
    EXPECT_EQ(options.buffer_size, 64 * 1024 * 1024);
    
    options.method = EraseMethod::DoD522022;
    EXPECT_EQ(options.buffer_size, 64 * 1024 * 1024);
    
    options.method = EraseMethod::Gutmann;
    EXPECT_EQ(options.buffer_size, 64 * 1024 * 1024);
}

// Test benchmark options defaults
TEST(CloneTest, BenchmarkOptionsDefaults) {
    BenchmarkOptions options;
    EXPECT_EQ(options.test_size, 1024 * 1024 * 1024); // 1GB
    EXPECT_EQ(options.block_size, 4096);
    EXPECT_EQ(options.duration_seconds, 10);
    EXPECT_FALSE(options.quick_test);
}

// Test copy options defaults
TEST(CloneTest, CopyOptionsDefaults) {
    CopyOptions options;
    EXPECT_FALSE(options.resize_to_fit);
    EXPECT_TRUE(options.verify);
    EXPECT_EQ(options.buffer_size, 64 * 1024 * 1024);
}

// Test benchmark result structure
TEST(CloneTest, BenchmarkResultStructure) {
    BenchmarkResult result;
    EXPECT_EQ(result.sequential_read_mbps, 0.0);
    EXPECT_EQ(result.sequential_write_mbps, 0.0);
    EXPECT_EQ(result.random_read_iops, 0.0);
    EXPECT_EQ(result.random_write_iops, 0.0);
    EXPECT_EQ(result.latency_ms, 0.0);
    EXPECT_EQ(result.buffer_size, 0);
}

// Note: Full integration tests for clone operations require actual disk access
// and are performed manually or in integration test environment.
// The clone operations depend on DiskIO which requires real device access.
