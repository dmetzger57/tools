#!/usr/bin/env python3

import argparse
import os
import sqlite3
import sys
from datetime import datetime

DB_DIR = os.path.expanduser("~/db/FileTracker")

def query_db(db_path):
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        cursor.execute("SELECT id, file_name, full_path, size, created, last_modified, owner, checksum FROM files ORDER BY file_name")
        rows = cursor.fetchall()
        conn.close()
        return rows
    except sqlite3.Error as e:
        print(f"Error accessing {db_path}: {e}", file=sys.stderr)
        return []

def format_record(row):
    id_, file_name, full_path, size, created, last_modified, owner, checksum = row
    return (
        f"ID: {id_}\n"
        f"File Name: {file_name}\n"
        f"Full Path: {full_path}\n"
        f"Size: {size} bytes\n"
        f"Created: {datetime.fromtimestamp(created)}\n"
        f"Last Modified: {datetime.fromtimestamp(last_modified)}\n"
        f"Owner: {owner}\n"
        f"Checksum: {checksum}\n"
        f"{'-'*60}"
    )

def main():
    parser = argparse.ArgumentParser(description="Generate report from a file_tracker database")
    parser.add_argument("-d", "--db", required=True, help="Database name inside ~/db/FileTracker")
    args = parser.parse_args()

    db_path = os.path.join(DB_DIR, args.db)
    if not os.path.exists(db_path):
        print(f"Database {args.db} not found in {DB_DIR}", file=sys.stderr)
        sys.exit(1)

    rows = query_db(db_path)
    if not rows:
        print("No records found.")
        sys.exit(0)

    for row in rows:
        print(format_record(row))

if __name__ == "__main__":
    main()
