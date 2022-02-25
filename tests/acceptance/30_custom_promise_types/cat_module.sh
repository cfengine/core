log() {
        echo "$1" >>/tmp/module.log
}

reset_operation() {
    # reset operation name to default value
    request_operation=""
}

handle_input_line() {
    # Split the line of input on the first '=' into 2 - key and value
    IFS='=' read -r key value <<< "$1"
    
    # save operation name, if that's what the line is about
    if [ "$key" = operation ]; then
        request_operation="$value"
    fi
}

receive_request() {
    # Read lines from input until empty line
    # Call handle_input_line for each non-empty line
    while IFS='$\n' read -r line; do
        # Different CIs (Travis and Jenkins) run cf-agent with different verbosity settings.
        # This test accepts two verbosity settings, replacing one of them with another.
        log "`echo "$line" | sed 's/^log_level=notice$/log_level=info/'`"
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

perform_operation() {
    case "$request_operation" in 
        validate_promise)
            response_result="valid" ;;
        evaluate_promise)
            response_result="kept" ;;
        terminate)
            response_result="success"
            write_response
            exit 0
            ;;
        *)
            response_result="error" ;;
    esac
    write_response
}

handle_request() {
    reset_state         # 1. Reset global variables
    receive_request     # 2. Receive / parse an operation from agent
    perform_operation   # 3. Perform operation (validate, evaluate, terminate)
}

skip_header() {
    # Skip until (and including) the first empty line
    while IFS='$\n' read -r line; do
        # save header to the log, stripping CFEngine version
        log "`echo "$line" | sed 's/ 3\.[0-9][0-9]\.[0-9][^ ]* / xxx /'`"
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
