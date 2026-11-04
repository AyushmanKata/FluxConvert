#include "converter/tempfile.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace converter {

namespace fs = std::filesystem;

TempFile::TempFile(const std::string& prefix, const std::string& extension) {
    // Generate unique temp filename
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    std::error_code ec;
    fs::path temp_dir = fs::temp_directory_path(ec);
    if (ec) {
        temp_dir = "/tmp";
    }

    for (int i = 0; i < 100; ++i) {
        std::ostringstream oss;
        oss << prefix << "_" << dis(gen) << extension;

        path_ = temp_dir / oss.str();

        // Try to create file exclusively
        std::ofstream file(path_, std::ios::out | std::ios::binary | std::ios::trunc);
        if (file.good()) {
            file.close();
            created_ = true;
            break;
        }
    }
}

TempFile::~TempFile() {
    if (created_ && !keep_) {
        std::error_code ec;
        if (fs::exists(path_, ec)) {
            fs::remove(path_, ec);
        }
    }
}

TempFile::TempFile(TempFile&& other) noexcept
    : path_(std::move(other.path_))
    , created_(other.created_)
    , keep_(other.keep_) {
    other.created_ = false;
}

TempFile& TempFile::operator=(TempFile&& other) noexcept {
    if (this != &other) {
        if (created_ && !keep_) {
            std::error_code ec;
            if (fs::exists(path_, ec)) {
                fs::remove(path_, ec);
            }
        }
        path_ = std::move(other.path_);
        created_ = other.created_;
        keep_ = other.keep_;
        other.created_ = false;
    }
    return *this;
}

} // namespace converter
