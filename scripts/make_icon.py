# This tool makes assets/leclaude.ico from assets/leclaudebot.png.
# The source is pixel art. The tool uses nearest-neighbor scale steps to keep the hard edges.

from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets" / "leclaudebot.png"
TARGET = ROOT / "assets" / "leclaude.ico"
SIZES = [16, 20, 24, 32, 48, 64, 128, 256]


def make_square(image: Image.Image) -> Image.Image:
    side = max(image.size)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    x = (side - image.width) // 2
    y = (side - image.height) // 2
    canvas.paste(image, (x, y))
    return canvas


def main() -> None:
    source = Image.open(SOURCE).convert("RGBA")
    square = make_square(source)
    variants = [square.resize((s, s), Image.NEAREST) for s in SIZES]
    largest = variants[-1]
    largest.save(
        TARGET,
        format="ICO",
        append_images=variants[:-1],
        sizes=[(s, s) for s in SIZES],
    )
    print(f"Wrote {TARGET} with sizes {SIZES}")


if __name__ == "__main__":
    main()
