#!/bin/bash

OUTPUT_FILE="combined_output.txt"

> "$OUTPUT_FILE"

for filename in *; do

    if [ -f "$filename" ]; then

        if [ "$filename" != "$OUTPUT_FILE" ] && [ "$filename" != "$(basename "$0")" ]; then

            echo "=====================================" >> "$OUTPUT_FILE"
            echo "File Name: $filename" >> "$OUTPUT_FILE"
            echo "=====================================" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"

            cat "$filename" >> "$OUTPUT_FILE"

            echo "" >> "$OUTPUT_FILE"
            echo "" >> "$OUTPUT_FILE"
        fi
    fi
done

echo "All files have been combined into $OUTPUT_FILE"