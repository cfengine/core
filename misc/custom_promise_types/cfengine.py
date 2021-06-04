import sys
import json
import traceback
from copy import copy
from collections import OrderedDict

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


def _cfengine_type(typing):
    if typing is str:
        return "string"
    if typing is int:
        return "int"
    if typing in (list, tuple):
        return "slist"
    if typing is dict:
        return "data container"
    if typing is bool:
        return "true/false"
    return "Error in promise module"


class AttributeObject(object):
    def __init__(self, d):
        for key, value in d.items():
            setattr(self, key, value)


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
        self._validator_attributes = OrderedDict()
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

    def _convert_types(self, promiser, attributes):
        # Will only convert types if module has typing information:
        if not self._has_validation_attributes:
            return promiser, attributes

        replacements = {}
        for name, value in attributes.items():
            if type(value) is not str:
                # If something is not string, assume it is correct type
                continue
            if name not in self._validator_attributes:
                # Unknown attribute, this will cause a validation error later
                continue
            # "true"/"false" -> True/False
            if self._validator_attributes[name]["typing"] is bool:
                if value == "true":
                    replacements[name] = True
                elif value == "false":
                    replacements[name] = False
            # "int" -> int()
            elif self._validator_attributes[name]["typing"] is int:
                try:
                    replacements[name] = int(value)
                except ValueError:
                    pass

        # Don't edit dict while iterating over it, after instead:
        attributes.update(replacements)

        return (promiser, attributes)


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
            promiser, attributes = self._convert_types(promiser, attributes)
            promiser, attributes = self.prepare_promiser_and_attributes(promiser, attributes)
            self._response["promiser"] = promiser
            self._response["attributes"] = attributes

        if operation == "init":
            self._handle_init()
        elif operation == "validate_promise":
            self._handle_validate(promiser, attributes, request)
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

    def add_attribute(self, name, typing, default=None, required=False, default_to_promiser=False, validator=None):
        attribute = OrderedDict()
        attribute["name"] = name
        attribute["typing"] = typing
        attribute["default"] = default
        attribute["required"] = required
        attribute["default_to_promiser"] = default_to_promiser
        attribute["validator"] = validator
        self._validator_attributes[name] = attribute

    @property
    def _has_validation_attributes(self):
        return bool(self._validator_attributes)

    def create_attribute_object(self, promiser, attributes):

        # Check for missing required attributes:
        for name, attribute in self._validator_attributes.items():
            if attribute["required"] and name not in attributes:
                raise ValidationError(f"Missing required attribute '{name}'")

        # Check for unknown attributes:
        for name in attributes:
            if name not in self._validator_attributes:
                raise ValidationError(f"Unknown attribute '{name}'")

        # Check typings and run custom validator callbacks:
        for name, value in attributes.items():
            expected = _cfengine_type(self._validator_attributes[name]["typing"])
            found = _cfengine_type(type(value))
            if found != expected:
                raise ValidationError(f"Wrong type for attribute '{name}', requires '{expected}', not '{value}'({found})")
            if self._validator_attributes[name]["validator"]:
                # Can raise ValidationError:
                self._validator_attributes[name]["validator"](value)

        # Construct as dict first:
        attribute_dict = OrderedDict()

        # Copy attributes specified by user policy:
        for key, value in attributes.items():
            attribute_dict[key] = value

        # Set defaults based on promise module validation hints:
        for name, value in self._validator_attributes.items():
            if value.get("default_to_promiser", False):
                attribute_dict.setdefault(name, promiser)
            elif value.get("default", None) is not None:
                attribute_dict.setdefault(name, copy(value["default"]))
            else:
                attribute_dict.setdefault(name, None)

        # Finally, convert to an object:
        return AttributeObject(attribute_dict)


    def _validate_attributes(self, promiser, attributes):
        if not self._has_validation_attributes:
            # Can only validate attributes if module
            # provided typings for attributes
            return
        self.create_attribute_object(promiser, attributes)
        return # Only interested in exceptions, return None

    def _handle_init(self):
        self._result = self.protocol_init(None)
        self._add_result()
        _put_response(self._response, self._out, self._record_file)

    def _handle_validate(self, promiser, attributes, request):
        try:
            self.validate_attributes(promiser, attributes)
            returned = self.validate_promise(promiser, attributes)
            if returned is None:
                # Good, expected
                self._result = Result.VALID
            else:
                # Bad, validate method shouldn't return anything else
                self.log_critical(f"Bug in promise module {self.name} - validate_promise() should not return anything")
                self._result = Result.ERROR
        except ValidationError as e:
            message = str(e)
            if "promise_type" in request:
                message += f" for {request['promise_type']} promise with promiser '{promiser}'"
            else:
                message += f" for promise with promiser '{promiser}'"
            if "filename" in request and "line_number" in request:
                message += f" ({request['filename']}:{request['line_number']})"

            self.log_error(message)
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

    def prepare_promiser_and_attributes(self, promiser, attributes):
        '''Override if you want to modify promiser or attributes before validate or evaluate'''
        return (promiser, attributes)

    def validate_attributes(self, promiser, attributes):
        '''Override this if you want to prevent automatic validation'''
        return self._validate_attributes(promiser, attributes)

    def validate_promise(self, promiser, attributes):
        '''Must override this or use validation through self.add_attribute()'''
        if not self._has_validation_attributes:
            raise NotImplementedError("Promise module must implement validate_promise")

    def evaluate_promise(self, promiser, attributes):
        raise NotImplementedError("Promise module must implement evaluate_promise")

    def protocol_terminate(self):
        return Result.SUCCESS
