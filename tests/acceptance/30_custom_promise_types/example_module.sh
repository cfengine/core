reset_state() {
    # Set global variables before we begin another request

    # Variables parsed directly from request:
    request_operation=""
    request_log_level=""
    request_promise_type=""
    request_promiser=""
    request_attribute_message=""

    # Variables to put into response:
    response_result=""

    # Other state:
    saw_unknown_key="no"
    saw_unknown_attribute="no"
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
    attribute_message)
        request_attribute_message="$value" ;;
    attribute_*)
        attribute_name=${key#"attribute_"}
        log error "Unknown attribute: '$attribute_name'"
        saw_unknown_attribute="yes" ;;
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
    write_response
    exit 0
}

operation_validate() {
    response_result="valid"
    if [ "$saw_unknown_attribute" != "no" ] ; then
        response_result="invalid"
    fi

    if [ "$request_promiser" = "" ] ; then
        log error "Promiser must be non-empty"
        response_result="invalid"
    fi

    if [ "$request_attribute_message" = "" ] ; then
        log error "Attribute 'message' is missing or empty"
        response_result="invalid"
    fi

    write_response
}

operation_evaluate() {
    local safe_promiser="$(echo "$request_promiser" | sed 's/,/_/g')"
    local classes=""

    local existed_before=0
    if [ -f "$request_promiser" ]; then
        existed_before=1
    fi

    if grep -q "$request_attribute_message" "$request_promiser" 2>/dev/null ; then
        response_result="kept"
        classes="${safe_promiser}_content_as_promised"
    else
        response_result="repaired"
        echo "$request_attribute_message" > "$request_promiser" && {
          printf "log_info=Updated file '%s' with content '%s'\n" "$request_promiser" "$request_attribute_message"
          if [ $existed_before = 0 ]; then
            classes="${safe_promiser}_created,${safe_promiser}_content_updated"
          else
            classes="${safe_promiser}_content_updated"
          fi
        } || response_result="not_kept"
    fi

    if ! grep -q "$request_attribute_message" "$request_promiser" 2>/dev/null ; then
        response_result="not_kept"
        if [ -z "$classes" ]; then
          classes="${safe_promiser}_content_update_failed"
        else
          classes="${classes},${safe_promiser}_content_update_failed"
        fi
    fi

    if [ -n "$classes" ]; then
        echo "result_classes=$classes"
    fi
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

# Skip the protocol header given by agent:
skip_header

# Write our header to request line based protocol:
echo "example_promises 0.0.1 v1 line_based"
echo ""

# Loop indefinitely, handling requests:
while true; do
    handle_request
done

# Should never get here.
