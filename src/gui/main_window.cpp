#include "main_window.hpp"
#include <converter/format.hpp>
#include <converter/resource_limits.hpp>

#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/alertdialog.h>
#include <giomm/file.h>
#include <giomm/liststore.h>
#include <glibmm/main.h>

#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fs = std::filesystem;

namespace converter {

// ── Constructor / Destructor ────────────────────────────────────────────────

MainWindow::MainWindow() {
    set_title("FluxConvert");
    set_default_size(600, 700);

    // Set default output directory to user's home
    const char* home = std::getenv("HOME");
    output_directory_ = home ? std::string(home) : "/tmp";

    // Connect dispatchers (thread-safe UI updates)
    dispatcher_progress_.connect(
        sigc::mem_fun(*this, &MainWindow::on_progress_update));
    dispatcher_done_.connect(
        sigc::mem_fun(*this, &MainWindow::on_conversion_done));

    build_ui();
    set_ui_state_idle();
}

MainWindow::~MainWindow() {
    // Cancel any running conversion
    if (cancel_token_) {
        cancel_token_->cancel();
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

// ── UI Construction ─────────────────────────────────────────────────────────

void MainWindow::build_ui() {
    build_header_bar();

    auto& main_box = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::VERTICAL, 12);
    main_box.set_margin(16);

    build_file_section(main_box);
    main_box.append(*Gtk::make_managed<Gtk::Separator>());
    build_output_section(main_box);
    build_options_section(main_box);
    main_box.append(*Gtk::make_managed<Gtk::Separator>());
    build_progress_section(main_box);
    build_action_buttons(main_box);

    set_child(main_box);
}

void MainWindow::build_header_bar() {
    header_bar_.set_show_title_buttons(true);
    set_titlebar(header_bar_);
}

void MainWindow::build_file_section(Gtk::Box& parent) {
    // File list view (with drag-and-drop)
    file_list_view_.signal_list_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_file_list_changed));
    parent.append(file_list_view_);

    // Button row: Add Files + Clear All
    auto& button_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);

    add_files_button_.set_label("Add Files");
    add_files_button_.set_icon_name("list-add-symbolic");
    add_files_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_add_files_clicked));
    button_row.append(add_files_button_);

    // Spacer
    auto& spacer = *Gtk::make_managed<Gtk::Label>("");
    spacer.set_hexpand(true);
    button_row.append(spacer);

    clear_button_.set_label("Clear All");
    clear_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_clear_clicked));
    button_row.append(clear_button_);

    parent.append(button_row);
}

void MainWindow::build_output_section(Gtk::Box& parent) {
    auto& section_label = *Gtk::make_managed<Gtk::Label>();
    section_label.set_markup("<b>Output Settings</b>");
    section_label.set_halign(Gtk::Align::START);
    parent.append(section_label);

    // Format row
    auto& format_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);

    auto& format_label = *Gtk::make_managed<Gtk::Label>("Format:");
    format_label.set_width_chars(10);
    format_label.set_halign(Gtk::Align::START);
    format_row.append(format_label);

    // Format dropdown - populate dynamically
    format_list_ = Gtk::StringList::create({});
    format_dropdown_.set_model(format_list_);
    format_dropdown_.set_hexpand(false);
    format_dropdown_.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &MainWindow::on_format_changed));
    format_row.append(format_dropdown_);

    // Spacer
    auto& spacer1 = *Gtk::make_managed<Gtk::Label>("");
    spacer1.set_hexpand(true);
    format_row.append(spacer1);

    parent.append(format_row);

    // File name row
    auto& name_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);

    auto& name_label = *Gtk::make_managed<Gtk::Label>("File name:");
    name_label.set_width_chars(10);
    name_label.set_halign(Gtk::Align::START);
    name_row.append(name_label);

    file_name_entry_.set_text("combined");
    file_name_entry_.set_placeholder_text("combined");
    file_name_entry_.set_hexpand(true);
    file_name_entry_.set_tooltip_text("Custom name for combined or converted output file (default: combined)");
    name_row.append(file_name_entry_);

    parent.append(name_row);

    // Output directory row
    auto& dir_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);

    auto& dir_label = *Gtk::make_managed<Gtk::Label>("Save to:");
    dir_label.set_width_chars(10);
    dir_label.set_halign(Gtk::Align::START);
    dir_row.append(dir_label);

    output_dir_label_.set_text(output_directory_);
    output_dir_label_.set_hexpand(true);
    output_dir_label_.set_halign(Gtk::Align::START);
    output_dir_label_.set_ellipsize(Pango::EllipsizeMode::START);
    output_dir_label_.set_selectable(true);
    dir_row.append(output_dir_label_);

    browse_button_.set_icon_name("folder-open-symbolic");
    browse_button_.set_tooltip_text("Choose output directory");
    browse_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_browse_output_clicked));
    dir_row.append(browse_button_);

    parent.append(dir_row);
}

void MainWindow::build_options_section(Gtk::Box& parent) {
    options_frame_.set_label("Options");
    options_frame_.set_margin_top(4);

    // Build all option panels
    build_jpeg_options();
    build_png_options();
    build_pdf_write_options();
    build_pdf_read_options();

    // Add to stack
    options_stack_.add(jpeg_options_box_, "jpeg", "JPEG Options");
    options_stack_.add(png_options_box_, "png", "PNG Options");
    options_stack_.add(pdf_write_options_box_, "pdf-write", "PDF Options");
    options_stack_.add(pdf_read_options_box_, "pdf-read", "PDF Read Options");

    // Empty page for when no relevant options
    auto& empty_box = *Gtk::make_managed<Gtk::Box>();
    auto& empty_label = *Gtk::make_managed<Gtk::Label>(
        "No additional options for this conversion");
    empty_label.add_css_class("dim-label");
    empty_label.set_margin(12);
    empty_box.append(empty_label);
    options_stack_.add(empty_box, "none", "No Options");

    options_stack_.set_visible_child("none");
    options_frame_.set_child(options_stack_);
    parent.append(options_frame_);
}

void MainWindow::build_jpeg_options() {
    jpeg_options_box_.set_orientation(Gtk::Orientation::HORIZONTAL);
    jpeg_options_box_.set_spacing(8);
    jpeg_options_box_.add_css_class("options-group");

    auto& label = *Gtk::make_managed<Gtk::Label>("Quality:");
    label.set_width_chars(12);
    label.set_halign(Gtk::Align::START);
    jpeg_options_box_.append(label);

    jpeg_quality_scale_.set_range(1, 100);
    jpeg_quality_scale_.set_value(90);
    jpeg_quality_scale_.set_digits(0);
    jpeg_quality_scale_.set_hexpand(true);
    jpeg_quality_scale_.set_draw_value(false);
    jpeg_quality_scale_.signal_value_changed().connect([this]() {
        jpeg_quality_value_label_.set_text(
            std::to_string(static_cast<int>(jpeg_quality_scale_.get_value())));
    });
    jpeg_options_box_.append(jpeg_quality_scale_);

    jpeg_quality_value_label_.set_text("90");
    jpeg_quality_value_label_.set_width_chars(4);
    jpeg_options_box_.append(jpeg_quality_value_label_);
}

void MainWindow::build_png_options() {
    png_options_box_.set_orientation(Gtk::Orientation::HORIZONTAL);
    png_options_box_.set_spacing(8);
    png_options_box_.add_css_class("options-group");

    auto& label = *Gtk::make_managed<Gtk::Label>("Compression:");
    label.set_width_chars(12);
    label.set_halign(Gtk::Align::START);
    png_options_box_.append(label);

    png_compression_scale_.set_range(0, 9);
    png_compression_scale_.set_value(6);
    png_compression_scale_.set_digits(0);
    png_compression_scale_.set_hexpand(true);
    png_compression_scale_.set_draw_value(false);
    png_compression_scale_.signal_value_changed().connect([this]() {
        png_compression_value_label_.set_text(
            std::to_string(static_cast<int>(png_compression_scale_.get_value())));
    });
    png_options_box_.append(png_compression_scale_);

    png_compression_value_label_.set_text("6");
    png_compression_value_label_.set_width_chars(4);
    png_options_box_.append(png_compression_value_label_);
}

void MainWindow::build_pdf_write_options() {
    pdf_write_options_box_.set_orientation(Gtk::Orientation::VERTICAL);
    pdf_write_options_box_.set_spacing(8);
    pdf_write_options_box_.add_css_class("options-group");

    // Page size row
    auto& size_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto& size_label = *Gtk::make_managed<Gtk::Label>("Page size:");
    size_label.set_width_chars(12);
    size_label.set_halign(Gtk::Align::START);
    size_row.append(size_label);

    page_size_list_ = Gtk::StringList::create(
        {"Fit to Image", "A4", "Letter"});
    pdf_page_size_dropdown_.set_model(page_size_list_);
    pdf_page_size_dropdown_.set_selected(0);
    size_row.append(pdf_page_size_dropdown_);
    pdf_write_options_box_.append(size_row);

    // Orientation row
    auto& orient_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto& orient_label = *Gtk::make_managed<Gtk::Label>("Orientation:");
    orient_label.set_width_chars(12);
    orient_label.set_halign(Gtk::Align::START);
    orient_row.append(orient_label);

    orientation_list_ = Gtk::StringList::create(
        {"Auto", "Portrait", "Landscape"});
    pdf_orientation_dropdown_.set_model(orientation_list_);
    pdf_orientation_dropdown_.set_selected(0);
    orient_row.append(pdf_orientation_dropdown_);
    pdf_write_options_box_.append(orient_row);

    // Margin row
    auto& margin_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 8);
    auto& margin_label = *Gtk::make_managed<Gtk::Label>("Margin (mm):");
    margin_label.set_width_chars(12);
    margin_label.set_halign(Gtk::Align::START);
    margin_row.append(margin_label);

    auto adj = Gtk::Adjustment::create(0.0, 0.0, 50.0, 1.0, 5.0);
    pdf_margin_spin_.set_adjustment(adj);
    pdf_margin_spin_.set_digits(1);
    margin_row.append(pdf_margin_spin_);
    pdf_write_options_box_.append(margin_row);
}

void MainWindow::build_pdf_read_options() {
    pdf_read_options_box_.set_orientation(Gtk::Orientation::HORIZONTAL);
    pdf_read_options_box_.set_spacing(8);
    pdf_read_options_box_.add_css_class("options-group");

    auto& label = *Gtk::make_managed<Gtk::Label>("DPI:");
    label.set_width_chars(12);
    label.set_halign(Gtk::Align::START);
    pdf_read_options_box_.append(label);

    auto adj = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_DPI),
        static_cast<double>(MIN_DPI),
        static_cast<double>(MAX_DPI),
        1.0, 10.0);
    pdf_dpi_spin_.set_adjustment(adj);
    pdf_dpi_spin_.set_digits(0);
    pdf_read_options_box_.append(pdf_dpi_spin_);
}

void MainWindow::build_progress_section(Gtk::Box& parent) {
    progress_box_.set_orientation(Gtk::Orientation::VERTICAL);
    progress_box_.set_spacing(4);

    progress_label_.set_text("Ready");
    progress_label_.set_halign(Gtk::Align::START);
    progress_label_.add_css_class("dim-label");
    progress_box_.append(progress_label_);

    progress_bar_.set_fraction(0.0);
    progress_bar_.set_show_text(false);
    progress_box_.append(progress_bar_);

    parent.append(progress_box_);
}

void MainWindow::build_action_buttons(Gtk::Box& parent) {
    auto& button_row = *Gtk::make_managed<Gtk::Box>(
        Gtk::Orientation::HORIZONTAL, 12);
    button_row.set_halign(Gtk::Align::CENTER);
    button_row.set_margin_top(8);

    convert_button_.set_label("Convert");
    convert_button_.set_icon_name("media-playback-start-symbolic");
    convert_button_.add_css_class("suggested-action");
    convert_button_.set_size_request(140, -1);
    convert_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_convert_clicked));
    button_row.append(convert_button_);

    cancel_button_.set_label("Cancel");
    cancel_button_.set_icon_name("process-stop-symbolic");
    cancel_button_.add_css_class("destructive-action");
    cancel_button_.set_size_request(140, -1);
    cancel_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &MainWindow::on_cancel_clicked));
    button_row.append(cancel_button_);

    parent.append(button_row);
}

// ── Options Panel Logic ─────────────────────────────────────────────────────

bool MainWindow::is_pdf_input() const {
    auto paths = file_list_view_.get_file_paths();
    if (paths.empty()) return false;
    // Check first file — all files should share format for simplicity
    Format fmt = detect_format_from_file(paths[0]);
    return fmt == Format::PDF;
}

void MainWindow::update_options_panel() {
    Format output = get_selected_output_format();
    bool pdf_input = is_pdf_input();

    if (output == Format::Unknown) {
        options_stack_.set_visible_child("none");
    } else if (pdf_input && (output == Format::PNG || output == Format::JPEG)) {
        // PDF → image: show DPI options + output format options
        if (output == Format::JPEG) {
            // Show both DPI and quality? For now, show DPI as primary
            options_stack_.set_visible_child("pdf-read");
        } else {
            options_stack_.set_visible_child("pdf-read");
        }
    } else if (output == Format::JPEG) {
        options_stack_.set_visible_child("jpeg");
    } else if (output == Format::PNG) {
        options_stack_.set_visible_child("png");
    } else if (output == Format::PDF) {
        options_stack_.set_visible_child("pdf-write");
    } else {
        options_stack_.set_visible_child("none");
    }
}

void MainWindow::populate_format_dropdown() {
    // Determine available output formats based on input files
    available_formats_.clear();

    auto paths = file_list_view_.get_file_paths();
    if (paths.empty()) {
        // Default: show all formats
        available_formats_ = {Format::PNG, Format::JPEG, Format::PDF};
    } else {
        // Determine input format from first file
        Format input_fmt = detect_format_from_file(paths[0]);
        if (input_fmt != Format::Unknown) {
            available_formats_ = manager_.get_supported_outputs(input_fmt);
        }
        if (available_formats_.empty()) {
            available_formats_ = {Format::PNG, Format::JPEG, Format::PDF};
        }
    }

    // Rebuild the string list
    // We need to clear and repopulate
    // GTK4 StringList doesn't have a clear() so we rebuild
    auto new_list = Gtk::StringList::create({});
    for (auto fmt : available_formats_) {
        new_list->append(std::string(format_to_string(fmt)));
    }
    format_list_ = new_list;
    format_dropdown_.set_model(format_list_);
    if (!available_formats_.empty()) {
        format_dropdown_.set_selected(0);
    }
}

Format MainWindow::get_selected_output_format() const {
    auto idx = format_dropdown_.get_selected();
    if (idx == GTK_INVALID_LIST_POSITION || idx >= available_formats_.size()) {
        return Format::Unknown;
    }
    return available_formats_[idx];
}

// ── Signal Handlers ─────────────────────────────────────────────────────────

void MainWindow::on_add_files_clicked() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Files");

    auto filters = Gio::ListStore<Gtk::FileFilter>::create();

    auto filter_all_supported = Gtk::FileFilter::create();
    filter_all_supported->set_name("Supported files (PNG, JPEG, PDF)");
    filter_all_supported->add_mime_type("image/png");
    filter_all_supported->add_mime_type("image/jpeg");
    filter_all_supported->add_mime_type("application/pdf");
    filters->append(filter_all_supported);

    auto filter_images = Gtk::FileFilter::create();
    filter_images->set_name("Images (PNG, JPEG)");
    filter_images->add_mime_type("image/png");
    filter_images->add_mime_type("image/jpeg");
    filters->append(filter_images);

    auto filter_pdf = Gtk::FileFilter::create();
    filter_pdf->set_name("PDF files");
    filter_pdf->add_mime_type("application/pdf");
    filters->append(filter_pdf);

    auto filter_any = Gtk::FileFilter::create();
    filter_any->set_name("All files");
    filter_any->add_pattern("*");
    filters->append(filter_any);

    dialog->set_filters(filters);
    dialog->set_default_filter(filter_all_supported);

    dialog->open_multiple(*this, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto files = dialog->open_multiple_finish(result);
            std::vector<std::string> paths;
            paths.reserve(files.size());
            for (const auto& file : files) {
                if (file) {
                    paths.push_back(file->get_path());
                }
            }
            if (!paths.empty()) {
                file_list_view_.add_files(paths);
            }
        } catch (const Glib::Error&) {
            // User cancelled or error occurred
        }
    });
}

void MainWindow::on_clear_clicked() {
    file_list_view_.clear();
}

void MainWindow::on_browse_output_clicked() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Output Directory");

    if (!output_directory_.empty()) {
        dialog->set_initial_folder(Gio::File::create_for_path(output_directory_));
    }

    dialog->select_folder(*this, [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->select_folder_finish(result);
            if (file) {
                output_directory_ = file->get_path();
                output_dir_label_.set_text(output_directory_);
                update_convert_button_sensitivity();
            }
        } catch (const Glib::Error&) {
            // User cancelled or error occurred
        }
    });
}

void MainWindow::on_format_changed() {
    update_options_panel();
    update_convert_button_sensitivity();
}

void MainWindow::on_file_list_changed() {
    populate_format_dropdown();
    update_options_panel();
    update_convert_button_sensitivity();
}

// ── Conversion ──────────────────────────────────────────────────────────────

void MainWindow::on_convert_clicked() {
    if (file_list_view_.is_empty()) return;

    Format output_fmt = get_selected_output_format();
    if (output_fmt == Format::Unknown) return;

    // Ensure output directory exists
    std::error_code ec;
    fs::create_directories(output_directory_, ec);
    if (ec) {
        auto dlg = Gtk::AlertDialog::create("Cannot create output directory:\n" +
            output_directory_ + "\n\n" + ec.message());
        dlg->show(*this);
        return;
    }

    set_ui_state_converting();

    // Start worker thread
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    cancel_token_ = std::make_shared<CancellationToken>();

    worker_thread_ = std::thread([this]() {
        run_conversion();
    });
}

void MainWindow::run_conversion() {
    auto paths = file_list_view_.get_file_paths();
    Format output_fmt = get_selected_output_format();

    ConversionRequest request;
    request.input_paths = paths;
    request.output_format = output_fmt;
    request.cancellation_token = cancel_token_;

    // Set progress callback (called from worker thread → dispatches to UI)
    request.progress_callback = [this](int pct, const std::string& msg) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_percent_ = pct;
            progress_message_ = msg;
        }
        dispatcher_progress_.emit();
    };

    // JPEG options
    request.jpeg_options.quality =
        static_cast<int>(jpeg_quality_scale_.get_value());

    // PNG options
    request.png_options.compression_level =
        static_cast<int>(png_compression_scale_.get_value());

    // PDF write options
    auto ps_idx = pdf_page_size_dropdown_.get_selected();
    switch (ps_idx) {
        case 0: request.pdf_write_options.page_size =
                    PDFWriteOptions::PageSize::FitToImage; break;
        case 1: request.pdf_write_options.page_size =
                    PDFWriteOptions::PageSize::A4; break;
        case 2: request.pdf_write_options.page_size =
                    PDFWriteOptions::PageSize::Letter; break;
        default: break;
    }

    auto orient_idx = pdf_orientation_dropdown_.get_selected();
    switch (orient_idx) {
        case 0: request.pdf_write_options.orientation =
                    PDFWriteOptions::Orientation::Auto; break;
        case 1: request.pdf_write_options.orientation =
                    PDFWriteOptions::Orientation::Portrait; break;
        case 2: request.pdf_write_options.orientation =
                    PDFWriteOptions::Orientation::Landscape; break;
        default: break;
    }

    request.pdf_write_options.margin_mm = pdf_margin_spin_.get_value();

    // PDF read options
    request.pdf_read_options.dpi =
        static_cast<int>(pdf_dpi_spin_.get_value());

    // Determine output path
    std::string custom_name = file_name_entry_.get_text();
    // Trim whitespace
    custom_name.erase(0, custom_name.find_first_not_of(" \t\n\r"));
    if (!custom_name.empty()) {
        custom_name.erase(custom_name.find_last_not_of(" \t\n\r") + 1);
    }

    std::string ext;
    switch (output_fmt) {
        case Format::PNG:  ext = ".png"; break;
        case Format::JPEG: ext = ".jpg"; break;
        case Format::PDF:  ext = ".pdf"; break;
        default: ext = ".out"; break;
    }

    std::string final_filename;
    if (custom_name.empty()) {
        final_filename = "combined" + ext;
    } else {
        fs::path p(custom_name);
        std::string filename_part = p.filename().string();
        std::string entered_ext = p.extension().string();

        std::string entered_ext_lower = entered_ext;
        std::transform(entered_ext_lower.begin(), entered_ext_lower.end(), entered_ext_lower.begin(), ::tolower);
        std::string target_ext_lower = ext;
        std::transform(target_ext_lower.begin(), target_ext_lower.end(), target_ext_lower.begin(), ::tolower);

        if (!entered_ext.empty() && (entered_ext_lower == target_ext_lower ||
            (target_ext_lower == ".jpg" && entered_ext_lower == ".jpeg") ||
            (target_ext_lower == ".jpeg" && entered_ext_lower == ".jpg"))) {
            final_filename = filename_part;
        } else {
            std::string stem_part = p.filename().stem().string();
            if (stem_part.empty()) {
                stem_part = "combined";
            }
            final_filename = stem_part + ext;
        }
    }

    if (paths.size() == 1 && output_fmt != Format::PDF) {
        // Single file → single output file
        request.output_path =
            (fs::path(output_directory_) / final_filename).string();
    } else if (output_fmt == Format::PDF && !is_pdf_input()) {
        // Multiple images or single image → PDF (combined or converted)
        request.output_path =
            (fs::path(output_directory_) / final_filename).string();
    } else {
        // Batch: output path is the directory
        request.output_path = output_directory_;
    }

    // Run conversion
    ConversionResult result = manager_.convert(request);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_result_ = std::move(result);
    }

    dispatcher_done_.emit();
}

void MainWindow::on_cancel_clicked() {
    if (cancel_token_) {
        cancel_token_->cancel();
    }
}

// ── Progress / Completion (UI thread) ───────────────────────────────────────

void MainWindow::on_progress_update() {
    int pct;
    std::string msg;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pct = progress_percent_;
        msg = progress_message_;
    }

    progress_bar_.set_fraction(static_cast<double>(pct) / 100.0);
    if (!msg.empty()) {
        progress_label_.set_text(msg);
    } else {
        progress_label_.set_text("Converting... " + std::to_string(pct) + "%");
    }
}

void MainWindow::on_conversion_done() {
    // Join worker thread
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    ConversionResult result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result = std::move(last_result_);
    }

    set_ui_state_idle();

    // Update progress to complete
    if (result.all_succeeded) {
        progress_bar_.set_fraction(1.0);
        progress_label_.set_text("Done — " +
            std::to_string(result.success_count()) + " file(s) converted in " +
            std::to_string(result.total_time.count()) + " ms");
    } else if (result.first_error() &&
               result.first_error()->find("cancelled") != std::string::npos) {
        progress_label_.set_text("Cancelled");
        progress_bar_.set_fraction(0.0);
    } else {
        progress_label_.set_text("Completed with errors");
    }

    show_results_dialog(result);
}

void MainWindow::show_results_dialog(const ConversionResult& result) {
    std::ostringstream ss;

    if (result.all_succeeded) {
        ss << "✓ All " << result.success_count()
           << " file(s) converted successfully.\n\n";
        ss << "Total time: " << result.total_time.count() << " ms\n";
        ss << "Output: " << output_directory_;
    } else if (result.success_count() == 0 && result.first_error() &&
               result.first_error()->find("cancelled") != std::string::npos) {
        ss << "Conversion was cancelled by user.";
    } else {
        ss << result.success_count() << " of " << result.total_count()
           << " file(s) converted.\n\n";

        // Show errors
        for (const auto& fr : result.file_results) {
            if (!fr.success) {
                ss << "✗ " << fs::path(fr.input_path).filename().string()
                   << ": " << fr.error_message << "\n";
            }
        }
    }

    auto dlg = Gtk::AlertDialog::create(ss.str());
    dlg->show(*this);
}

// ── UI State ────────────────────────────────────────────────────────────────

void MainWindow::set_ui_state_idle() {
    convert_button_.set_sensitive(true);
    cancel_button_.set_sensitive(false);
    add_files_button_.set_sensitive(true);
    clear_button_.set_sensitive(true);
    browse_button_.set_sensitive(true);
    format_dropdown_.set_sensitive(true);
    options_frame_.set_sensitive(true);
    file_list_view_.set_interactive(true);
    update_convert_button_sensitivity();
}

void MainWindow::set_ui_state_converting() {
    convert_button_.set_sensitive(false);
    cancel_button_.set_sensitive(true);
    add_files_button_.set_sensitive(false);
    clear_button_.set_sensitive(false);
    browse_button_.set_sensitive(false);
    format_dropdown_.set_sensitive(false);
    options_frame_.set_sensitive(false);
    file_list_view_.set_interactive(false);
    file_name_entry_.set_sensitive(false);

    progress_bar_.set_fraction(0.0);
    progress_label_.set_text("Starting conversion...");
}

void MainWindow::update_convert_button_sensitivity() {
    bool can_convert = !file_list_view_.is_empty() &&
                       get_selected_output_format() != Format::Unknown &&
                       !output_directory_.empty();
    convert_button_.set_sensitive(can_convert);

    auto paths = file_list_view_.get_file_paths();
    Format output_fmt = get_selected_output_format();
    bool is_batch_separate = paths.size() > 1 && !(output_fmt == Format::PDF && !is_pdf_input());

    if (is_batch_separate) {
        file_name_entry_.set_sensitive(false);
        file_name_entry_.set_tooltip_text("Custom name is only available for single file conversion or combined PDF");
    } else {
        file_name_entry_.set_sensitive(true);
        file_name_entry_.set_tooltip_text("Custom name for combined or converted output file (default: combined)");
    }
}

} // namespace converter
