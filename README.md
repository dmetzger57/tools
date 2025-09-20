# tools

This repository contains my personal tools.

## file_tracker
Creates a SQL database of all files in a specified path containing the file name. checksum, last modified data and owner

The databases are stored in the \${USER}/Desktop/db/FileTracker folder

### Usage:
file_tracker [-v] [-p path] [-d db_name]<br>

* -u: Update the database for files that have been added, deleted or modified<br>
* -v: Compare by calculating the current checksum, without the -v the last modified time is used to verify<br>
* -p: Full path of the directory structure to be processed<br>
* -d: Name of the database which will reside in \$HOME/db/FileTracker folder. The name will have '.db' added as a suffix.


## find_locator

Searches the specified (or all) file_tracker databases for a specific file (exact name match)

### Syntax
find_locator -f file_name [-d db_name] [-p] [-v]

* -f file_name
* -d database_name
* -p Match partia file names
* -v Verbose output

## weather_data

Pulls weather data from meteostat.p.rapidapi.com for a specified date range. The output is a CSV file named weather\_data\_\${START}\_to\_\${END}.csv.

### Syntax
weather_data Start_Date End_Date<br>
Date Format: YYYY-MM-DD

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