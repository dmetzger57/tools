# tools

This repository contains my personal tools.

## file_tracker
Creates a SQL database of all files in a specified path containing the file name. checksum, last modified data and owner

The databases are stored in the \${USER}/Desktop/db/FileTracker folder

### Usage:
file_tracker [-v] [-p path] [-d db_name]<br>
-u: Update the database for files that have been added, deleted or modified<br>
-v: Compare by calculating the current checksum, without the -v the last modified time is used to verify<br>
-p: Full path of the directory structure to be processed<br>
-d: Name of the database which will reside in \$HOME/db/FileTracker folder. The name will have '.db' added as a suffix.

## find_file

Searches the specified (or all) file_tracker databases for a specific file (exact name match)

### Requirements

Create a CLI tool to search for a specific file in the file_tracker database(s)

The commad requires the following option:<br>
-f FileName<br>
Where FileName is the name of the file being searched for

The command optionally accepts the following option:<br>
-d DbName<br>
Where DbName is the name of the file_tracker database to be searched. If the -d option is not provided the command will search all file_tracker databases.

The file_tracker databases are stored in \${HOME}/db/FileTracker

If a record exists in the database, dispay the record in human friendly format

If a record does not exist in the database, display 'File Not Found' message

The database schema wad created by: CREATE TABLE files (id INTEGER PRIMARY KEY, file_name TEXT, full_path TEXT UNIQUE, size INTEGER, created INTEGER, last_modified INTEGER, owner TEXT, checksum TEXT);

### Usage
find_file -f file_name [-d db_name]

## report_files

Display data regarding all the files recorded in the given file_tracker database

### Requirements
Create a CLI tool to display information on all the files recorded in the file_tracker database

The commad requires the following option:<br>
-d DbName<br>
Where DbName is the name of the file_tracker database to be processed.

The file_tracker databases are stored in \${HOME}/db/FileTracker

Produce a report with 1 row per file printed in human friendly format

The database schema wad created by: CREATE TABLE files (id INTEGER PRIMARY KEY, file_name TEXT, full_path TEXT UNIQUE, size INTEGER, created INTEGER, last_modified INTEGER, owner TEXT, checksum TEXT);

### Usage
report_files -d db_name

## weather_data

Pulls weather data from meteostat.p.rapidapi.com for a specified date range. The output is a CSV file named weather\_data\_\${START}\_to\_\${END}.csv.

### CSV Data

- Temp (Min)
- Temp (Max)
- Temp (Avg)
- Dew Point
- Relative Humidity
- Wind Speed
- Barometric Pressure
- Precipitation
- Rainfall

### Usage
<p>
weather_data Start_Date End_Date<br>
Date Format: YYYY-MM-DD
</p>
