import os
import sys


def split_line(line):
    assert "=" in line and "\n" not in line
    split = line.index("=")
    key, value = line[0:split], line[split + 1 :]
    assert key and "=" not in key and " " not in key
    return key, value


def get_request(file):
    request = {}
    while True:
        line = file.readline().strip()
        if not line:
            return request
        key, value = split_line(line)
        request[key] = value


def validate_promise(request, file):
    result = "invalid"
    if "attribute_repo" not in request:
        file.write("log_error=repo attribute missing\n")
    elif request["promiser"][0] != "/":
        file.write("log_error=local path must be absolute\n")
    else:
        result = "valid"
    file.write(f"result={result}\n\n")
    file.flush()


def evaluate_promise(request, file):
    folder = request["promiser"]
    url = request["attribute_repo"]

    if os.path.exists(folder):
        file.write("result=kept\n\n")
        file.flush()
        return

    file.write(f"log_info=Cloning '{url}' -> '{folder}'...\n")
    file.flush()

    os.system(f"git clone {url} {folder} 2>/dev/null")

    if os.path.exists(folder):
        file.write(f"log_info=Successfully cloned '{url}' -> '{folder}'\n")
        file.write("result=repaired\n\n")
        file.flush()
    else:
        file.write(f"log_error=Failed to clone '{url}' -> '{folder}'\n")
        file.write("result=not_kept\n\n")
        file.flush()


def handle_request(request, file):
    for key, value in request.items():
        assert key != "result"
        if key in ["promiser", "operation"] or key.startswith("attribute_"):
            file.write(f"{key}={value}\n")

    operation = request["operation"]
    if operation == "init":
        file.write("result=success\n\n")
    elif operation == "terminate":
        file.write("result=success\n\n")
        sys.exit(0)
    elif operation == "validate_promise":
        validate_promise(request, file)
    elif operation == "evaluate_promise":
        evaluate_promise(request, file)
    else:
        raise NotImplementedError(f"Unknown protocol command: {operation}")


def main():
    header = sys.stdin.readline().strip().split(" ")
    assert header[0] == "CFEngine"
    _ = sys.stdin.readline()

    sys.stdout.write("git_promises 0.0.1 v1 line_based\n\n")
    sys.stdout.flush()

    while True:
        request = get_request(sys.stdin)
        handle_request(request, sys.stdout)


if __name__ == "__main__":
    main()
