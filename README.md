<picture><source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/mscr-metadata%20scraper-7c3aed?style=flat&logo=read-the-docs&logoColor=white"><source media="(prefers-color-scheme: light)" srcset="https://img.shields.io/badge/mscr-metadata%20scraper-7c3aed?style=flat&logo=read-the-docs&logoColor=white"><img alt="mscr" src="https://img.shields.io/badge/mscr-metadata%20scraper-7c3aed?style=flat&logo=read-the-docs&logoColor=white"></picture>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/C%2B%2B-20-informational?style=flat&logo=cplusplus&logoColor=white">
  <source media="(prefers-color-scheme: light)" srcset="https://img.shields.io/badge/C%2B%2B-20-informational?style=flat&logo=cplusplus&logoColor=white">
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-informational?style=flat&logo=cplusplus&logoColor=white">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/license-MIT-blue?style=flat">
  <source media="(prefers-color-scheme: light)" srcset="https://img.shields.io/badge/license-MIT-blue?style=flat">
  <img alt="MIT" src="https://img.shields.io/badge/license-MIT-blue?style=flat">
</picture>
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://img.shields.io/badge/platform-linux-333?style=flat&logo=linux&logoColor=white">
  <source media="(prefers-color-scheme: light)" srcset="https://img.shields.io/badge/platform-linux-eee?style=flat&logo=linux&logoColor=333">
  <img alt="Linux" src="https://img.shields.io/badge/platform-linux-333?style=flat&logo=linux&logoColor=white">
</picture>

a zero-dependency metadata scraper and stripper for linux. scans files recursively, extracts image dimensions & exif, analyzes text, and can strip metadata in-place — all with **no external libraries, no python, no overhead**.

## features

- **🔍 scan** — recursive directory crawling with full stat info (permissions, owner, timestamps, inode)
- **🖼️ image metadata** — jpeg (dimensions + exif camera make/model/orientation/gps), png, gif, bmp, webp
- **📝 text analysis** — word/line/character counts, ascii / utf-8 detection, binary file detection
- **✂️ strip** — remove exif from jpeg, ancillary chunks from png, comments from gif, bom + trailing whitespace from text files
- **⚡ multi-threaded** — automatic parallel scanning with configurable thread count
- **📦 json output** — machine-readable, pipeable, pretty-print support

## build

```bash
# clone
git clone https://github.com/kroown/mscr.git
cd mscr

# build
cmake -B build
cmake --build build

# install (optional)
cp build/mscr ~/.local/bin/
```

**zero dependencies** — only a c++20 compiler (gcc 16+, clang 18+).

## usage

### scan a single file

```bash
mscr image.jpg
```

### scan a directory recursively

```bash
mscr ~/Pictures/
```

### pretty-print output

```bash
mscr --pretty ~/Downloads/*.jpg
```

### strip metadata in-place

```bash
mscr --strip -v ~/Pictures/phone_photo.jpg
```

### strip with verbose output

```bash
mscr --strip -v ~/Pictures/
```

### multi-threaded scan

```bash
mscr --threads 8 --pretty ~/media/
```

### pipe to jq

```bash
mscr ~/Pictures/ | jq '.[] | {file: .name, width: .image.width, height: .image.height}'
```

## options

| flag | description |
|------|-------------|
| `--pretty` | pretty-print json output |
| `--strip` | strip metadata from files in-place |
| `-v`, `--verbose` | verbose progress to stderr |
| `-t`, `--threads` | worker threads (default: all cores) |
| `--help` | display this help |

## example output

```json
{
  "path": "/home/user/photo.jpg",
  "name": "photo.jpg",
  "extension": ".jpg",
  "size": 421356,
  "permissions": "rw-r--r--",
  "owner": "user",
  "group": "user",
  "mtime_ns": 1700000000000000000,
  "image": {
    "format": "jpeg",
    "width": 4032,
    "height": 3024,
    "orientation": 1,
    "camera_make": "Apple",
    "camera_model": "iPhone 15 Pro",
    "has_gps": true
  }
}
```

## what gets stripped

| format | removed |
|--------|---------|
| **jpeg** | app1 (exif), app2, com (comments) — keeps jfif/app0, frame/scan headers |
| **png** | all ancillary chunks: `tEXt`, `iTXt`, `zTXt`, `eXIf`, etc. — keeps `IHDR`, `PLTE`, `IDAT`, `IEND` |
| **gif** | comment, application, plain text extensions — keeps graphics control, image data |
| **text** | utf-8 bom, crlf → lf normalization |

output is **structurally identical** — images remain valid and renderable.

## architecture

```
src/
  main.cpp       cli entry point, dispatch (scan vs strip)
  scanner.cpp/h  recursive file scanner + stat collector
  image.cpp/h    jpeg → exif parser, png/gif/bmp/webp dimension parsers
  text.cpp/h     word/line/char counter, encoding detection
  json.cpp/h     json serializer (no dependencies)
  strip.cpp/h    in-place metadata removal per format
```

## license

mit — do whatever you want.
