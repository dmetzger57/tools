#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>
#include <sqlite3.h>
#include <openssl/evp.h>

#define HASH_SIZE 65
#define MAX_PATH 4096

// ==== Globals ====
int number_of_threads = 4;
int update = 0;
int unchangedCount = 0, changedCount = 0, newCount = 0, missingCount = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER; // protect counters

typedef struct FileNode {
    char path[MAX_PATH];
    char name[256];
    struct FileNode *next;
} FileNode;

FileNode *queue_head = NULL, *queue_tail = NULL;
int done_traversal = 0; // signals producer finished
char global_db_path[1024];
int verifyChecksum = 0;
int verbose = 0;

// ==== Queue functions ====
void enqueue(const char *path, const char *name) {
    FileNode *node = malloc(sizeof(FileNode));
    strncpy(node->path, path, MAX_PATH);
    strncpy(node->name, name, 256);
    node->next = NULL;

    pthread_mutex_lock(&queue_mutex);
    if (!queue_tail) {
        queue_head = queue_tail = node;
    } else {
        queue_tail->next = node;
        queue_tail = node;
    }
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

FileNode *dequeue() {
    pthread_mutex_lock(&queue_mutex);
    while (!queue_head && !done_traversal) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    if (!queue_head) {
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }
    FileNode *node = queue_head;
    queue_head = queue_head->next;
    if (!queue_head) queue_tail = NULL;
    pthread_mutex_unlock(&queue_mutex);
    return node;
}

// ==== Utility functions ====
void get_owner(uid_t uid, char *owner, size_t size) {
    struct passwd *pw = getpwuid(uid);
    if (pw) snprintf(owner, size, "%s", pw->pw_name);
    else snprintf(owner, size, "%d", uid);
}

void compute_sha256(const char *path, char *outputBuffer) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        strcpy(outputBuffer, "");
        return;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        strcpy(outputBuffer, "");
        return;
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

    const int bufSize = 32768;
    unsigned char *buffer = malloc(bufSize);
    if (!buffer) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        strcpy(outputBuffer, "");
        return;
    }

    int bytesRead;
    while ((bytesRead = fread(buffer, 1, bufSize, file))) {
        EVP_DigestUpdate(mdctx, buffer, bytesRead);
    }

    EVP_DigestFinal_ex(mdctx, hash, &hash_len);

    for (unsigned int i = 0; i < hash_len; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[hash_len * 2] = '\0';

    EVP_MD_CTX_free(mdctx);
    fclose(file);
    free(buffer);
}

// ==== File processor ====
void process_file(const char *path, const char *name, sqlite3 *db) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT last_modified, checksum FROM files WHERE full_path = ?";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        time_t db_mtime = sqlite3_column_int64(stmt, 0);
        const char *db_checksum = (const char *)sqlite3_column_text(stmt, 1);

        if (!verifyChecksum && db_mtime == st.st_mtime) {
            if (verbose == 1) {
                printf("Unchanged: %s\n", path);
	    }
            pthread_mutex_lock(&count_mutex);
            unchangedCount++;
            pthread_mutex_unlock(&count_mutex);
        } else {
            char checksum[HASH_SIZE];
	    if( verifyChecksum) {
            	compute_sha256(path, checksum);
	    }
            if (verifyChecksum && strcmp(checksum, db_checksum) == 0) {
		if( verbose == 1 ) {
                    printf("Unchanged: %s\n", path);
		}
                pthread_mutex_lock(&count_mutex);
                unchangedCount++;
                pthread_mutex_unlock(&count_mutex);
            } else {
                if (update == 1) {
                    const char *update_sql = "UPDATE files SET checksum = ?, last_modified = ? WHERE full_path = ?";
                    sqlite3_stmt *update_stmt;
                    sqlite3_prepare_v2(db, update_sql, -1, &update_stmt, NULL);
                    sqlite3_bind_text(update_stmt, 1, checksum, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(update_stmt, 2, st.st_mtime);
                    sqlite3_bind_text(update_stmt, 3, path, -1, SQLITE_STATIC);
                    sqlite3_step(update_stmt);
                    sqlite3_finalize(update_stmt);
                }
		if (verbose == 1) {
                    printf("Changed: %s\n", path);
		}
                pthread_mutex_lock(&count_mutex);
                changedCount++;
                pthread_mutex_unlock(&count_mutex);
            }
        }
    } else {
        char checksum[HASH_SIZE], owner[256];
        if (update == 1) {
            compute_sha256(path, checksum);
            get_owner(st.st_uid, owner, sizeof(owner));

            const char *insert_sql =
                "INSERT INTO files (file_name, full_path, size, created, last_modified, owner, checksum) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)";
            sqlite3_stmt *insert_stmt;
            sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, NULL);
            sqlite3_bind_text(insert_stmt, 1, name, -1, SQLITE_STATIC);
            sqlite3_bind_text(insert_stmt, 2, path, -1, SQLITE_STATIC);
            sqlite3_bind_int64(insert_stmt, 3, st.st_size);
            sqlite3_bind_int64(insert_stmt, 4, st.st_ctime);
            sqlite3_bind_int64(insert_stmt, 5, st.st_mtime);
            sqlite3_bind_text(insert_stmt, 6, owner, -1, SQLITE_STATIC);
            sqlite3_bind_text(insert_stmt, 7, checksum, -1, SQLITE_STATIC);
            sqlite3_step(insert_stmt);
            sqlite3_finalize(insert_stmt);
        }
	if (verbose == 1) {
            printf("New: %s\n", path);
	}
        pthread_mutex_lock(&count_mutex);
        newCount++;
        pthread_mutex_unlock(&count_mutex);
    }
    sqlite3_finalize(stmt);
}

// ==== Directory traversal (producer) ====
void traverse_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                traverse_directory(full_path);
            } else {
                if (strstr(full_path, ".DS_Store") == NULL &&
                    strstr(full_path, ".Trashes") == NULL &&
                    strstr(full_path, ".Trash") == NULL &&
                    strstr(full_path, ".Spotlight") == NULL &&
                    strstr(full_path, ".fseventsd") == NULL &&
                    strstr(full_path, ".localized") == NULL) {
                    enqueue(full_path, entry->d_name);
                }
            }
        }
    }
    closedir(dir);
}

// ==== Worker thread function ====
// void *worker(void *arg) {
void *worker() {
    sqlite3 *db;
    if (sqlite3_open(global_db_path, &db)) {
        fprintf(stderr, "Worker can't open DB: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    while (1) {
        FileNode *node = dequeue();
        if (!node) break; // no more work
        process_file(node->path, node->name, db);
        free(node);
    }

    sqlite3_close(db);
    return NULL;
}

// ==== Main ====
int main(int argc, char *argv[]) {
    char *path = NULL, *db_name = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) path = argv[++i];
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) db_name = argv[++i];
        else if (strcmp(argv[i], "-c") == 0) verifyChecksum = 1;
        else if (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (strcmp(argv[i], "-u") == 0) update = 1;
	else if (strcmp(argv[i], "-t") == 0) number_of_threads = atoi(argv[++i]);
    }

    char input[1024];
    if (!path) {
        printf("Enter path to scan: ");
        fgets(input, sizeof(input), stdin);
        path = strtok(input, "\n");
    }
    if (!db_name) {
        printf("Enter database name: ");
        fgets(input, sizeof(input), stdin);
        db_name = strtok(input, "\n");
    }

    snprintf(global_db_path, sizeof(global_db_path),
             "%s/db/FileTracker/%s.db", getenv("HOME"), db_name);
    char *mkdir_cmd;
    asprintf(&mkdir_cmd, "mkdir -p %s/db/FileTracker", getenv("HOME"));
    system(mkdir_cmd);
    free(mkdir_cmd);

    // Ensure DB exists and has schema
    sqlite3 *db;
    if (sqlite3_open(global_db_path, &db)) {
        fprintf(stderr, "Can't open DB: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY, file_name TEXT, full_path TEXT UNIQUE, size INTEGER, "
        "created INTEGER, last_modified INTEGER, owner TEXT, checksum TEXT);";
    sqlite3_exec(db, create_sql, 0, 0, 0);
    sqlite3_close(db);

    // Launch worker threads
    pthread_t threads[number_of_threads];
    for (int i = 0; i < number_of_threads; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    // Producer: walk directories
    traverse_directory(path);

    // Signal no more files
    pthread_mutex_lock(&queue_mutex);
    done_traversal = 1;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    // Join workers
    for (int i = 0; i < number_of_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    if (verbose == 1) {
        printf("Looking for missing files\n");
    }

    // Handle missing files
    if (sqlite3_open(global_db_path, &db) == 0) {
        const char *missing_sql = "SELECT full_path FROM files";
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, missing_sql, -1, &stmt, NULL);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *db_path = (const char *)sqlite3_column_text(stmt, 0);
            if (access(db_path, F_OK) != 0) {
                pthread_mutex_lock(&count_mutex);
                missingCount++;
                pthread_mutex_unlock(&count_mutex);
                if (verbose == 1) {
                    printf("Missing: %s\n", db_path);
		}
                sqlite3_stmt *del_stmt;
                sqlite3_prepare_v2(db, "DELETE FROM files WHERE full_path = ?", -1, &del_stmt, NULL);
                sqlite3_bind_text(del_stmt, 1, db_path, -1, SQLITE_STATIC);
                sqlite3_step(del_stmt);
                sqlite3_finalize(del_stmt);
            }
        }
        sqlite3_finalize(stmt);

        // Total records
        int totalRecords = 0;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM files", -1, &stmt, NULL);
        if (sqlite3_step(stmt) == SQLITE_ROW) totalRecords = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        printf("###\n");
        printf("Unchanged: %d\nChanged: %d\nNew: %d\nMissing Files: %d\nTotal Records: %d\n",
               unchangedCount, changedCount, newCount, missingCount, totalRecords);

        sqlite3_close(db);
    }

    return 0;
}
