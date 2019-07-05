import os
import sys

blue = "#0071a6"
orange = "#f5821f"
green = "#79A442"
grey = "#92B3B7"

color_names = {
    blue: "blue",
    orange: "orange",
    green: "green",
    grey: "grey",
}

orange_blue = {"head": orange, "body": blue}
blue_blue = {"head": blue, "body": blue}
orange_green = {"head": orange, "body": green}
green_blue = {"head": green, "body": blue}
grey_blue = {"head": grey, "body": blue}

normal = {"arms": "down", "legs": "straight"}
out = {"arms": "out", "legs": "out"}
angled = {"arms": "angled", "legs": "straight"}
up = {"arms": "up", "legs": "straight"}
up_out = {"arms": "up", "legs": "out"}

colors = [orange_blue, blue_blue, orange_green, green_blue, grey_blue]
sizes = [8, 16, 32, 64, 128]
poses = [normal, out, angled, up, up_out]

for size in sizes:
    diameter = size * 2
    width = diameter * 14
    height = diameter * 12
    dimensions = f"{width}x{height}"

    radius_opt = f"--radius {size}"
    os.makedirs(f"./{dimensions}", exist_ok=True)
    for color in colors:
        head = color["head"]
        body = color["body"]
        color_opts = f"--head '{head}' --body '{body}'"
        head = color_names[head]
        body = color_names[body]
        for pose in poses:
            arms = pose["arms"]
            legs = pose["legs"]
            pose_opts = f"--arms {arms} --legs {legs}"
            command = f"python3 agentsvg.py {color_opts} {pose_opts} {radius_opt}"
            base_name = f"agent_{head}_{body}_{arms}_{legs}_{dimensions}"
            generate = f"{command} > ./{dimensions}/{base_name}.svg"
            convert = f"convert ./{dimensions}/{base_name}.svg ./{dimensions}/{base_name}.png"
            command = f"{generate} && {convert}"
            print(command)
            ret = os.system(command)
            if ret != 0:
                sys.exit("Error: The command above failed! (Non-zero exit code)")
