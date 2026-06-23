<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=auto&height=200&section=header&text=mscr&fontSize=70&fontAlignY=35&animation=twinkling">
</p>

<p align="center">
  <code>metadata scraper · linux · c++</code>
</p>

<p align="center">
  <img src="https://skillicons.dev/icons?i=linux,cpp">
</p>

scans files recursively, extracts metadata, and can strip it in-place. zero dependencies — just the binary.

## features

- recursive directory scanning with full stat info (permissions, owner, timestamps, inode)
- image metadata: jpeg (exif), png, gif, bmp, webp
- text analysis: word/line/char counts, encoding detection
- in-place stripping: exif, png ancillary chunks, gif comments, bom/trailing whitespace
- json output with pretty-print
- multi-threaded scanning

## usage

```
mscr --pretty image.jpg
mscr --strip -v image.jpg
mscr --threads 8 --pretty ~/Pictures/
mscr ~/Pictures/ | jq '.[] | {file: .name, width: .image.width, height: .image.height}'
```

## build

```bash
git clone https://github.com/kroown/mscr.git
cd mscr
cmake -B build
cmake --build build
cp build/mscr ~/.local/bin/
```

## options

| flag | description |
|------|-------------|
| `--pretty` | pretty-print json |
| `--strip` | strip metadata in-place |
| `-v`, `--verbose` | verbose output |
| `-t`, `--threads` | worker threads |
| `--help` | show help |

## example output

```json
{
  "path": "/home/user/photo.jpg",
  "name": "photo.jpg",
  "size": 421356,
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
| **png** | all ancillary chunks: `tEXt`, `iTXt`, `zTXt`, `eXIf` — keeps `IHDR`, `PLTE`, `IDAT`, `IEND` |
| **gif** | comment, application, plain text extensions — keeps graphics control, image data |
| **text** | utf-8 bom, crlf normalization |

## architecture

```
src/
  main.cpp       cli entry point, dispatch (scan vs strip)
  scanner.cpp/h  recursive file scanner + stat collector
  image.cpp/h    jpeg exif, png/gif/bmp/webp dimension parsers
  text.cpp/h     word/line/char counter, encoding detection
  json.cpp/h     json serializer (no dependencies)
  strip.cpp/h    in-place metadata removal per format
```

<p align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=auto&height=100&section=footer">
</p>
