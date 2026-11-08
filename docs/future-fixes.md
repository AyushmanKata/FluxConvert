# Future Fixes

The following items are intentionally deferred from the first release. They
should be addressed before expanding supported-file claims or increasing
resource limits.

## Encrypted PDF support

Add explicit encrypted-PDF detection and a user-facing error, or implement
password handling if encrypted input becomes a supported feature. Add fixtures
and tests for password-protected PDFs.

## Incorrectly named files

Expand end-to-end coverage for files whose extensions do not match their
content. Preserve signature-based detection, document the expected behavior,
and ensure output naming remains predictable.

## Large PDF resource validation

Exercise large and high-page-count PDFs against the configured DPI, page-count,
pixel, and memory limits. Confirm malformed PDFs cannot bypass those limits and
that cancellation or failure never leaves partial output files.
