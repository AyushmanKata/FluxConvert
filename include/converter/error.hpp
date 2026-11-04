#pragma once

#include <string>
#include <stdexcept>
#include <system_error>

namespace converter {

// Exception hierarchy for conversion errors
class ConversionError : public std::runtime_error {
public:
    explicit ConversionError(const std::string& message)
        : std::runtime_error(message) {}
};

class FormatError : public ConversionError {
public:
    explicit FormatError(const std::string& message)
        : ConversionError("Format error: " + message) {}
};

class IOError : public ConversionError {
public:
    explicit IOError(const std::string& message)
        : ConversionError("I/O error: " + message) {}
};

class ValidationError : public ConversionError {
public:
    explicit ValidationError(const std::string& message)
        : ConversionError("Validation error: " + message) {}
};

class ResourceLimitError : public ConversionError {
public:
    explicit ResourceLimitError(const std::string& message)
        : ConversionError("Resource limit exceeded: " + message) {}
};

class CancelledException : public ConversionError {
public:
    CancelledException()
        : ConversionError("Operation cancelled by user") {}
};

} // namespace converter
