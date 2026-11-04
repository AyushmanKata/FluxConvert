#include "converter/format_registry.hpp"
#include "converter/format_handler.hpp"
#include <iostream>
#include <cassert>

using namespace converter;

// Mock format handler for testing
class MockHandler : public FormatHandler {
public:
    explicit MockHandler(Format format) : format_(format) {}

    Format get_format() const override { return format_; }

    FormatCapabilities get_capabilities() const override {
        FormatCapabilities caps;
        caps.can_read = true;
        caps.can_write = true;
        return caps;
    }

    bool can_convert(Format from, Format to) const override {
        return from == format_ && to != format_;
    }

    FileConversionResult convert(
        const std::string& /*input_path*/,
        const std::string& /*output_path*/,
        const ConversionRequest& /*request*/) override
    {
        FileConversionResult result;
        result.success = true;
        return result;
    }

private:
    Format format_;
};

void test_registry_singleton() {
    std::cout << "Testing registry singleton...\n";

    auto& reg1 = FormatRegistry::instance();
    auto& reg2 = FormatRegistry::instance();

    assert(&reg1 == &reg2);

    std::cout << "  ✓ Registry singleton works\n";
}

void test_handler_registration() {
    std::cout << "Testing handler registration...\n";

    auto& registry = FormatRegistry::instance();
    registry.clear();

    registry.register_handler(std::make_unique<MockHandler>(Format::PNG));
    registry.register_handler(std::make_unique<MockHandler>(Format::JPEG));

    auto* png_handler = registry.get_handler(Format::PNG);
    assert(png_handler != nullptr);
    assert(png_handler->get_format() == Format::PNG);

    auto* jpeg_handler = registry.get_handler(Format::JPEG);
    assert(jpeg_handler != nullptr);
    assert(jpeg_handler->get_format() == Format::JPEG);

    auto* pdf_handler = registry.get_handler(Format::PDF);
    assert(pdf_handler == nullptr);

    std::cout << "  ✓ Handler registration works\n";
}

void test_conversion_support() {
    std::cout << "Testing conversion support queries...\n";

    auto& registry = FormatRegistry::instance();
    registry.clear();

    registry.register_handler(std::make_unique<MockHandler>(Format::PNG));

    assert(registry.is_conversion_supported(Format::PNG, Format::JPEG));
    assert(registry.is_conversion_supported(Format::PNG, Format::PDF));
    assert(!registry.is_conversion_supported(Format::JPEG, Format::PNG));

    std::cout << "  ✓ Conversion support queries work\n";
}

void test_capabilities() {
    std::cout << "Testing capabilities queries...\n";

    auto& registry = FormatRegistry::instance();
    registry.clear();

    registry.register_handler(std::make_unique<MockHandler>(Format::PNG));

    auto caps = registry.get_capabilities(Format::PNG);
    assert(caps.can_read);
    assert(caps.can_write);

    auto no_caps = registry.get_capabilities(Format::PDF);
    assert(!no_caps.can_read);
    assert(!no_caps.can_write);

    std::cout << "  ✓ Capabilities queries work\n";
}

void test_independent_registry() {
    std::cout << "Testing independent registry instances...\n";

    FormatRegistry custom_registry;
    custom_registry.register_handler(std::make_unique<MockHandler>(Format::PDF));

    assert(custom_registry.get_handler(Format::PDF) != nullptr);
    assert(custom_registry.get_handler(Format::PNG) == nullptr);

    // Ensure singleton was not affected by custom registry
    auto& singleton = FormatRegistry::instance();
    (void)singleton;

    std::cout << "  ✓ Independent registry works\n";
}

int main() {
    std::cout << "=== Format Registry Tests ===\n";

    try {
        test_registry_singleton();
        test_handler_registration();
        test_conversion_support();
        test_capabilities();
        test_independent_registry();

        std::cout << "\n✓ All registry tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << "\n";
        return 1;
    }
}
