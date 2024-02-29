#!/bin/bash

# Get the current directory
folder=$(pwd)

# Iterate over each .tex file in the current directory
for file in "$folder"/*.tex; do
    # Check if the file exists and is a regular file
    if [ -f "$file" ]; then
        # Get the base filename without the extension
        filename=$(basename -- "$file")
        filename_no_ext="${filename%.*}"

        # Run pdflatex to compile the .tex file
        pdflatex -interaction=batchmode -output-directory="$folder" "$file"

        # Check if pdflatex compilation was successful
        if [ $? -eq 0 ]; then
            echo "Compilation successful: $filename"

            # Convert PDF to PNG
            convert -density 300 "$filename_no_ext.pdf" -quality 100 "$filename_no_ext.png"

            # Check if PNG conversion was successful
            if [ $? -eq 0 ]; then
                echo "PNG conversion successful: $filename_no_ext.pdf"
            else
                echo "Error converting PDF to PNG: $filename_no_ext.pdf"
            fi
        else
            echo "Error compiling: $filename"
        fi
    fi
done
