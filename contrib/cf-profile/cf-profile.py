from argparse import ArgumentParser
import sys
import json
import re

def parse_args():
    parser = ArgumentParser()

    parser.add_argument("--top", type=int, default=10)
    parser.add_argument("--bundles", action="store_true")
    parser.add_argument("--promises", action="store_true")
    parser.add_argument("--functions", action="store_true")

    return parser.parse_args()

def profile(data, args):

    events = sorted([event for event in data["events"]], key=lambda x: x["elapsed"], reverse=True)

    filter = []

    if args.bundles:
        filter.append("bundle")

    if args.promises:
        filter.append("promise")

    if args.functions:
        filter.append("function")

    if filter:
        events = [event for event in events if event["type"] in filter]

    print("%-60s %-90s %20s" % ("Component", "Location", "Time"))
    for t in events[:args.top]:

        label = "%s %s" % (t["type"], t["name"])
        location = "%s:%s" % (t["filename"], t["offset"]["line"])
        time_ms = "%.2f ms" % (float(t["elapsed"]) / 1e6)

        print("%-60s %-90s %20s" % (label, location, time_ms))

def main():
    args = parse_args()
    m = re.search(r"\{[.\s\S]*\}", sys.stdin.read())
    data = json.loads(m.group(0))

    profile(data, args)


if __name__ == "__main__":
    main()