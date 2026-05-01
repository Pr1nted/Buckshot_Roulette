#!/bin/bash

# Make matching case-insensitive (catches .JPG, .png, .Ico, etc.)
shopt -s nocasematch

OUTPUT_FILE="combined_output.txt"

# Clear the output file if it already exists
> "$OUTPUT_FILE"

for filename in *; do
    # Only process regular files (ignore directories)
    if [ -f "$filename" ]; then

        # Skip the output file and the script itself
        if [ "$filename" != "$OUTPUT_FILE" ] && [ "$filename" != "$(basename "$0")" ]; then

            # Check if the file ends with an image or icon extension
            case "$filename" in
                *.jpg|*.jpeg|*.png|*.gif|*.bmp|*.svg|*.webp|*.ico|*.icns|*.tiff)
                    # If it matches, skip to the next file
                    continue
                    ;;
            esac

            # If it makes it past the check, append it to the output file
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

echo "All text files have been combined into $OUTPUT_FILE"