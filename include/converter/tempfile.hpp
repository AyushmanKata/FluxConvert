#pragma once

#include <filesystem>
#include <string>

namespace converter {

class TempFile {
public:
    explicit TempFile(const std::string& prefix = "tmp", const std::string& extension = ".tmp");
    ~TempFile();

    const std::filesystem::path& path() const { return path_; }
    void keep() { keep_ = true; }
    bool is_valid() const { return created_; }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    TempFile(TempFile&& other) noexcept;
    TempFile& operator=(TempFile&& other) noexcept;

private:
    std::filesystem::path path_;
    bool created_ = false;
    bool keep_ = false;
};

} // namespace converter
