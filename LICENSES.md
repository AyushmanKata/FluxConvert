# License Information

## Application License

**FluxConvert** is licensed under the **GNU Affero General Public License v3.0 or later (AGPL-3.0+)**.

This choice is **required** because the application uses MuPDF, which is AGPL-licensed.

### What This Means

✓ **You can**:
- Use this software for any purpose
- Study how it works and modify it
- Distribute copies
- Distribute modified versions

✗ **You must**:
- Make source code available to users
- License derivative works under AGPL-3.0+
- Provide source code if you offer the software as a network service
- Include copyright and license notices

### Full License Text

See the [LICENSE](LICENSE) file for the complete AGPL-3.0 license text.

## Dependency Licenses

This application uses several open-source libraries with different licenses:

| Library         | Version | License      | Type       | Compatibility |
|-----------------|---------|--------------|------------|---------------|
| MuPDF           | 1.23.10 | AGPL-3.0+    | Copyleft   | ✓ (requires AGPL) |
| libpng          | 1.6.43  | zlib/libpng  | Permissive | ✓ Compatible |
| libjpeg-turbo   | 2.1.5   | BSD-style    | Permissive | ✓ Compatible |
| GTK4            | 4.14.5  | LGPL-2.1+    | Library    | ✓ Compatible |
| gtkmm           | 4.10.0  | LGPL-2.1+    | Library    | ✓ Compatible |

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for detailed license texts.

## Commercial Licensing Alternative

If AGPL-3.0 licensing is not suitable for your use case:

1. **MuPDF Commercial License**: Artifex Software offers commercial licenses for MuPDF
   that do not require AGPL compliance. See https://artifex.com/licensing/
   
2. **Alternative Implementation**: The PDF renderer is isolated behind an adapter
   interface and can be replaced with a different library (e.g., Poppler with
   LGPL/GPL license).

## Source Code Availability

In compliance with AGPL-3.0, the complete source code for this application
is available at: [GitHub repository URL would go here]

Build instructions are provided in [README.md](README.md).

## Questions?

For licensing questions, please open an issue on GitHub or contact the maintainers.

---

**Note**: This is not legal advice. Consult with a lawyer for specific licensing questions.
