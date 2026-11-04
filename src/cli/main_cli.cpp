#include <converter/conversion_manager.hpp>
#include <converter/conversion_request.hpp>
#include <converter/conversion_result.hpp>
#include <converter/format.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace converter;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <input-files...> [options]\n\n"
              << "Options:\n"
              << "  -o, --output <path>       Output file path\n"
              << "  --output-dir <dir>        Output directory for batch conversion\n"
              << "  -n, --name <filename>     Custom name for combined or converted output file (default: combined)\n"
              << "  -f, --format <format>     Output format (png, jpeg, jpg, pdf)\n"
              << "  -q, --quality <1-100>     JPEG quality (default: 90)\n"
              << "  -c, --compression <0-9>   PNG compression level (default: 6)\n"
              << "  --dpi <72-600>            PDF rendering DPI (default: 150)\n"
              << "  --page-size <fit|a4|letter> PDF page size (default: fit)\n"
              << "  --margin <mm>             PDF page margin in mm (default: 0)\n"
              << "  --orientation <auto|portrait|landscape> PDF orientation (default: auto)\n"
              << "  -t, --threads <N>         Worker thread count for batch execution (default: auto)\n"
              << "  -h, --help                Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> input_files;
    std::string output_path;
    std::string output_dir;
    std::string custom_name;
    Format output_format = Format::Unknown;
    int quality = 90;
    int compression = 6;
    int dpi = 150;
    PDFWriteOptions::PageSize page_size = PDFWriteOptions::PageSize::FitToImage;
    PDFWriteOptions::Orientation orientation = PDFWriteOptions::Orientation::Auto;
    double margin_mm = 0.0;
    size_t threads = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if ((arg == "-n" || arg == "--name") && i + 1 < argc) {
            custom_name = argv[++i];
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            output_format = extension_to_format(argv[++i]);
        } else if ((arg == "-q" || arg == "--quality") && i + 1 < argc) {
            quality = std::stoi(argv[++i]);
        } else if ((arg == "-c" || arg == "--compression") && i + 1 < argc) {
            compression = std::stoi(argv[++i]);
        } else if (arg == "--dpi" && i + 1 < argc) {
            dpi = std::stoi(argv[++i]);
        } else if (arg == "--margin" && i + 1 < argc) {
            margin_mm = std::stod(argv[++i]);
        } else if (arg == "--page-size" && i + 1 < argc) {
            std::string ps = argv[++i];
            if (ps == "a4" || ps == "A4") page_size = PDFWriteOptions::PageSize::A4;
            else if (ps == "letter" || ps == "Letter") page_size = PDFWriteOptions::PageSize::Letter;
            else page_size = PDFWriteOptions::PageSize::FitToImage;
        } else if (arg == "--orientation" && i + 1 < argc) {
            std::string ori = argv[++i];
            if (ori == "portrait") orientation = PDFWriteOptions::Orientation::Portrait;
            else if (ori == "landscape") orientation = PDFWriteOptions::Orientation::Landscape;
            else orientation = PDFWriteOptions::Orientation::Auto;
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            threads = static_cast<size_t>(std::max(1, std::stoi(argv[++i])));
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        } else {
            input_files.push_back(arg);
        }
    }

    if (input_files.empty()) {
        std::cerr << "Error: No input files specified.\n";
        return 1;
    }

    // Infer output format from output_path if not specified
    if (output_format == Format::Unknown && !output_path.empty()) {
        output_format = extension_to_format(fs::path(output_path).extension().string());
    }

    if (output_format == Format::Unknown) {
        std::cerr << "Error: Output format must be specified with -f/--format or via output file extension.\n";
        return 1;
    }

    ConversionRequest request;
    request.input_paths = input_files;
    request.output_format = output_format;
    request.jpeg_options.quality = quality;
    request.png_options.compression_level = compression;
    request.pdf_read_options.dpi = dpi;
    request.pdf_write_options.page_size = page_size;
    request.pdf_write_options.orientation = orientation;
    request.pdf_write_options.margin_mm = margin_mm;

    if (!output_path.empty()) {
        request.output_path = output_path;
    } else {
        std::string ext;
        switch (output_format) {
            case Format::PNG:  ext = ".png"; break;
            case Format::JPEG: ext = ".jpg"; break;
            case Format::PDF:  ext = ".pdf"; break;
            default: ext = ".out"; break;
        }

        bool is_images_to_pdf = (output_format == Format::PDF);
        if (is_images_to_pdf) {
            for (const auto& in : input_files) {
                auto ext_str = fs::path(in).extension().string();
                if (ext_str == ".pdf" || ext_str == ".PDF") {
                    is_images_to_pdf = false;
                    break;
                }
            }
        }

        if (input_files.size() == 1 && output_format != Format::PDF) {
            std::string filename;
            if (!custom_name.empty()) {
                fs::path p(custom_name);
                filename = p.has_extension() ? p.filename().string() : (p.filename().string() + ext);
            } else {
                auto stem = fs::path(input_files[0]).stem().string();
                filename = stem + ext;
            }
            fs::path dir = !output_dir.empty() ? fs::path(output_dir) : fs::path(input_files[0]).parent_path();
            request.output_path = (dir / filename).string();
        } else if (is_images_to_pdf) {
            std::string filename = !custom_name.empty() ? custom_name : "combined";
            fs::path p(filename);
            filename = p.has_extension() ? p.filename().string() : (p.filename().string() + ".pdf");
            fs::path dir = !output_dir.empty() ? fs::path(output_dir) : fs::current_path();
            request.output_path = (dir / filename).string();
        } else if (!output_dir.empty()) {
            request.output_path = output_dir;
        } else {
            request.output_path = fs::current_path().string();
        }
    }

    request.progress_callback = [](int percentage, const std::string& message) {
        std::cout << "\r[" << percentage << "%] " << message << std::flush;
    };

    ConversionManager manager(threads);
    std::cout << "Starting conversion (using " << manager.get_thread_count() << " worker threads)...\n";
    ConversionResult result = manager.convert(request);
    std::cout << "\n";

    if (result.all_succeeded) {
        std::cout << "Success: " << result.success_count() << " file(s) converted in "
                  << result.total_time.count() << " ms.\n";
        for (const auto& fr : result.file_results) {
            std::cout << "  " << fr.input_path << " -> " << fr.output_path << "\n";
        }
        return 0;
    } else {
        std::cerr << "Conversion finished with errors (" << result.failure_count() << " failed):\n";
        for (const auto& fr : result.file_results) {
            if (!fr.success) {
                std::cerr << "  " << fr.input_path << ": " << fr.error_message << "\n";
            }
        }
        return 1;
    }
}
