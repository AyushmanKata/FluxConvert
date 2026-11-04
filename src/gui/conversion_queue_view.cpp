#include "conversion_queue_view.hpp"
#include <converter/format.hpp>
#include <gdkmm/contentprovider.h>
#include <gtkmm/droptarget.h>
#include <giomm/file.h>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace converter {

// ── FileRow ──────────────────────────────────────────────────────────────────

FileRow::FileRow(const FileEntry& entry)
    : Gtk::Box(Gtk::Orientation::HORIZONTAL, 8)
    , entry_(entry)
{
    set_margin(4);

    // File icon + name
    name_label_.set_text("📄  " + entry.display_name);
    name_label_.set_hexpand(true);
    name_label_.set_halign(Gtk::Align::START);
    name_label_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    name_label_.set_tooltip_text(entry.path);

    // Format badge
    format_label_.set_text(entry.format_label);
    format_label_.add_css_class("dim-label");
    format_label_.set_width_chars(5);

    // File size
    size_label_.set_text(entry.size_label);
    size_label_.add_css_class("dim-label");
    size_label_.set_width_chars(10);
    size_label_.set_halign(Gtk::Align::END);

    // Remove button
    remove_button_.set_icon_name("edit-delete-symbolic");
    remove_button_.set_has_frame(false);
    remove_button_.set_tooltip_text("Remove file");
    remove_button_.signal_clicked().connect([this]() {
        signal_remove_.emit(this);
    });

    append(name_label_);
    append(format_label_);
    append(size_label_);
    append(remove_button_);
}

// ── FileListView ─────────────────────────────────────────────────────────────

FileListView::FileListView()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0)
{
    // Scrolled window containing the list box
    scrolled_window_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scrolled_window_.set_min_content_height(180);
    scrolled_window_.set_vexpand(true);

    list_box_.set_selection_mode(Gtk::SelectionMode::NONE);
    list_box_.add_css_class("boxed-list");
    scrolled_window_.set_child(list_box_);

    // Placeholder label
    placeholder_label_.set_text("Drop files here or click \"Add Files\"");
    placeholder_label_.add_css_class("dim-label");
    placeholder_label_.set_vexpand(true);
    placeholder_label_.set_valign(Gtk::Align::CENTER);
    placeholder_label_.set_halign(Gtk::Align::CENTER);
    list_box_.set_placeholder(placeholder_label_);

    append(scrolled_window_);

    // Setup drag-and-drop
    setup_drop_target();
}

void FileListView::setup_drop_target() {
    drop_target_ = Gtk::DropTarget::create(Gio::File::get_type(), Gdk::DragAction::COPY);

    // Accept multiple values
    drop_target_->set_actions(Gdk::DragAction::COPY);

    // Handle the drop
    drop_target_->signal_drop().connect(
        [this](const Glib::ValueBase& value, double /*x*/, double /*y*/) -> bool {
            if (G_VALUE_HOLDS(value.gobj(), G_TYPE_OBJECT)) {
                auto* obj = static_cast<GObject*>(g_value_get_object(value.gobj()));
                if (G_IS_FILE(obj)) {
                    auto file = Glib::wrap(G_FILE(obj), true);
                    if (file) {
                        auto path = file->get_path();
                        if (!path.empty()) {
                            add_files({path});
                            return true;
                        }
                    }
                }
            }
            return false;
        },
        false);

    // Visual feedback
    drop_target_->signal_enter().connect(
        [this](double, double) -> Gdk::DragAction {
            add_css_class("drop-active");
            return Gdk::DragAction::COPY;
        },
        false);

    drop_target_->signal_leave().connect(
        [this]() {
            remove_css_class("drop-active");
        });

    add_controller(drop_target_);
}

void FileListView::add_files(const std::vector<std::string>& paths) {
    if (!interactive_) return;

    for (const auto& path : paths) {
        // Skip duplicates
        bool already_exists = false;
        for (const auto& e : entries_) {
            if (e.path == path) {
                already_exists = true;
                break;
            }
        }
        if (already_exists) continue;

        // Skip non-existent files
        std::error_code ec;
        if (!fs::exists(path, ec)) continue;

        FileEntry entry;
        entry.path = path;
        entry.display_name = fs::path(path).filename().string();
        entry.format_label = detect_format_label(path);
        entry.size_label = format_file_size(fs::file_size(path, ec));

        // Skip files with unknown/unsupported formats
        if (entry.format_label == "???") continue;

        add_file_entry(entry);
    }

    signal_list_changed_.emit();
}

void FileListView::add_file_entry(const FileEntry& entry) {
    entries_.push_back(entry);

    auto* row = Gtk::make_managed<FileRow>(entry);
    row->signal_remove().connect(
        sigc::mem_fun(*this, &FileListView::remove_row));
    list_box_.append(*row);
}

void FileListView::remove_row(FileRow* row) {
    if (!interactive_) return;

    // Remove from entries
    const auto& entry = row->get_entry();
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&entry](const FileEntry& e) { return e.path == entry.path; }),
        entries_.end());

    // Remove widget
    list_box_.remove(*row);
    signal_list_changed_.emit();
}

void FileListView::clear() {
    // Remove all children from list_box_
    while (auto* child = list_box_.get_first_child()) {
        // Skip the placeholder which is managed by set_placeholder
        if (child == &placeholder_label_) break;
        list_box_.remove(*child);
    }
    entries_.clear();
    signal_list_changed_.emit();
}

std::vector<std::string> FileListView::get_file_paths() const {
    std::vector<std::string> paths;
    paths.reserve(entries_.size());
    for (const auto& e : entries_) {
        paths.push_back(e.path);
    }
    return paths;
}

void FileListView::set_interactive(bool interactive) {
    interactive_ = interactive;
    // Dim the list when non-interactive
    set_sensitive(interactive);
}

void FileListView::update_placeholder() {
    // Placeholder is handled automatically by ListBox::set_placeholder
}

std::string FileListView::format_file_size(std::uintmax_t bytes) {
    constexpr std::uintmax_t KB = 1024;
    constexpr std::uintmax_t MB = 1024 * KB;
    constexpr std::uintmax_t GB = 1024 * MB;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    if (bytes >= GB) {
        ss << (static_cast<double>(bytes) / static_cast<double>(GB)) << " GB";
    } else if (bytes >= MB) {
        ss << (static_cast<double>(bytes) / static_cast<double>(MB)) << " MB";
    } else if (bytes >= KB) {
        ss << (static_cast<double>(bytes) / static_cast<double>(KB)) << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

std::string FileListView::detect_format_label(const std::string& path) {
    Format fmt = detect_format_from_file(path);
    if (fmt != Format::Unknown) {
        return std::string(format_to_string(fmt));
    }
    // Fallback to extension
    auto ext = fs::path(path).extension().string();
    if (!ext.empty()) {
        fmt = extension_to_format(ext);
        if (fmt != Format::Unknown) {
            return std::string(format_to_string(fmt));
        }
    }
    return "???";
}

} // namespace converter
