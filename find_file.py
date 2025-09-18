#!/usr/bin/env python3

import argparse
import os
import sqlite3
import sys
from datetime import datetime

DB_DIR = os.path.expanduser("~/db/FileTracker")

def query_db(db_path, file_name):
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        cursor.execute("SELECT id, file_name, full_path, size, created, last_modified, owner, checksum FROM files WHERE file_name = ?", (file_name,))
        row = cursor.fetchone()
        conn.close()
        return row
    except sqlite3.Error as e:
        print(f"Error accessing {db_path}: {e}", file=sys.stderr)
        return None

def format_record(row):
    if not row:
        return None
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
    )

def main():
    parser = argparse.ArgumentParser(description="Search for a file in file_tracker databases")
    parser.add_argument("-f", "--file", required=True, help="File name to search for")
    parser.add_argument("-d", "--db", help="Specific database name to search (otherwise search all)")
    args = parser.parse_args()

    file_name = args.file
    db_name = args.db

    dbs_to_search = []
    if db_name:
        db_path = os.path.join(DB_DIR, db_name)
        if not os.path.exists(db_path):
            print(f"Database {db_name} not found in {DB_DIR}", file=sys.stderr)
            sys.exit(1)
        dbs_to_search.append(db_path)
    else:
        dbs_to_search = [os.path.join(DB_DIR, f) for f in os.listdir(DB_DIR) if f.endswith(".db")]

    found = False
    for db in dbs_to_search:
        row = query_db(db, file_name)
        if row:
            print(f"\nDatabase: {os.path.basename(db)}")
            print(format_record(row))
            found = True

    if not found:
        print("File Not Found")

if __name__ == "__main__":
    main()
