#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/attr.h>

// macOS-specific creation date setter
int set_creation_time(const char *path, const struct timespec *ts) {
    struct attrlist attrList;
    memset(&attrList, 0, sizeof(attrList));
    attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrList.commonattr = ATTR_CMN_CRTIME;

    struct {
        struct timespec crtime;
    } attrBuf;

    attrBuf.crtime = *ts;

    int result = setattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), 0);
    if (result != 0) {
        perror("setattrlist (creation time)");
        return -1;
    }
    return 0;
}

int update_file_time(const char *filename, time_t t) {
    struct timeval times[2];
    times[0].tv_sec = t; times[0].tv_usec = 0; // access
    times[1].tv_sec = t; times[1].tv_usec = 0; // modification

    if (utimes(filename, times) != 0) {
        perror("utimes");
        return -1;
    }

    struct timespec ts;
    ts.tv_sec = t;
    ts.tv_nsec = 0;

    if (set_creation_time(filename, &ts) != 0) {
        fprintf(stderr, "Failed to set creation date for %s\n", filename);
        return -1;
    }

    printf("Updated %s\n", filename);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s \"YYYY-MM-DD HH:MM:SS\" file1.jpg [file2.jpg ...]\n", argv[0]);
        return 1;
    }

    const char *datetime = argv[1];

    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    if (strptime(datetime, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
        fprintf(stderr, "Invalid datetime format. Use YYYY-MM-DD HH:MM:SS\n");
        return 1;
    }

    time_t t = mktime(&tm);
    if (t == -1) {
        perror("mktime");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        update_file_time(argv[i], t);
    }

    return 0;
}
