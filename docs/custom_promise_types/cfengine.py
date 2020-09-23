import os
import sys
import json
from enum import Enum, auto, unique


def _skip_until_empty_line(file):
    while True:
        line = file.readline().strip()
        if not line:
            break


def _get_request(file):
    line = file.readline().strip()
    _ = file.readline()
    # sys.stderr.write("LINE: " + line + "\n")
    return json.loads(line)


def _put_response(data, file):
    data = json.dumps(data)
    file.write(data + "\n\n")
    file.flush()


class ValidationError(Exception):
    def __init__(self, message):
        self.message = message


class ProtocolError(Exception):
    def __init__(self, message):
        self.message = message


class Result:
    # Promise evaluation outcomes, can reveal "real" problems with system:
    KEPT = "kept"  # Satisfied already, no change
    REPAIRED = "repaired"  # Not satisfied before , but fixed
    NOTKEPT = "not_kept"  # Not satisfied before , not fixed

    # Validation only, can reveal problems in CFEngine policy:
    VALID = "valid"  # Validation successful
    INVALID = "invalid"  # Validation failed, error in cfengine policy

    # Generic succes / fail for init / terminate requests:
    SUCCESS = "success"
    FAILURE = "failure"

    # Unexpected, can reveal problems in promise module:
    ERROR = "error"  # Something went wrong in module / protocol


class PromiseModule:
    def __init__(self):
        pass

    def start(self, in_file=None, out_file=None):
        self._in = in_file or sys.stdin
        self._out = out_file or sys.stdout

        header = self._in.readline().strip().split(" ")
        name = header[0]
        version = header[1]
        protocol_version = header[2]
        flags = header[3:]

        assert name == "CFEngine"
        assert version.startswith("3.16")
        assert protocol_version == "v1"
        assert flags == []

        _skip_until_empty_line(self._in)

        self._out.write("git_promises 0.0.1 v1 json_based\n\n")
        self._out.flush()

        while True:
            self._response = {}
            self._result = None
            request = _get_request(self._in)
            self._handle_request(request)

    def _handle_request(self, request):
        if not request:
            sys.exit("Error: Empty/invalid request or EOF reached")

        operation = request["operation"]
        log_level = request.get("log_level", "info")
        self._response["operation"] = operation

        # Agent will never request log level critical
        assert log_level in ["error", "warning", "notice", "info", "verbose", "debug"]

        if operation in ["validate_promise", "evaluate_promise"]:
            promiser = request["promiser"]
            attributes = request.get("attributes", {})
            self._response["promiser"] = promiser
            self._response["attributes"] = attributes

        if operation == "init":
            self._handle_init()
        elif operation == "validate_promise":
            self._handle_validate(promiser, attributes)
        elif operation == "evaluate_promise":
            self._handle_evaluate(promiser, attributes)
        elif operation == "terminate":
            self._handle_terminate()
        else:
            raise ProtocolError(f"Unknown operation: '{operation}'")

    def _add_result(self):
        self._response["result"] = self._result

    def _handle_init(self):
        self._result = Result.SUCCESS
        self.protocol_init(None)
        self._add_result()
        _put_response(self._response, self._out)

    def _handle_validate(self, promiser, attributes):
        self._result = Result.VALID
        self.validate_promise(promiser, attributes)
        self._add_result()
        _put_response(self._response, self._out)

    def _handle_evaluate(self, promiser, attributes):
        self._result = Result.KEPT
        self.evaluate_promise(promiser, attributes)
        self._add_result()
        _put_response(self._response, self._out)

    def _handle_terminate(self):
        self._result = Result.SUCCESS
        self.protocol_terminate()
        self._add_result()
        _put_response(self._response, self._out)
        sys.exit(0)

    def _log(self, level, message):
        self._out.write(f"log_{level}={message}\n")
        self._out.flush()

    def log_critical(self, message):
        self._log("critical", message)

    def log_error(self, message):
        self._log("error", message)

    def log_warning(self, message):
        self._log("warning", message)

    def log_notice(self, message):
        self._log("notice", message)

    def log_info(self, message):
        self._log("info", message)

    def log_verbose(self, message):
        self._log("verbose", message)

    def log_debug(self, message):
        self._log("debug", message)

    def promise_kept(self):
        self._result = Result.KEPT

    def promise_repaired(self):
        self._result = Result.REPAIRED

    def promise_not_kept(self):
        self._result = Result.NOTKEPT

    # Functions to override in subclass:

    def protocol_init(self, version):
        pass

    def validate_promise(self, promiser, attributes):
        raise NotImplementedError("Promise module must implement validate_promise")

    def evaluate_promise(self, promiser, attributes):
        raise NotImplementedError("Promise module must implement evaluate_promise")

    def protocol_terminate(self):
        pass
