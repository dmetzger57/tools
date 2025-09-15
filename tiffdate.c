#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s -d \"YYYY:MM:DD HH:MM:SS\" file1.tif [file2.tif ...]\n", progname);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        usage(argv[0]);
    }

    char *date = NULL;
    int i = 1;

    // Parse arguments
    if (strcmp(argv[i], "-d") == 0) {
        if (i + 1 >= argc) usage(argv[0]);
        date = argv[i + 1];
        i += 2;
    } else {
        usage(argv[0]);
    }

    if (date == NULL) {
        fprintf(stderr, "Error: no date specified\n");
        return 1;
    }

    // Validate date format (basic check, must be "YYYY:MM:DD HH:MM:SS")
    if (strlen(date) != 19 || date[4] != ':' || date[7] != ':' || date[10] != ' ' ||
        date[13] != ':' || date[16] != ':') {
        fprintf(stderr, "Error: date format must be \"YYYY:MM:DD HH:MM:SS\"\n");
        return 1;
    }

    // Process each TIFF file
    for (; i < argc; i++) {
        char *filename = argv[i];
        TIFF *tif = TIFFOpen(filename, "r+");
        if (!tif) {
            fprintf(stderr, "Error opening %s\n", filename);
            continue;
        }

        // Set the DateTime tag (TIFFTAG_DATETIME = 306)
        if (TIFFSetField(tif, TIFFTAG_DATETIME, date) != 1) {
            fprintf(stderr, "Failed to set DateTime for %s\n", filename);
            TIFFClose(tif);
            continue;
        }

        // Rewrite directory to save changes
        if (!TIFFRewriteDirectory(tif)) {
            fprintf(stderr, "Failed to rewrite directory for %s\n", filename);
        } else {
            printf("Updated creation date in %s to %s\n", filename, date);
        }

        TIFFClose(tif);
    }

    return 0;
}
