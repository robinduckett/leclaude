# This tool makes assets/leclaude.ico from assets/leclaude.svg.
#
# The SVG is pixel art on a 16x16 grid. It contains only rectangles.
# Each rectangle has a transform matrix from the vector editor, and the
# transformed edges are almost exactly on the integer grid.
#
# The tool does not use a general SVG renderer. It applies the transforms,
# snaps the edges to the grid, and paints each rectangle with hard integer
# pixel edges. Thus each icon size is crisp, without anti-aliasing.

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets" / "leclaude.svg"
TARGET = ROOT / "assets" / "leclaude.ico"
SIZES = [16, 20, 24, 32, 48, 64, 128, 256]
GRID = 16

# The distance limit between a transformed edge and the grid. A larger
# distance shows that the SVG is not aligned, and the tool must stop.
SNAP_TOLERANCE = 0.05

SVG_NS = "{http://www.w3.org/2000/svg}"


def parse_fill(style: str | None) -> tuple[int, int, int, int]:
    # The SVG default fill is black.
    if style:
        match = re.search(r"fill:\s*rgb\((\d+),\s*(\d+),\s*(\d+)\)", style)
        if match:
            r, g, b = (int(v) for v in match.groups())
            return (r, g, b, 255)
    return (0, 0, 0, 255)


def snap(value: float) -> int:
    snapped = round(value)
    if abs(value - snapped) > SNAP_TOLERANCE:
        raise SystemExit(f"The edge {value} is not on the grid. Repair the SVG alignment.")
    return snapped


def parse_rects(svg_path: Path):
    """Returns a list of (x0, y0, x1, y1, color) tuples on the 16x16 grid, in paint order."""
    tree = ET.parse(svg_path)
    rects = []
    for group in tree.getroot():
        if not group.tag.endswith("}g") and not group.tag.endswith("}rect"):
            continue
        # The transform is "matrix(a,0,0,d,e,f)". Without a transform, use the identity.
        a, d, e, f = 1.0, 1.0, 0.0, 0.0
        transform = group.get("transform")
        if transform:
            numbers = [float(v) for v in re.findall(r"-?\d+\.?\d*", transform)]
            a, b, c, d, e, f = numbers
            if b != 0.0 or c != 0.0:
                raise SystemExit("The tool does not support a rotation in the transform.")
        elements = [group] if group.tag.endswith("}rect") else group.findall(f"{SVG_NS}rect")
        for rect in elements:
            x = float(rect.get("x", "0"))
            y = float(rect.get("y", "0"))
            w = float(rect.get("width"))
            h = float(rect.get("height"))
            x0 = snap(a * x + e)
            y0 = snap(d * y + f)
            x1 = snap(a * (x + w) + e)
            y1 = snap(d * (y + h) + f)
            rects.append((x0, y0, x1, y1, parse_fill(rect.get("style"))))
    if not rects:
        raise SystemExit("The SVG contains no rectangles.")
    return rects


def render(rects, size: int) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    scale = size / GRID
    for x0, y0, x1, y1, color in rects:
        px0 = round(x0 * scale)
        py0 = round(y0 * scale)
        px1 = round(x1 * scale)
        py1 = round(y1 * scale)
        if px1 > px0 and py1 > py0:
            draw.rectangle([px0, py0, px1 - 1, py1 - 1], fill=color)
    return image


def main() -> None:
    rects = parse_rects(SOURCE)
    print(f"Parsed {len(rects)} rectangles.")
    variants = [render(rects, s) for s in SIZES]
    largest = variants[-1]
    largest.save(
        TARGET,
        format="ICO",
        append_images=variants[:-1],
        sizes=[(s, s) for s in SIZES],
    )
    print(f"Wrote {TARGET} with sizes {SIZES}")

    # A preview helps a visual check. It shows the 16-pixel variant at 8x scale.
    preview = variants[0].resize((128, 128), Image.NEAREST)
    preview_path = ROOT / "assets" / "preview-16px.png"
    preview.save(preview_path)
    print(f"Wrote {preview_path}")


if __name__ == "__main__":
    main()
