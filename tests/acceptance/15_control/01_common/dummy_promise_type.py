#!/usr/bin/python3
#
# Minimal custom promise module which accepts any promise and does nothing,
# used to have a custom promise type in evaluation order tests.

import sys


def read_request():
    request = {}
    for line in sys.stdin:
        line = line.rstrip("\n")
        if not line:
            return request
        key, _, value = line.partition("=")
        request[key] = value
    sys.exit(0)


def write_response(operation, result):
    sys.stdout.write("operation=%s\nresult=%s\n\n" % (operation, result))
    sys.stdout.flush()


# Skip the header from the agent and send ours
read_request()
sys.stdout.write("dummy_promise_type 0.0.1 v1 line_based\n\n")
sys.stdout.flush()

while True:
    operation = read_request().get("operation")
    if operation == "validate_promise":
        write_response(operation, "valid")
    elif operation == "evaluate_promise":
        write_response(operation, "kept")
    elif operation == "terminate":
        write_response(operation, "success")
        sys.exit(0)
    else:
        write_response(operation, "error")
