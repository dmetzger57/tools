# tools

This repository contains my personal tools.

## file_tracker

<p>Creates a SQL database of all files in a specified path containing the file name. checksum, last modified data and owner.</p>

<p>The database is stored in the folder 'db' located in the Home directory.</>

<p>Usage:
file_tracker [-v] [-p path] [-d db_name]<br>
-u: Update the database for files that habe been added, deleted or modified<br>
-v: Compare by calculating the current checksum, without the -v the last modified time is used to verify<br>
-p: Full path of the directory structure to be processed<br>
-d: Name of the database which will reside in $HOME/db. The name will have '.db' added as a suffix.
</p>

## archive\_changed\_files.sh

### Requirements

<p>Create zipped archive of files in ~/Documents changed between two dates specified by "-s" as the start date and "-e" as the end date using the format "mm-dd-yy". Store the archive in /Volumes/dockdata/iCloudArchive using the name format "Documents_Changed_MM-DD-YY_to_MM-DD-YY"</p>

