#!/bin/bash

OUTPUT_DIR="files"
NUM_FILES=10
LINES_PER_FILE=50
SEARCH_STR="hello"

# Создаём каталог
mkdir -p "$OUTPUT_DIR"

# Генерируем файлы
for i in $(seq 1 $NUM_FILES); do
    FILE="$OUTPUT_DIR/file_$i.txt"
    echo "Generating $FILE..."
    > "$FILE"

    for j in $(seq 1 $LINES_PER_FILE); do
        if (( RANDOM % 5 == 0 )); then
            echo "$SEARCH_STR world $j" >> "$FILE"
        else
            echo "random line $j" >> "$FILE"
        fi
    done
done

LIST_FILE="file_list.txt"
> "$LIST_FILE"
for f in "$OUTPUT_DIR"/*.txt; do
    echo "$f" >> "$LIST_FILE"
done

echo "Generated $NUM_FILES files in $OUTPUT_DIR and list in $LIST_FILE"
