# Extending FluxConvert

## Overview

FluxConvert is designed with a modular architecture that makes adding support for new image or document formats (e.g. WebP, TIFF, SVG, BMP) straightforward and isolated from the core conversion pipeline and presentation layers.

---

## Architecture of a Format Plugin

Format support in FluxConvert revolves around three primary concepts:

```
┌──────────────────────────────────────────────┐
│                Format Enum                   │
│        (include/converter/format.hpp)        │
└──────────────────────┬───────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────┐
│            FormatHandler Interface           │
│    (include/converter/format_handler.hpp)    │
└──────────────────────┬───────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────┐
│                FormatRegistry                │
│    (include/converter/format_registry.hpp)   │
└──────────────────────────────────────────────┘
```

1. **Format Identification**: Magic-byte sniffing and extension mapping.
2. **Adapter Layer**: Low-level library wrapper that decodes from / encodes to [`ImageData`](file:///home/ayushman/Projects/file-converter/include/converter/image_data.hpp).
3. **FormatHandler Implementation**: Implements conversion rules, capabilities, and batch/multi-file logic.
4. **Registration**: Exposing the handler to [`FormatRegistry`](file:///home/ayushman/Projects/file-converter/include/converter/format_registry.hpp).

---

## Step-by-Step Guide: Adding a New Format (Example: WebP)

### Step 1: Add Format Enum Value

In [`include/converter/format.hpp`](file:///home/ayushman/Projects/file-converter/include/converter/format.hpp):

```cpp
enum class Format {
    Unknown,
    PNG,
    JPEG,
    PDF,
    WEBP  // <-- Add new format
};
```

Update `format_to_string()`, `extension_to_format()`, and `detect_format_from_buffer()` with the magic number (e.g. `RIFF....WEBP`):

```cpp
// In src/core/format.cpp
if (size >= 12 &&
    data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
    data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
    return Format::WEBP;
}
```

---

### Step 2: Implement Codec Adapter

Create `src/formats/webp/webp_adapter.hpp` and `webp_adapter.cpp` wrapping `libwebp`:

```cpp
#pragma once
#include "converter/image_data.hpp"
#include <string>

namespace converter {

ImageData webp_decode(const std::string& path);
void webp_encode(const ImageData& image, const std::string& path, int quality);

} // namespace converter
```

---

### Step 3: Implement `FormatHandler`

Subclass [`FormatHandler`](file:///home/ayushman/Projects/file-converter/include/converter/format_handler.hpp) in `src/formats/webp/webp_handler.cpp`:

```cpp
#include "converter/format_handler.hpp"
#include "converter/format_registry.hpp"
#include "converter/error.hpp"
#include "webp_adapter.hpp"

namespace converter {

class WebpHandler : public FormatHandler {
public:
    Format get_format() const override {
        return Format::WEBP;
    }

    FormatCapabilities get_capabilities() const override {
        FormatCapabilities caps;
        caps.can_read = true;
        caps.can_write = true;
        caps.supports_transparency = true;
        caps.supports_multi_page = false;
        return caps;
    }

    bool can_convert(Format from, Format to) const override {
        return (from == Format::WEBP && (to == Format::PNG || to == Format::JPEG || to == Format::PDF)) ||
               ((from == Format::PNG || from == Format::JPEG) && to == Format::WEBP);
    }

    FileConversionResult convert(
        const std::string& input_path,
        const std::string& output_path,
        const ConversionRequest& request) override
    {
        // Decode source, encode destination, return FileConversionResult
        ...
    }
};

void register_webp_handler(FormatRegistry& registry) {
    registry.register_handler(std::make_unique<WebpHandler>());
}

void register_webp_handler() {
    register_webp_handler(FormatRegistry::instance());
}

} // namespace converter
```

---

### Step 4: Hook into Registration and Build

1. Declare the registration function in [`include/converter/handler_registration.hpp`](file:///home/ayushman/Projects/file-converter/include/converter/handler_registration.hpp):
   ```cpp
   void register_webp_handler(FormatRegistry& registry);
   void register_webp_handler();
   ```
2. Call `register_webp_handler(registry)` in `register_builtin_handlers()` inside [`src/core/registry.cpp`](file:///home/ayushman/Projects/file-converter/src/core/registry.cpp).
3. Add source files and pkg-config dependencies to [`CMakeLists.txt`](file:///home/ayushman/Projects/file-converter/CMakeLists.txt).

---

### Step 5: Test the Integration

Add unit and integration tests under [`tests/`](file:///home/ayushman/Projects/file-converter/tests/):
1. Unit test format detection in `tests/unit/test_format_detection.cpp`.
2. Add conversion round-trip assertions in `tests/integration/test_image_conversion.cpp`.
3. Verify test suite passes with `ctest --test-dir build-release`.
