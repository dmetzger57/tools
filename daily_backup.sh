#!/bin/bash

# === Configuration ===
SOURCE="Path To Folder Structure" # Edit Me
BACKUP_FOLDER="Path To Folder Holding The Backups" # Edit Me

# === Setup ===
BASE_FOLDER="$BACKUP_FOLDER/Base"
TODAY=$(date +%F)   # YYYY-MM-DD format
DAILY_FOLDER="$BACKUP_FOLDER/$TODAY"

mkdir -p "$BACKUP_FOLDER"

# === Initial full backup ===
if [ ! -d "$BASE_FOLDER" ]; then
    echo "Performing initial full backup..."
    mkdir -p "$BASE_FOLDER"
    rsync -a --delete "$SOURCE/" "$BASE_FOLDER/"
    echo "Initial backup completed to $BASE_FOLDER"
    exit 0
fi

# === Incremental backup ===
if [ -d "$DAILY_FOLDER" ]; then
    echo "Backup for $TODAY already exists at $DAILY_FOLDER"
    exit 0
fi

echo "Performing incremental backup for $TODAY..."
mkdir -p "$DAILY_FOLDER"

# Copy only changed files since last backup
rsync -a --update --delete --compare-dest="$BASE_FOLDER" "$SOURCE/" "$DAILY_FOLDER/"

echo "Incremental backup completed to $DAILY_FOLDER"
