#include "main_window.hpp"
#include <gtkmm/application.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/styleprovider.h>
#include <gdkmm/display.h>
#include <iostream>

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create("org.fluxconvert.FluxConvert");

    // Load CSS for custom styling
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(
        ".drop-active { border: 2px dashed @accent_color; }"
        ".success-label { color: @success_color; }"
        ".error-label { color: @error_color; }"
        ".options-group { padding: 8px; }"
    );
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    return app->make_window_and_run<converter::MainWindow>(argc, argv);
}
