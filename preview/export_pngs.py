"""Export current PPM/RGBA-TGA captures and source-backed comparison sheets."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import math
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
source_files = sorted(path for path in root.glob('draw-only-*')
                      if path.is_file() and path.suffix.lower() in {'.ppm', '.tga'})
# A transparent GL capture may share its stem with its RGB companion. Preserve
# RGBA in the published PNG; both originals are retained in the hash manifest.
selected = {}
for path in source_files:
    if path.stem not in selected or path.suffix.lower() == '.tga':
        selected[path.stem] = path

sources, captures = [], []
for path in source_files:
    with Image.open(path) as picture:
        sources.append({'name': path.name, 'sha256': digest(path), 'width': picture.width,
                        'height': picture.height, 'mode': picture.mode})
for stem, path in sorted(selected.items()):
    png = path.with_suffix('.png')
    with Image.open(path) as picture:
        picture.load()
        if 'A' in picture.getbands():
            picture = picture.convert('RGBA')
        else:
            picture = picture.convert('RGB')
        picture.save(png)
        capture = {'source': path.name, 'source_sha256': digest(path),
                   'source_format': path.suffix[1:].upper(), 'png': png.name,
                   'png_sha256': digest(png), 'width': picture.width, 'height': picture.height,
                   'mode': picture.mode}
        if picture.mode == 'RGBA':
            histogram = picture.getchannel('A').histogram()
            capture['alpha_pixels'] = {'clear': histogram[0], 'partial': sum(histogram[1:255]),
                                       'opaque': histogram[255]}
    companion = path.with_suffix('.ppm')
    if companion.is_file():
        capture['ppm'], capture['ppm_sha256'] = companion.name, digest(companion)
    captures.append(capture)
print(f'Exported {len(captures)} PNG previews from {len(source_files)} GL captures.')

font_file = Path('C:/Windows/Fonts/msyh.ttc')
font = ImageFont.truetype(str(font_file), 16) if font_file.is_file() else ImageFont.load_default()
overviews = []

def register_overview(path, image, capture_names):
    image.save(path)
    overviews.append({'name': path.name, 'sha256': digest(path), 'width': image.width,
                      'height': image.height, 'captures': capture_names})

def paste_capture(canvas, picture, x, y, box):
    picture = picture.copy()
    picture.thumbnail(box, Image.Resampling.LANCZOS)
    left, top = x + (box[0] - picture.width) // 2, y + (box[1] - picture.height) // 2
    if picture.mode == 'RGBA':
        pen = ImageDraw.Draw(canvas)
        tile = 12
        for row in range(0, picture.height, tile):
            for column in range(0, picture.width, tile):
                color = '#26323E' if (row // tile + column // tile) % 2 else '#1B2530'
                pen.rectangle((left + column, top + row,
                               left + min(column + tile - 1, picture.width - 1),
                               top + min(row + tile - 1, picture.height - 1)), fill=color)
        canvas.paste(picture, (left, top), picture)
    else:
        canvas.paste(picture, (left, top))

def contact_sheet(items, output_name):
    if not items:
        return
    columns = min(4, len(items))
    rows = math.ceil(len(items) / columns)
    overview = Image.new('RGB', (400 * columns, 300 * rows), '#111821')
    pen = ImageDraw.Draw(overview)
    for index, item in enumerate(items):
        x, y = index % columns * 400, index // columns * 300
        with Image.open(root / item['png']) as picture:
            paste_capture(overview, picture, x + 10, y + 8, (380, 248))
        label = Path(item['png']).stem.removeprefix('draw-only-')
        pen.text((x + 12, y + 262), label[:42], fill='#E9F1FA', font=font)
        pen.text((x + 12, y + 282), f"{item['width']} x {item['height']} px / {item['mode']}",
                 fill='#91A3B8', font=font)
    register_overview(root / output_name, overview, [item['png'] for item in items])

# Theme pages are captured at a common size. Crop to the real panel only for
# this sheet; responsive sheets retain the full tested viewport and dimensions.
names = ('瓷白', '雾透', '石墨', '海湾', '松林', '鸢尾', '暖砂')
by_name = {item['png']: item for item in captures}
theme_names = [f'draw-only-drawing-theme-{index}.png' for index in range(7)]
if all(name in by_name for name in theme_names):
    overview = Image.new('RGB', (1600, 624), '#111821')
    pen = ImageDraw.Draw(overview)
    for index, name in enumerate(names):
        x, y = index % 4 * 400, index // 4 * 312
        with Image.open(root / theme_names[index]) as picture:
            width, height = picture.size
            crop = picture.crop((round(width * .1375), round(height * .12),
                                 round(width * .8625), round(height * .88)))
            paste_capture(overview, crop, x + 10, y + 12, (380, 249))
        pen.text((x + 18, y + 276), f'{index + 1:02d} / {name}', fill='#E9F1FA', font=font)
    pen.text((1220, 410), 'lsrael / 液体玻璃', fill='#E9F1FA', font=font)
    pen.text((1220, 446), '七种全局主题', fill='#91A3B8', font=font)
    register_overview(root / 'draw-only-theme-overview.png', overview, theme_names)

contact_sheet([item for item in captures
               if 'backdrop' in item['png'].lower() or 'background' in item['png'].lower()],
              'draw-only-background-overview.png')
contact_sheet([item for item in captures if 'liquid' in item['png'].lower()
               or 'transparent' in item['png'].lower()], 'draw-only-liquid-overview.png')
comparison_names = ['draw-only-liquid-alpha-home-2.png', 'draw-only-liquid-alpha-drawing-2.png']
if all(name in by_name for name in comparison_names):
    comparison = Image.new('RGB', (1640, 600), '#E8EEF3')
    pen = ImageDraw.Draw(comparison)
    for index, name in enumerate(comparison_names):
        with Image.open(root / name) as source:
            source = source.convert('RGBA').crop((280, 158, 1322, 844))
            source.thumbnail((800, 530), Image.Resampling.LANCZOS)
            backdrop = Image.new('RGB', source.size, '#C9DCE9')
            lines = ImageDraw.Draw(backdrop)
            for x in range(-source.height, source.width, 46):
                lines.line((x, 0, x + source.height, source.height), fill='#EAF2F8', width=16)
            backdrop.paste(source, (0, 0), source)
            comparison.paste(backdrop, (20 + index * 820, 50))
        pen.text((24 + index * 820, 18), '主页文案' if index == 0 else '绘制功能行', fill='#263B4C', font=font)
    register_overview(root / 'draw-only-liquid-consistency.png', comparison, comparison_names)
responsive = [item for item in captures if 'responsive' in item['png'].lower()]
contact_sheet(responsive, 'draw-only-responsive-overview.png')
matrix = sorted({(item['width'], item['height']) for item in responsive})
(root / 'capture-manifest.json').write_text(json.dumps({
    'generated_at': datetime.now(timezone.utc).isoformat(), 'scope': 'desktop OpenGL captures',
    'exporter_sha256': digest(Path(__file__)), 'sources': sources, 'captures': captures,
    'overviews': overviews,
    'responsive_capture_resolutions_px': [{'width': width, 'height': height} for width, height in matrix],
}, ensure_ascii=False, indent=2), 'utf-8')
