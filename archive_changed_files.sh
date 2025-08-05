#!/bin/bash

# === DEFAULT VALUES ===
SOURCE_DIR="$HOME/Documents"
DEST_DIR="/Volumes/dockdata/iCloudArchive"
DATE_FORMAT="+%m-%d-%y"

# === HELP FUNCTION ===
usage() {
  echo "Usage: $0 -s start_date -e end_date"
  echo "  Dates must be in mm-dd-yy format (e.g., 08-01-25)"
  exit 1
}

# === PARSE ARGUMENTS ===
while getopts "s:e:" opt; do
  case $opt in
    s) START_DATE_RAW="$OPTARG" ;;
    e) END_DATE_RAW="$OPTARG" ;;
    *) usage ;;
  esac
done

# === VALIDATION ===
if [ -z "$START_DATE_RAW" ] || [ -z "$END_DATE_RAW" ]; then
  usage
fi

# === CONVERT TO YYYY-MM-DD ===
START_DATE=$(date -jf "%m-%d-%y" "$START_DATE_RAW" +"%Y-%m-%d") || exit 1
END_DATE=$(date -jf "%m-%d-%y" "$END_DATE_RAW" +"%Y-%m-%d") || exit 1

# === SAFETY CHECKS ===
if [ ! -d "$SOURCE_DIR" ]; then
  echo "Error: Source directory $SOURCE_DIR does not exist."
  exit 1
fi

if [ ! -d "$DEST_DIR" ]; then
  echo "Error: Destination directory $DEST_DIR does not exist."
  exit 1
fi

# === SET ARCHIVE NAME ===
ARCHIVE_NAME="Documents_Changed_${START_DATE_RAW}_to_${END_DATE_RAW}.zip"

# === FIND & ZIP FILES ===
TMP_LIST=$(mktemp)

find "$SOURCE_DIR" -type f -newermt "$START_DATE" ! -newermt "$END_DATE" > "$TMP_LIST"

if [ -s "$TMP_LIST" ]; then
  zip -@ "$DEST_DIR/$ARCHIVE_NAME" < "$TMP_LIST"
  echo "✅ Archive created: $DEST_DIR/$ARCHIVE_NAME"
else
  echo "ℹ️ No files modified between $START_DATE_RAW and $END_DATE_RAW."
fi

# === CLEAN UP ===
rm "$TMP_LIST"
