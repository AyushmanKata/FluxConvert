#pragma once

#include "conversion_queue_view.hpp"
#include <converter/conversion_manager.hpp>
#include <converter/conversion_request.hpp>
#include <converter/conversion_result.hpp>
#include <converter/progress.hpp>
#include <converter/resource_limits.hpp>

#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/scale.h>
#include <gtkmm/frame.h>
#include <gtkmm/separator.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/stack.h>
#include <gtkmm/messagedialog.h>
#include <giomm/liststore.h>
#include <glibmm/dispatcher.h>

#include <thread>
#include <mutex>
#include <memory>

namespace converter {

class MainWindow : public Gtk::ApplicationWindow {
public:
    MainWindow();
    ~MainWindow() override;

private:
    // ── UI Construction ──
    void build_ui();
    void build_header_bar();
    void build_file_section(Gtk::Box& parent);
    void build_output_section(Gtk::Box& parent);
    void build_options_section(Gtk::Box& parent);
    void build_progress_section(Gtk::Box& parent);
    void build_action_buttons(Gtk::Box& parent);

    // ── Options panel builders ──
    void build_jpeg_options();
    void build_png_options();
    void build_pdf_write_options();
    void build_pdf_read_options();
    void update_options_panel();
    bool is_pdf_input() const;

    // ── Signal handlers ──
    void on_add_files_clicked();
    void on_clear_clicked();
    void on_browse_output_clicked();
    void on_format_changed();
    void on_file_list_changed();
    void on_convert_clicked();
    void on_cancel_clicked();

    // ── Threading ──
    void on_progress_update();
    void on_conversion_done();
    void run_conversion();

    // ── State management ──
    void set_ui_state_idle();
    void set_ui_state_converting();
    void update_convert_button_sensitivity();
    Format get_selected_output_format() const;
    void show_results_dialog(const ConversionResult& result);
    void populate_format_dropdown();

    // ── Conversion engine ──
    ConversionManager manager_;
    std::shared_ptr<CancellationToken> cancel_token_;

    // ── Worker thread ──
    std::thread worker_thread_;
    Glib::Dispatcher dispatcher_progress_;
    Glib::Dispatcher dispatcher_done_;
    std::mutex mutex_;
    int progress_percent_ = 0;
    std::string progress_message_;
    ConversionResult last_result_;

    // ── Header ──
    Gtk::HeaderBar header_bar_;

    // ── File list section ──
    FileListView file_list_view_;
    Gtk::Button add_files_button_;
    Gtk::Button clear_button_;

    // ── Output section ──
    Gtk::DropDown format_dropdown_;
    Glib::RefPtr<Gtk::StringList> format_list_;
    Gtk::Entry file_name_entry_;
    Gtk::Label output_dir_label_;
    Gtk::Button browse_button_;
    std::string output_directory_;

    // ── Options section ──
    Gtk::Frame options_frame_;
    Gtk::Stack options_stack_;

    // JPEG options
    Gtk::Box jpeg_options_box_;
    Gtk::Scale jpeg_quality_scale_;
    Gtk::Label jpeg_quality_value_label_;

    // PNG options
    Gtk::Box png_options_box_;
    Gtk::Scale png_compression_scale_;
    Gtk::Label png_compression_value_label_;

    // PDF write options
    Gtk::Box pdf_write_options_box_;
    Gtk::DropDown pdf_page_size_dropdown_;
    Glib::RefPtr<Gtk::StringList> page_size_list_;
    Gtk::DropDown pdf_orientation_dropdown_;
    Glib::RefPtr<Gtk::StringList> orientation_list_;
    Gtk::SpinButton pdf_margin_spin_;

    // PDF read options
    Gtk::Box pdf_read_options_box_;
    Gtk::SpinButton pdf_dpi_spin_;

    // ── Progress section ──
    Gtk::Box progress_box_;
    Gtk::ProgressBar progress_bar_;
    Gtk::Label progress_label_;

    // ── Action buttons ──
    Gtk::Button convert_button_;
    Gtk::Button cancel_button_;

    // ── Format tracking ──
    std::vector<Format> available_formats_;
};

} // namespace converter
