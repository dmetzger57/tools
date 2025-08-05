# tools
Personal Tools


###
# files_change_audit.py #
###

    Requirements
    ------------
	    Use a SQL database containing the following information for
        each file under a specified folder with the database with the
        database stored in the folder named db under the users home
        directory. The name of the database can either be specified
        by the command line option “--db database_name” or the script
        should prompt for the database name. 

	    The script should prompt for the pathname of the root
        directory to process unless it is specified by the command
        line argument “--root path_name”.

	    The script should recursively process all folders under the
        root directory

	    If a file in the file system is not currently in the database
		    - Create an entry in the database
		    - Print a line indicating the file was added in the format
              “Added - file_name”

	    If a file is in the database but no longer on the file system
		    - Print a warning in the format “Missing - file_name”

	    If a file on the file system has changed
		    - Print a warning in the format “Changed - file name”

	    When all folder / files have been processed, print the
        following statistics
		    - Number of folders processed
		    - Number of files processed
		    - Number of files added
		    - Number of files missing
		    - Number of files changed

    Usage
    -----

	    files_changed_audit without parameters, the scipt will prompt
        for the database file name and path name to be inventoried.

	    files_changed_audit --db database_file_name --root path_name_to_be_processed


###
# archive_changed_files.sh
###

    Requirements
    ------------
	    Create zipped archive of files in ~/Documents changed between
        two dates specified by "-s" as the start date and "-e" as the
        end date using the format "mm-dd-yy". Store the archive in
        /Volumes/dockdata/iCloudArchive using the name format
        "Documents_Changed_MM-DD-YY_to_MM-DD-YY"

