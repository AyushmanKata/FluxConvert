#include "converter/conversion_manager.hpp"
#include "converter/format_registry.hpp"
#include "converter/job_executor.hpp"
#include "converter/error.hpp"
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace converter {

class ConversionManager::Impl {
public:
    explicit Impl(size_t worker_threads, FormatRegistry* registry = nullptr)
        : executor_(std::make_unique<JobExecutor>(worker_threads)),
          registry_(registry ? registry : &FormatRegistry::instance()) {
        register_builtin_handlers(*registry_);
    }

    size_t get_thread_count() const {
        return executor_->thread_count();
    }

    void set_thread_count(size_t threads) {
        executor_->set_thread_count(threads);
    }

    ConversionResult convert(const ConversionRequest& request) {
        ConversionResult result;
        auto start_time = std::chrono::steady_clock::now();

        // Validate request
        if (!request.validate()) {
            FileConversionResult error_result;
            error_result.success = false;
            error_result.error_message = "Invalid conversion request";
            result.file_results.push_back(error_result);
            return result;
        }

        // Check if this is a multi-file request
        if (request.input_paths.size() > 1) {
            // Special Case: Multiple images combined into a single PDF
            Format first_fmt = detect_format_from_file(request.input_paths[0]);
            bool is_images_to_single_pdf = (request.output_format == Format::PDF) &&
                                           (first_fmt == Format::PNG || first_fmt == Format::JPEG);

            if (is_images_to_single_pdf) {
                auto* handler = registry_->find_handler_for_conversion(
                    first_fmt, request.output_format);
                if (handler) {
                    try {
                        std::string target_path = request.output_path;
                        if (fs::is_directory(target_path) || (!target_path.empty() && (target_path.back() == '/' || target_path.back() == '\\'))) {
                            target_path = (fs::path(target_path) / "combined.pdf").string();
                        }
                        auto file_result = handler->convert_multiple(
                            request.input_paths,
                            target_path,
                            request
                        );
                        result.file_results.push_back(file_result);
                    } catch (const ConversionError& e) {
                        FileConversionResult error_result;
                        error_result.success = false;
                        error_result.error_message = e.what();
                        result.file_results.push_back(error_result);
                    } catch (const std::exception& e) {
                        FileConversionResult error_result;
                        error_result.success = false;
                        error_result.error_message = e.what();
                        result.file_results.push_back(error_result);
                    }
                    auto end_time = std::chrono::steady_clock::now();
                    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time);
                    result.all_succeeded = (result.failure_count() == 0);
                    return result;
                }
            }

            // General Batch Conversion: execute files concurrently via JobExecutor
            auto* reg = registry_;
            auto convert_single_fn = [reg](
                const std::string& in_path,
                const std::string& out_path,
                const ConversionRequest& file_req) -> FileConversionResult {
                Format in_fmt = detect_format_from_file(in_path);
                if (in_fmt == Format::Unknown) {
                    FileConversionResult err;
                    err.input_path = in_path;
                    err.output_path = out_path;
                    err.success = false;
                    err.error_message = "Unknown input format";
                    return err;
                }

                auto* handler = reg->find_handler_for_conversion(
                    in_fmt, file_req.output_format);
                if (!handler) {
                    FileConversionResult err;
                    err.input_path = in_path;
                    err.output_path = out_path;
                    err.success = false;
                    err.error_message = "Unsupported conversion: " +
                        std::string(format_to_string(in_fmt)) + " to " +
                        std::string(format_to_string(file_req.output_format));
                    return err;
                }

                return handler->convert(in_path, out_path, file_req);
            };

            return executor_->execute_batch(request, convert_single_fn);
        }

        // Single file conversion
        Format input_format = detect_format_from_file(request.input_paths[0]);
        if (input_format == Format::Unknown) {
            FileConversionResult error_result;
            error_result.input_path = request.input_paths[0];
            error_result.success = false;
            error_result.error_message = "Unknown input format";
            result.file_results.push_back(error_result);
            return result;
        }

        auto* handler = registry_->find_handler_for_conversion(
            input_format, request.output_format);

        if (!handler) {
            FileConversionResult error_result;
            error_result.input_path = request.input_paths[0];
            error_result.success = false;
            error_result.error_message = "Unsupported conversion: " +
                std::string(format_to_string(input_format)) + " to " +
                std::string(format_to_string(request.output_format));
            result.file_results.push_back(error_result);
            return result;
        }

        try {
            auto file_result = handler->convert(
                request.input_paths[0],
                request.output_path,
                request
            );
            result.file_results.push_back(file_result);
        } catch (const ConversionError& e) {
            FileConversionResult error_result;
            error_result.input_path = request.input_paths[0];
            error_result.success = false;
            error_result.error_message = e.what();
            result.file_results.push_back(error_result);
        } catch (const std::exception& e) {
            FileConversionResult error_result;
            error_result.input_path = request.input_paths[0];
            error_result.success = false;
            error_result.error_message = e.what();
            result.file_results.push_back(error_result);
        }

        auto end_time = std::chrono::steady_clock::now();
        result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        result.all_succeeded = (result.failure_count() == 0);

        return result;
    }

    bool is_supported(Format from, Format to) const {
        return registry_->is_conversion_supported(from, to);
    }

    std::vector<Format> get_supported_outputs(Format input) const {
        std::vector<Format> outputs;
        for (auto format : {Format::PNG, Format::JPEG, Format::PDF}) {
            if (format != input && is_supported(input, format)) {
                outputs.push_back(format);
            }
        }
        return outputs;
    }

private:
    std::unique_ptr<JobExecutor> executor_;
    FormatRegistry* registry_;
};

ConversionManager::ConversionManager(size_t worker_threads, FormatRegistry* registry)
    : impl_(std::make_unique<Impl>(worker_threads, registry)) {}

ConversionManager::~ConversionManager() = default;

size_t ConversionManager::get_thread_count() const {
    return impl_->get_thread_count();
}

void ConversionManager::set_thread_count(size_t threads) {
    impl_->set_thread_count(threads);
}

ConversionResult ConversionManager::convert(const ConversionRequest& request) {
    return impl_->convert(request);
}

bool ConversionManager::is_supported(Format from, Format to) const {
    return impl_->is_supported(from, to);
}

std::vector<Format> ConversionManager::get_supported_outputs(Format input) const {
    return impl_->get_supported_outputs(input);
}

} // namespace converter
