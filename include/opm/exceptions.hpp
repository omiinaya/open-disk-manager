#pragma once

#include <stdexcept>
#include <string>

namespace opm {

// Base exception
class OpmException : public std::runtime_error {
public:
    explicit OpmException(const std::string& msg) : std::runtime_error(msg) {}
};

// Device-related exceptions
class DeviceException : public OpmException {
public:
    explicit DeviceException(const std::string& msg) : OpmException(msg) {}
};

class DeviceNotFoundException : public DeviceException {
public:
    explicit DeviceNotFoundException(const std::string& device) 
        : DeviceException("Device not found: " + device) {}
};

class DeviceBusyException : public DeviceException {
public:
    explicit DeviceBusyException(const std::string& device)
        : DeviceException("Device is busy: " + device) {}
};

class PermissionDeniedException : public DeviceException {
public:
    explicit PermissionDeniedException(const std::string& device)
        : DeviceException("Permission denied: " + device) {}
};

// Partition-related exceptions
class PartitionException : public OpmException {
public:
    explicit PartitionException(const std::string& msg) : OpmException(msg) {}
};

class InvalidPartitionException : public PartitionException {
public:
    explicit InvalidPartitionException(const std::string& msg)
        : PartitionException("Invalid partition: " + msg) {}
};

class PartitionNotFoundException : public PartitionException {
public:
    explicit PartitionNotFoundException(int number)
        : PartitionException("Partition not found: " + std::to_string(number)) {}
};

// File system exceptions
class FilesystemException : public OpmException {
public:
    explicit FilesystemException(const std::string& msg) : OpmException(msg) {}
};

class UnsupportedFilesystemException : public FilesystemException {
public:
    explicit UnsupportedFilesystemException(const std::string& fs)
        : FilesystemException("Unsupported filesystem: " + fs) {}
};

class CorruptedFilesystemException : public FilesystemException {
public:
    explicit CorruptedFilesystemException(const std::string& msg)
        : FilesystemException("Corrupted filesystem: " + msg) {}
};

// I/O exceptions
class IOException : public OpmException {
public:
    explicit IOException(const std::string& msg) : OpmException(msg) {}
};

class ReadException : public IOException {
public:
    explicit ReadException(const std::string& msg) : IOException("Read error: " + msg) {}
};

class WriteException : public IOException {
public:
    explicit WriteException(const std::string& msg) : IOException("Write error: " + msg) {}
};

class SeekException : public IOException {
public:
    explicit SeekException(const std::string& msg) : IOException("Seek error: " + msg) {}
};

// Validation exceptions
class ValidationException : public OpmException {
public:
    explicit ValidationException(const std::string& msg) : OpmException(msg) {}
};

class ChecksumException : public ValidationException {
public:
    explicit ChecksumException(const std::string& msg)
        : ValidationException("Checksum error: " + msg) {}
};

class AlignmentException : public ValidationException {
public:
    explicit AlignmentException(const std::string& msg)
        : ValidationException("Alignment error: " + msg) {}
};

// Operation exceptions
class OperationException : public OpmException {
public:
    explicit OperationException(const std::string& msg) : OpmException(msg) {}
};

class OperationCancelledException : public OperationException {
public:
    OperationCancelledException() : OperationException("Operation cancelled by user") {}
};

class OperationNotSupportedException : public OperationException {
public:
    explicit OperationNotSupportedException(const std::string& op)
        : OperationException("Operation not supported: " + op) {}
};

} // namespace opm
