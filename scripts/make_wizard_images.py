# This tool makes the wizard images for the Inno Setup program.
#
# The images come from assets/leclaude.svg, with the renderer from
# make_icon.py. Each display scale gets one file. The setup program
# selects the file that matches the display scale. The robot has an
# integer pixel scale in each file. Thus the pixel art stays crisp.
#
# The large image shows on the left of the "Finished" page.
# The small image shows in the top right corner of the other pages.

from pathlib import Path

from PIL import Image

from make_icon import GRID, parse_rects, render

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets" / "leclaude.svg"

BACKGROUND = (255, 255, 255)

# (name, canvas width, canvas height, robot scale) for each display scale.
LARGE = [
    ("wizard-image-100.bmp", 164, 314, 8),
    ("wizard-image-125.bmp", 205, 393, 10),
    ("wizard-image-150.bmp", 246, 471, 12),
    ("wizard-image-200.bmp", 328, 628, 16),
]
SMALL = [
    ("wizard-small-100.bmp", 55, 58, 3),
    ("wizard-small-125.bmp", 69, 73, 4),
    ("wizard-small-150.bmp", 83, 87, 5),
    ("wizard-small-200.bmp", 110, 116, 6),
]


def compose(rects, width: int, height: int, scale: int) -> Image.Image:
    robot = render(rects, GRID * scale)
    canvas = Image.new("RGB", (width, height), BACKGROUND)
    x = (width - robot.width) // 2
    y = (height - robot.height) // 2
    canvas.paste(robot, (x, y), robot)
    return canvas


def main() -> None:
    rects = parse_rects(SOURCE)
    for name, width, height, scale in LARGE + SMALL:
        image = compose(rects, width, height, scale)
        # An 8-bit palette keeps the file small. The pixel art has few colors.
        image = image.convert("P", palette=Image.ADAPTIVE)
        target = ROOT / "assets" / name
        image.save(target, format="BMP")
        print(f"Wrote {target}")


if __name__ == "__main__":
    main()
