# Troubleshooting

## The application does not start

Run the CLI with `--help` first to confirm that the installation is complete:

```bash
fluxconvert-cli --help
```

For a source build, use the executable from the selected build directory, such
as `./build-release/fluxconvert-gui`. GTK4 and the runtime image/PDF libraries
must be installed on the target system.

## A file is rejected

FluxConvert checks file signatures instead of trusting extensions. Rename a
file only when its contents match the new extension. Encrypted PDFs,
unsupported formats, malformed files, and images that exceed the configured
resource limits are rejected.

## Conversion fails while writing output

Ensure the destination directory exists and is writable, and choose an output
path different from the input path. Check available disk space for large
images and PDFs.

## Batch conversion reports partial failures

Batch jobs continue processing independent files and report each failed input
at the end. Correct or remove the failed inputs and run the batch again; files
that completed successfully are not retried automatically.
