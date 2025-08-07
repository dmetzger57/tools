# tools

This repository contains my personal tools.


## files_change_audit.py

### Requirements

<p>Use a SQL database containing the following information for each file under a specified folder with the database with the database stored in the folder named db under the users home directory. The name of the database can either be specified by the command line option “--db database_name” or the script should prompt for the database name. </p>

<p>The script should prompt for the pathname of the root directory to process unless it is specified by the command line argument “--root path_name”.</p>

<p>The script should recursively process all folders under the root directory</p>

<p>If a file in the file system is not currently in the database</p>
- Create an entry in the database
- Print a line indicating the file was added in the format “Added - file_name

<p>If a file is in the database but no longer on the file system</p>
- Print a warning in the format ***“Missing - file_name”***

<p>If a file on the file system has changed</p>
- Print a warning in the format ***“Changed - file name”***

<p>When all folder / files have been processed, print the following statistics:</p>
- Number of folders processed
- Number of files processed
- Number of files added
- Number of files missing
- Number of files changed

### Usage

<p>files_changed_audit --db database_file_name --root path_name_to_be_processed</p>

<p>When run without parameters, the scipt will prompt for the database file name and path name to be inventoried.</p>

## archive_changed_files.sh

### Requirements

<p>Create zipped archive of files in ~/Documents changed between two dates specified by "-s" as the start date and "-e" as the end date using the format "mm-dd-yy". Store the archive in /Volumes/dockdata/iCloudArchive using the name format "Documents_Changed_MM-DD-YY_to_MM-DD-YY"</p>

