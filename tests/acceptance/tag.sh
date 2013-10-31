#!/bin/sh

folder=$1
project_id=$2
category_id=$3

echo "Tagging files on $folder with $project_id, $category_id";

for i in `find $folder -name "*.cf" -print`;
do
    echo "### PROJECT_ID: $project_id" >> $i
    echo "### CATEGORY_ID: $category_id" >> $i
done
exit 0
