#!/usr/bin/env python3
"""Generate beautiful SVG agent logos

Example:
    Generate an SVG agent logo, with circle radius 16::

        $ python3 agentsvg.py 16 > agent.svg

    Use ImageMagick (installed separately) to convert to png:

        $ convert agent.svg agent.png

"""

import argparse

cfengine_blue = "#0071a6"
cfengine_orange = "#f5821f"


class Circle:
    """An SVG Circle with x, y, radius and color"""

    def __init__(self, x, y, r, c):
        self.x = x * r * 2 + r
        self.y = y * r * 2 + r
        self.r = r
        self.c = c

    def __str__(self):
        return '<circle cx="{}" cy="{}" r="{}" fill="{}" stroke="none"/>'.format(
            self.x,
            self.y,
            self.r,
            self.c)


def rect(x, y, w, h, radius, color):
    """Returns a list of circles forming a rectangle of size w,h"""
    circles = []
    for r in range(y, y + h):
        for c in range(x, x + w):
            circles.append(Circle(c, r, radius, color))
    return circles


def line(x, y, dx, dy, n, radius, color):
    """Returns a list of circles forming a line of length n"""
    circles = []
    if dx != 0 and dy != 0:
        diagonal_factor = 0.70710678118  # sin(45) = cos(45) = sqrt(2) / 2
        dx = dx * diagonal_factor
        dy = dy * diagonal_factor
    for i in range(n):
        circles.append(Circle(x + dx * i, y + dy * i, radius, color))
    return circles


def get_args():
    ap = argparse.ArgumentParser(
        description="Beautiful CFEngine agent logos",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    ap.add_argument("--radius", "-r", help="Circle raidus", type=int, default=16)
    ap.add_argument("--body", help="Body color", type=str, default=cfengine_blue)
    ap.add_argument("--head", help="Head color", type=str, default=cfengine_orange)
    ap.add_argument(
        "--arms",
        help="Arms pose",
        type=str,
        default="down",
        choices=["down",
                 "out",
                 "up",
                 "angled"])
    ap.add_argument(
        "--legs",
        help="Legs pose",
        type=str,
        default="straight",
        choices=["straight",
                 "out"])

    args = ap.parse_args()
    return args


def main():
    args = get_args()

    radius = int(args.radius)
    body = args.body
    head = args.head
    arms = args.arms
    legs = args.legs

    diameter = radius * 2
    width = diameter * 14
    height = diameter * 12

    # Actual logo:
    content = []
    content += rect(4, 0, 3, 2, radius, head)  # Head
    content += rect(4, 3, 3, 3, radius, body)  # Torso

    if arms == "down":
        content += line(2, 3, 0, 1, 4, radius, body)  # Left arm
        content += line(8, 3, 0, 1, 4, radius, body)  # Right arm
    elif arms == "out":
        content += line(2, 3, -1, 0, 4, radius, body)  # Left arm
        content += line(8, 3, 1, 0, 4, radius, body)  # Right arm
    elif arms == "up":
        content += line(2, 3, -1, -1, 4, radius, body)  # Left arm
        content += line(8, 3, 1, -1, 4, radius, body)  # Right arm
    elif arms == "angled":
        content += line(1, 4, 1, -1, 2, radius, body)  # Left arm
        content += line(1, 5, 0, 1, 2, radius, body)  # Left arm
        content += line(9, 4, -1, -1, 2, radius, body)  # Right arm
        content += line(9, 5, 0, 1, 2, radius, body)  # Right arm

    if legs == "straight":
        content += line(4, 7, 0, 1, 4, radius, body)  # Left leg
        content += line(6, 7, 0, 1, 4, radius, body)  # Right leg
    elif legs == "out":
        content += line(4, 7, -1, 1, 4, radius, body)  # Left leg
        content += line(6, 7, 1, 1, 4, radius, body)  # Right leg

    content.append(Circle(5, 7, radius, body))  # Groin

    # Offsets needed to center this logo in canvas:
    offset_x = radius + diameter * 1
    offset_y = radius

    # Apply offsets to all circles:
    for circle in content:
        circle.x += offset_x
        circle.y += offset_y

    # The SVG container:
    container = """
    <svg width="{}" height="{}" version="1.1" xmlns="http://www.w3.org/2000/svg">
    {}
    </svg>
    """

    # Render SVG, print to stdout:
    content = "\n".join(str(x) for x in content)
    print(container.format(width, height, content))


if __name__ == "__main__":
    main()
