#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/droptarget.h>
#include <giomm/file.h>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace converter {

// Data for one file in the queue
struct FileEntry {
    std::string path;
    std::string display_name;
    std::string format_label;   // e.g. "JPEG", "PNG", "PDF"
    std::string size_label;     // e.g. "2.3 MB"
};

// Single row in the file list
class FileRow : public Gtk::Box {
public:
    explicit FileRow(const FileEntry& entry);

    const FileEntry& get_entry() const { return entry_; }

    // Signal emitted when the remove button is clicked
    using RemoveSignal = sigc::signal<void(FileRow*)>;
    RemoveSignal signal_remove() { return signal_remove_; }

private:
    FileEntry entry_;
    Gtk::Label name_label_;
    Gtk::Label format_label_;
    Gtk::Label size_label_;
    Gtk::Button remove_button_;
    RemoveSignal signal_remove_;
};

// File list widget with drag-and-drop support
class FileListView : public Gtk::Box {
public:
    FileListView();

    // Add files to the queue
    void add_files(const std::vector<std::string>& paths);

    // Remove all files
    void clear();

    // Get all file paths
    std::vector<std::string> get_file_paths() const;

    // Check if empty
    bool is_empty() const { return entries_.empty(); }

    // Get number of files
    size_t file_count() const { return entries_.size(); }

    // Signal emitted when the file list changes
    using ListChangedSignal = sigc::signal<void()>;
    ListChangedSignal signal_list_changed() { return signal_list_changed_; }

    // Enable/disable interaction (during conversion)
    void set_interactive(bool interactive);

private:
    void setup_drop_target();
    void add_file_entry(const FileEntry& entry);
    void remove_row(FileRow* row);
    void update_placeholder();
    static std::string format_file_size(std::uintmax_t bytes);
    static std::string detect_format_label(const std::string& path);

    Gtk::ScrolledWindow scrolled_window_;
    Gtk::ListBox list_box_;
    Gtk::Label placeholder_label_;
    Glib::RefPtr<Gtk::DropTarget> drop_target_;

    std::vector<FileEntry> entries_;
    ListChangedSignal signal_list_changed_;
    bool interactive_ = true;
};

} // namespace converter
