#!/usr/bin/env python3

import os
import sqlite3
import hashlib
import argparse
from pathlib import Path

# --- Argument parsing ---
parser = argparse.ArgumentParser(description="Audit and track file inventory.")
parser.add_argument("--db", type=str, help="Database name (no path)")
parser.add_argument("--root", type=str, help="Root directory to scan")

args = parser.parse_args()

# Prompt for DB name if not provided
db_name = args.db or input("Enter the database name (without .db extension): ").strip()
if not db_name.endswith(".db"):
    db_name += ".db"

# Prompt for root directory if not provided
root_dir = args.root or input("Enter the root directory to scan: ").strip()
if not os.path.isdir(root_dir):
    print("❌ Invalid root directory.")
    exit(1)

# Setup DB path in ~/db
db_dir = Path.home() / "db"
db_dir.mkdir(exist_ok=True)
db_path = db_dir / db_name

# --- Connect to database ---
conn = sqlite3.connect(db_path)
cursor = conn.cursor()

cursor.execute('''
CREATE TABLE IF NOT EXISTS files (
    filename TEXT,
    checksum TEXT,
    full_path TEXT PRIMARY KEY
)
''')
conn.commit()

# --- Statistics counters ---
folders_processed = 0
files_processed = 0
files_added = 0
files_missing = 0
files_changed = 0
seen_paths = set()

# --- Checksum utility ---
def compute_checksum(path):
    sha256 = hashlib.sha256()
    try:
        with open(path, 'rb') as f:
            while chunk := f.read(8192):
                sha256.update(chunk)
        return sha256.hexdigest()
    except Exception as e:
        print(f"⚠️ Error reading {path}: {e}")
        return None

# --- Scan and update database ---
for folder, _, files in os.walk(root_dir):
    folders_processed += 1
    for fname in files:
        files_processed += 1
        full_path = os.path.abspath(os.path.join(folder, fname))
        seen_paths.add(full_path)

        checksum = compute_checksum(full_path)
        if checksum is None:
            continue

        cursor.execute("SELECT checksum FROM files WHERE full_path = ?", (full_path,))
        row = cursor.fetchone()

        if row is None:
            cursor.execute(
                "INSERT INTO files (filename, checksum, full_path) VALUES (?, ?, ?)",
                (fname, checksum, full_path)
            )
            print(f"Added - {fname}")
            files_added += 1
        else:
            old_checksum = row[0]
            if old_checksum != checksum:
                print(f"Changed - {fname}")
                files_changed += 1
                cursor.execute(
                    "UPDATE files SET checksum = ? WHERE full_path = ?",
                    (checksum, full_path)
                )

conn.commit()

# --- Check for missing files ---
cursor.execute("SELECT full_path, filename FROM files")
for full_path, fname in cursor.fetchall():
    if full_path not in seen_paths and not os.path.exists(full_path):
        print(f"Missing - {fname}")
        files_missing += 1

# --- Summary ---
print("\n--- Summary ---")
print(f"Folders processed: {folders_processed}")
print(f"Files processed: {files_processed}")
print(f"Files added: {files_added}")
print(f"Files missing: {files_missing}")
print(f"Files changed: {files_changed}")

conn.close()
