# cfg-lib

`cfg-lib` is the binary configuration library used by `commonlog-lib` and
`clogctl`.

## Runtime path

All cfg-lib APIs operate directly on:

- `/etc/commonlog.conf`

The library reads and writes that file directly on each API call.

## Default config generation

The default binary config file is not stored directly in source form.
Instead, it is generated during the build from a text source.

Files involved:

- `log-default.txt`
  - Human-editable source of the default config data.
- `convert_to_cfg.sh`
  - Converts `log-default.txt` into the binary `commonlog.conf` file.
- `commonlog.conf`
  - Generated binary config file.

## Build flow

`Makefile.am` is wired so that `commonlog.conf` is generated from
`log-default.txt` by `convert_to_cfg.sh`:

- during `make`
- again during `make install`

The generated file is installed to:

- `/etc/commonlog.conf`

This means the default `.cfg` content always comes from the current
`log-default.txt` whenever the recipe/component is built.

## Why the script generates the binary directly

`convert_to_cfg.sh` writes the binary format directly using a host-side Perl
script.

It does not execute `clogctl` during the build. This is intentional so the
Yocto recipe can generate the default `.cfg` file without trying to run a
target-built binary on the host.

## Supported text record types

`log-default.txt` supports these record types:

- `GLOBAL <sink> <level>`
- `SETTING <key> <value>`
- `APP <app_id> <sink> <level>`
- `CTX <app_id> <context_id> <level>`
- `LCTX <context_id> <level>`

These map to the same binary sections used by `cfg-lib` at runtime.

Supported `SETTING` keys:

- `add_clogdetails_in_msg`
  - `0` or `1`; when enabled, `commonlog-lib` adds backend-specific APPID and
    CONTEXTID detail tags derived from the registered numeric IDs and app/
    context strings.
- `sink_file_max_size_mb`
  - Positive integer, interpreted as megabytes. This is the max size of the
    active `/data/common_logs/<timestamp>_<APPID>.txt` file before the file
    backend starts a new timestamped file for that app.
- `sink_file_rotate_count`
  - Non-negative integer. This is the maximum number of archived timestamped
    files retained per app in addition to the active file. Total file-sink
    retention per app is capped at `sink_file_rotate_count + 1` files.
