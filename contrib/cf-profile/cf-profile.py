from argparse import ArgumentParser
import sys
import json
import re
from collections import defaultdict

"""
This script parses profiling output from cf-agent and lists policy bundles, promises, and functions sorted by execution time.

Usage:
    $ sudo /var/cfengine/cf-agent -Kp
    $ python3 cf-profile.py profiling_output.json --bundles --promises --functions
"""


def parse_args():
    parser = ArgumentParser()

    # parser.add_argument("profiling_output")
    parser.add_argument("--top", type=int, default=10)
    parser.add_argument("--bundles", action="store_true")
    parser.add_argument("--promises", action="store_true")
    parser.add_argument("--functions", action="store_true")
    parser.add_argument("--flamegraph", str="", default="stack.txt")

    return parser.parse_args()


def format_elapsed_time(elapsed_ns):
    elapsed_ms = float(elapsed_ns) / 1e6

    if elapsed_ms < 1000:
        return "%.2f ms" % elapsed_ms
    elif elapsed_ms < 60000:
        elapsed_s = elapsed_ms / 1000.0
        return "%.2fs" % elapsed_s
    else:
        elapsed_s = elapsed_ms / 1000.0
        minutes = int(elapsed_s // 60)
        seconds = int(elapsed_s % 60)
        return "%dm%ds" % (minutes, seconds)


def format_label(event_type, name):
    if event_type == "function":
        return "%s() %s call" % (name, event_type)
    elif name == "methods":
        return "bundle invocation"
    else:
        return "%s %s" % (name, event_type)


def format_columns(events, top):

    labels = []

    for event in events[:top]:
        label = format_label(event["type"], event["name"])
        location = "%s:%s" % (event["filename"], event["offset"]["line"])
        time = format_elapsed_time(event["elapsed"])

        labels.append((label, location, time))

    return labels


def get_max_column_lengths(lines, indent=4):

    max_type, max_location, max_time = 0, 0, 0

    for label, location, time_ms in lines:
        max_type = max(max_type, len(label))
        max_location = max(max_location, len(location))
        max_time = max(max_time, len(time_ms))

    return max_type + indent, max_location + indent, max_time + indent


def profile(data, args):

    events = data["events"]
    filter = defaultdict(list)

    if args.bundles:
        filter["type"].append("bundle")
        filter["name"].append("methods")

    if args.promises:
        filter["name"] += list(
            set(
                event["name"]
                for event in events
                if event["type"] == "promise" and event["name"] != "methods"
            )
        )

    if args.functions:
        filter["type"].append("function")

    # filter events
    if filter is not None:
        events = [
            event
            for field in filter.keys()
            for event in events
            if event[field] in filter[field]
        ]

    # sort events
    events = sorted(events, key=lambda x: x["elapsed"], reverse=True)

    lines = format_columns(events, args.top)
    line_format = "%-{}s %-{}s %{}s".format(*get_max_column_lengths(lines))

    # print top k filtered events
    print(line_format % ("Type", "Location", "Time"))
    for label, location, time_ms in lines:
        print(line_format % (label, location, time_ms))

def generate_callstacks(data, stack_path):
    events = data["events"]

    with open(stack_path, "w") as f:
        for event in events:
            f.write("%s %d\n" % (event["callstack"], event["elapsed"]))

def main():
    args = parse_args()
    m = re.search(r"\{[.\s\S]*\}", sys.stdin.read())
    # with open(args.profiling_output, "r") as f:
    #     data = json.load(f)
    data = json.loads(m.group(0))


    if args.flamegraph:
        generate_callstacks(data, args.flamegraph)
    else:
        profile(data, args)



if __name__ == "__main__":
    main()
