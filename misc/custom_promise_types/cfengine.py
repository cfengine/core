import sys
import json
import traceback

_LOG_LEVELS = {level: idx for idx, level in enumerate(("critical", "error", "warning", "notice", "info", "verbose", "debug"))}

def _skip_until_empty_line(file):
    while True:
        line = file.readline().strip()
        if not line:
            break


def _get_request(file, record_file=None):
    line = file.readline()
    blank_line = file.readline()
    if record_file is not None:
        record_file.write("< " + line)
        record_file.write("< " + blank_line)

    return json.loads(line.strip())


def _put_response(data, file, record_file=None):
    data = json.dumps(data)
    file.write(data + "\n\n")
    file.flush()

    if record_file is not None:
        record_file.write("> " + data + "\n")
        record_file.write("> \n")


def _would_log(level_set, msg_level):
    if msg_level not in _LOG_LEVELS:
        # uknown level, assume it would be logged
        return True

    return _LOG_LEVELS[msg_level] <= _LOG_LEVELS[level_set]


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
    NOT_KEPT = "not_kept"  # Not satisfied before , not fixed

    # Validation only, can reveal problems in CFEngine policy:
    VALID = "valid"  # Validation successful
    INVALID = "invalid"  # Validation failed, error in cfengine policy

    # Generic succes / fail for init / terminate requests:
    SUCCESS = "success"
    FAILURE = "failure"

    # Unexpected, can reveal problems in promise module:
    ERROR = "error"  # Something went wrong in module / protocol


class PromiseModule:
    def __init__(self, name = "default_module_name", version = "0.0.1", record_file_path=None):
        self.name = name
        self.version = version
        # Note: The class doesn't expose any way to set protocol version
        # or flags, because that should be abstracted away from the
        # user (module author).

        self._result_classes = None

        # File to record all the incoming and outgoing communication
        self._record_file = open(record_file_path, "a") if record_file_path else None

    def start(self, in_file=None, out_file=None):
        self._in = in_file or sys.stdin
        self._out = out_file or sys.stdout

        first_line = self._in.readline()
        if self._record_file is not None:
            self._record_file.write("< " + first_line)

        header = first_line.strip().split(" ")
        name = header[0]
        version = header[1]
        protocol_version = header[2]
        # flags = header[3:] -- unused for now

        assert len(name) > 0              # cf-agent
        assert version.startswith("3.")   # 3.18.0
        assert protocol_version[0] == "v" # v1

        _skip_until_empty_line(self._in)

        header_reply = f"{self.name} {self.version} v1 json_based\n\n"
        self._out.write(header_reply)
        self._out.flush()

        if self._record_file is not None:
            self._record_file.write("> " + header_reply.strip() + "\n")
            self._record_file.write(">\n")

        while True:
            self._response = {}
            self._result = None
            request = _get_request(self._in, self._record_file)
            self._handle_request(request)

    def _handle_request(self, request):
        if not request:
            sys.exit("Error: Empty/invalid request or EOF reached")

        operation = request["operation"]
        self._log_level = request.get("log_level", "info")
        self._response["operation"] = operation

        # Agent will never request log level critical
        assert self._log_level in ["error", "warning", "notice", "info", "verbose", "debug"]

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
            self._log_level = None
            raise ProtocolError(f"Unknown operation: '{operation}'")

        self._log_level = None

    def _add_result(self):
        self._response["result"] = self._result

    def _add_result_classes(self):
        if self._result_classes:
            self._response["result_classes"] = self._result_classes

    def _add_traceback_to_response(self):
        if self._log_level != "debug":
            return

        trace = traceback.format_exc()
        logs = self._response.get("log", [])
        logs.append({"level": "debug", "message": trace})
        self._response["log"] = logs

    def _handle_init(self):
        self._result = self.protocol_init(None)
        self._add_result()
        _put_response(self._response, self._out, self._record_file)

    def _handle_validate(self, promiser, attributes):
        try:
            returned = self.validate_promise(promiser, attributes)
            if returned is None:
                # Good, expected
                self._result = Result.VALID
            else:
                # Bad, validate method shouldn't return anything else
                self._result = Result.ERROR
        except ValidationError as e:
            self.log_error(e)
            self._result = Result.INVALID
        except Exception as e:
            self.log_critical(f"{type(e).__name__}: {e}")
            self._add_traceback_to_response()
            self._result = Result.ERROR
        self._add_result()
        _put_response(self._response, self._out, self._record_file)

    def _handle_evaluate(self, promiser, attributes):
        self._result_classes = None
        try:
            results = self.evaluate_promise(promiser, attributes)

            # evaluate_promise should return either a result or a (result, result_classes) pair
            if type(results) == str:
                self._result = results
            else:
                assert len(results) == 2
                self._result = results[0]
                self._result_classes = results[1]
        except Exception as e:
            self.log_critical(f"{type(e).__name__}: {e}")
            self._add_traceback_to_response()
            self._result = Result.ERROR
        self._add_result()
        self._add_result_classes()
        _put_response(self._response, self._out, self._record_file)

    def _handle_terminate(self):
        self._result = self.protocol_terminate()
        self._add_result()
        _put_response(self._response, self._out, self._record_file)
        sys.exit(0)

    def _log(self, level, message):
        if self._log_level is not None and not _would_log(self._log_level, level):
            return

        # Message can be str or an object which implements __str__()
        # for example an exception:
        message = str(message).replace("\n", r"\n")
        assert "\n" not in message
        self._out.write(f"log_{level}={message}\n")
        self._out.flush()

        if self._record_file is not None:
            self._record_file.write(f"log_{level}={message}\n")

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

    def _log_traceback(self):
        trace = traceback.format_exc().split('\n')
        for line in trace:
            self.log_debug(line)

    # Functions to override in subclass:

    def protocol_init(self, version):
        return Result.SUCCESS

    def validate_promise(self, promiser, attributes):
        raise NotImplementedError("Promise module must implement validate_promise")

    def evaluate_promise(self, promiser, attributes):
        raise NotImplementedError("Promise module must implement evaluate_promise")

    def protocol_terminate(self):
        return Result.SUCCESS
