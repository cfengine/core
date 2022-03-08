import os
import sys

colors = []
color_names = {}

# Normal, official colors agent:
orange = "#F5821F" # Head, CFEngine orange
blue = "#052569"   # Body, CFEngine blue
color_names[orange] = "orange"
color_names[blue] = "blue"
colors.append({"head": orange, "body": blue, "name": "official"})

# Dark mode agent:
off_orange = "#F9AB2D" # Head, dark mode
off_white = "#E5E5E5"  # Body, dark mode
color_names[off_orange] = "off_orange"
color_names[off_white] = "off_white"
colors.append({"head": off_orange, "body": off_white, "name": "dark_mode"})

# Mender agent:
mender_purple = "#551741" # Head, Mender color, dark purple
mender_teal = "#235867"   # Body, Mender color, dark teal
color_names[mender_purple] = "mender_purple"
color_names[mender_teal] = "mender_teal"
colors.append({"head": mender_purple, "body": mender_teal, "name": "mender"})

# Neutral grey and blue agent:
dark_blue = "#274B97" # Head, Dark blue
grey = "#ACADAE"      # Body, Neutral grey
color_names[dark_blue] = "dark_blue"
color_names[grey] = "grey"
colors.append({"head": dark_blue, "body": grey, "name": "neutral"})

# Winter / Northern.tech agent:
nt_blue = "#00B0F0"   # Head, Northern.tech blue
# Body is CFEngine blue
color_names[nt_blue] = "nt_blue"
colors.append({"head": nt_blue, "body": blue, "name": "northerntech"})

# Green and orange community / spring agent:
# Head is CFEngine orange
green = "#00843D" # Body, green
color_names[green] = "green"
colors.append({"head": orange, "body": green, "name": "community"})

# Norwegian agent:
norway_blue = "#274B97" # Head, Norwegian blue
norway_red = "#AA2734"  # Body, Norwegian red
color_names[norway_blue] = "norway_blue"
color_names[norway_red] = "norway_red"
colors.append({"head": norway_blue, "body": norway_red, "name": "norway"})

# Fall / alert / attention agent:
# Head is CFEngine orange
yellow = "#FCC335"  # Body, yellow
color_names[yellow] = "yellow"
colors.append({"head": orange, "body": yellow, "name": "yellow"})

# Valentine / charity agent:
bright_pink = "#FACED1" # Head, bright pink
pink = "#DA7972"        # Body, red/pink
color_names[bright_pink] = "bright_pink"
color_names[pink] = "pink"
colors.append({"head": bright_pink, "body": pink, "name": "valentine"})

# Green St. Patty's agent:
bright_green = "#95D0A7" # Head, bright green
# Body is CFEngine community green
color_names[bright_green] = "bright_green"
colors.append({"head": bright_green, "body": green, "name": "green"})

normal = {"arms": "down", "legs": "straight"}
out = {"arms": "out", "legs": "out"}
angled = {"arms": "angled", "legs": "straight"}
up = {"arms": "up", "legs": "straight"}
up_out = {"arms": "up", "legs": "out"}

sizes = [8, 16, 32, 64, 128]
poses = [normal, out, angled, up, up_out]

opts = ["", "--no-margins"]

for size in sizes:
    diameter = size * 2
    width = diameter * 14
    height = diameter * 14
    dimensions = f"{width}x{height}"

    radius_opt = f"--radius {size}"
    os.makedirs(f"./{dimensions}", exist_ok=True)
    for color in colors:
        head = color["head"]
        body = color["body"]
        name = color["name"]
        color_opts = f"--head '{head}' --body '{body}'"
        for pose in poses:
            arms = pose["arms"]
            legs = pose["legs"]
            pose_opts = f"--arms {arms} --legs {legs}"
            for opt in opts:
                command = f"python3 agentsvg.py {color_opts} {pose_opts} {radius_opt} {opt}"
                if opt:
                    opt = opt.replace("--", "").replace("-", "_")
                base_name = f"agent_{name}_{arms}_{legs}_{dimensions}" + (f"_{opt}" if opt else "")
                generate = f"{command} > ./{dimensions}/{base_name}.svg"
                convert = f"convert -background none ./{dimensions}/{base_name}.svg ./{dimensions}/{base_name}.png"
                command = f"{generate} && {convert}"
                print(command)
                ret = os.system(command)
                if ret != 0:
                    sys.exit("Error: The command above failed! (Non-zero exit code)")
