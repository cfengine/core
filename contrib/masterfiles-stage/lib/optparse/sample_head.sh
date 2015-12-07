#!/usr/bin/env bash

# Source the optparse.bash file ---------------------------------------------------
source optparse.bash
# Define options
optparse.define short=f long=file desc="The file to process" variable=file
optparse.define short=o long=output desc="The output file" variable=output default=head_output.txt
optparse.define short=l long=lines desc="The number of lines to head (default:5)" variable=lines default=5
optparse.define short=v long=verbose desc="Flag to set verbose mode on" variable=verbose_mode value=true default=false
# Source the output file ----------------------------------------------------------
source $( optparse.build )

if [ "$file" == "" ]; then
	echo "ERROR: Please provide a file"
	exit 1
fi

# Display arguments
if [ "$verbose_mode" = "true" ]; then
	echo "Verbose mode ON"	
	echo "FILE  : $file"
	echo "OUTPUT: $output"
	echo "LINES : $lines"
fi

# Check if input file exists
if [ "$verbose_mode" = "true" ]; then echo "Checking input file $file..." ; fi
if [ ! -f $file ]; then
	echo "File does not exist"
	exit 1
fi

if [ "$verbose_mode" = "true" ]; then echo "Heading first $lines lines into $output..." ; fi
cat $file | head -n $lines > $output

if [ "$verbose_mode" = "true" ]; then echo "Done."; fi

exit 0
	
