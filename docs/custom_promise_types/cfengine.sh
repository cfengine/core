#!/bin/false
#
# This file should be sourced, not run

log() {
    if [ "$#" != 2 ] ; then
        echo "log_critical=Error in promise module (log must be used with 2 arguments, level and message)"
        exit 1
    fi
    level="$1"
    message="$2"
    echo "log_$level=$message"
}

reset_state() {
    # Set global variables before we begin another request

    # Variables parsed directly from request:
    request_operation=""
    request_log_level=""
    request_promise_type=""
    request_promiser=""

    for var in $(env | cut -d '=' -f 1 | grep '^request_attribute_'); do
        unset $var
    done

    # Variables to put into response:
    response_result=""

    # Other state:
    saw_unknown_key="no"
    saw_unknown_attribute="no"

    unknown_attribute_names=""
}

handle_input_line() {

    # Split the line of input on the first '=' into 2 - key and value
    IFS='=' read -r key value <<< "$1"

    case "$key" in
        operation)
            request_operation="$value" ;;
        log_level)
            request_log_level="$value" ;;
        promise_type)
            request_promise_type="$value" ;;
        promiser)
            request_promiser="$value" ;;
        attribute_*)
            attribute_name=${key#"attribute_"}
            if ! expr " $required_attributes $optional_attributes " : ".* $attribute_name " >/dev/null; then
                saw_unknown_attribute="yes"
                unknown_attribute_names="$unknown_attribute_names, $attribute_name"
            fi
            eval "request_${key}=\$value" ;;
        *)
            saw_unknown_key="yes" ;;
    esac
}

receive_request() {
    # Read lines from input until empty line
    # Call handle_input_line for each non-empty line
    while IFS='$\n' read -r line; do
        if [ "x$line" = "x" ] ; then
            break
        fi
        handle_input_line "$line" # Parses a key=value pair
    done
}

write_response() {
    echo "operation=$request_operation"
    echo "result=$response_result"
    echo ""
}

operation_terminate() {
    response_result="success"
    type do_terminate >/dev/null 2>&1 && do_terminate
    write_response
    exit 0
}

operation_validate() {
    response_result="valid"
    if [ "$saw_unknown_attribute" != "no" -a "$all_attributes_are_valid" != "yes" ] ; then
        log error "Unknown attribute/s: ${unknown_attribute_names#, }"
        response_result="invalid"
    fi

    if [ ! -z "$required_attributes" ]; then
        for attribute_name in $required_attributes; do
            # Note: ${!varname} syntax expands to value of variable, which name
            # is saved in $varname variable. Example:
            # var_1=something
            # varname=var_1
            # echo "${!varname}" # prints "something"
            varname="request_attribute_$attribute_name"
            if [ -z "${!varname}" ]; then
                log error "Attribute '$attribute_name' is missing or empty"
                response_result="invalid"
            fi
        done
    fi

    type do_validate >/dev/null 2>&1 && do_validate
    write_response
}

operation_evaluate() {
    response_result="error" # it's responsibility of do_evaluate to override this
    type do_evaluate >/dev/null 2>&1 && do_evaluate
    write_response
}

operation_unknown() {
    response_result="error"
    log error "Promise module received unexpected operation: $request_operation"
    write_response
}

perform_operation() {
    case "$request_operation" in
        validate_promise)
            operation_validate ;;
        evaluate_promise)
            operation_evaluate ;;
        terminate)
            operation_terminate ;;
        *)
            operation_unknown ;;
    esac
}

handle_request() {
    reset_state         # 1. Reset global variables
    receive_request     # 2. Receive / parse an operation from agent
    perform_operation   # 3. Perform operation (validate, evaluate, terminate)
}

skip_header() {
    # Skip until (and including) the first empty line
    while IFS='$\n' read -r line; do
        if [ "x$line" = "x" ] ; then
            return;
        fi
    done
}

module_main() {
    # Check arguments provided by the caller. Must have two arguments with no spaces.
    if [ "$#" != 2 ] || expr "$1$2" : ".* " >/dev/null; then
        exit 1
    fi
    module_name="$1"
    module_version="$2"

    # Skip the protocol header given by agent:
    skip_header

    # Write our header to request line based protocol:
    echo "$module_name $module_version v1 line_based"
    echo ""

    type do_initialize >/dev/null 2>&1 && do_initialize

    # Loop indefinitely, handling requests:
    while true; do
        handle_request
    done

    # Should never get here.
}
