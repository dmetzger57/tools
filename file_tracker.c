#include <dirent.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <locale.h>

#define HASH_SIZE 65
#define MAX_PATH 4096
#define MAX_IGNORES 1024

// ==== Globals ====
int verbose = 0;
int update = 0;
int verifyChecksum = 0;

// Aggregate counters (Protected by global_count_mutex)
int total_unchanged = 0, total_changed = 0, total_new = 0, total_missing = 0,
    total_ignored = 0, total_error = 0;

char *ignore_list[MAX_IGNORES];
int ignore_count = 0;

pthread_mutex_t global_count_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char source_path[MAX_PATH];
    char db_path[MAX_PATH];
    char log_path[MAX_PATH];
    FILE *log_fp;
    int unchanged, changed, new, missing, ignored, error;
} ThreadContext;

// ==== Ignore List Helpers ====
void load_ignore_list() {
    const char *home = getenv("HOME");
    char ignore_path[MAX_PATH];
    snprintf(ignore_path, sizeof(ignore_path), "%s/.rsync-ignore", home);

    FILE *f = fopen(ignore_path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f) && ignore_count < MAX_IGNORES) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            ignore_list[ignore_count++] = strdup(line);
        }
    }
    fclose(f);
}

int is_ignored(const char *name) {
    for (int i = 0; i < ignore_count; i++) {
        if (strcmp(name, ignore_list[i]) == 0) return 1;
    }
    return 0;
}

// ==== Logging Helper ====
void log_message(ThreadContext *ctx, const char *status, const char *path) {
    if (ctx->log_fp) {
        fprintf(ctx->log_fp, "[%-18s] %s\n", status, path);
        if (verbose) {
            fprintf(stdout, "[%s][%-18s] %s\n", basename(ctx->source_path), status, path);
        }
    }
}

// ==== Utility Functions ====
void compute_sha256(const char *path, char *outputBuffer) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        strcpy(outputBuffer, "");
        return;
    }
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    const int bufSize = 32768;
    unsigned char *buffer = malloc(bufSize);
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

void get_owner(uid_t uid, char *owner, size_t size) {
    struct passwd *pw = getpwuid(uid);
    if (pw) snprintf(owner, size, "%s", pw->pw_name);
    else snprintf(owner, size, "%d", uid);
}

// ==== Core Logic ====
void process_file(ThreadContext *ctx, const char *path, const char *name, sqlite3 *db) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return;

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT last_modified, checksum FROM files WHERE full_path = ? LIMIT 1", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        time_t db_mtime = sqlite3_column_int64(stmt, 0);
        const char *db_checksum = (const char *)sqlite3_column_text(stmt, 1);
        int mtime_match = (db_mtime == st.st_mtime);

        if (!verifyChecksum && mtime_match) {
            log_message(ctx, "UNCHANGED", path);
            ctx->unchanged++;
        } else {
            char checksum[HASH_SIZE];
            compute_sha256(path, checksum);
            int checksum_match = (db_checksum && strcmp(checksum, db_checksum) == 0);

            if (verifyChecksum && checksum_match) {
                log_message(ctx, "UNCHANGED", path);
                ctx->unchanged++;
            } else {
                log_message(ctx, (!mtime_match) ? "CHANGED (Metadata)" : "CHANGED (Checksum)", path);
                if (update) {
                    sqlite3_stmt *up_stmt;
                    sqlite3_prepare_v2(db, "UPDATE files SET checksum = ?, last_modified = ? WHERE full_path = ?", -1, &up_stmt, NULL);
                    sqlite3_bind_text(up_stmt, 1, checksum, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(up_stmt, 2, st.st_mtime);
                    sqlite3_bind_text(up_stmt, 3, path, -1, SQLITE_STATIC);
                    sqlite3_step(up_stmt);
                    sqlite3_finalize(up_stmt);
                }
                ctx->changed++;
            }
        }
    } else {
        log_message(ctx, "NEW", path);
        if (update) {
            char checksum[HASH_SIZE], owner[256];
            compute_sha256(path, checksum);
            get_owner(st.st_uid, owner, sizeof(owner));
            sqlite3_stmt *ins_stmt;
            sqlite3_prepare_v2(db, "INSERT INTO files (file_name, full_path, size, created, last_modified, owner, checksum) VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &ins_stmt, NULL);
            sqlite3_bind_text(ins_stmt, 1, name, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins_stmt, 2, path, -1, SQLITE_STATIC);
            sqlite3_bind_int64(ins_stmt, 3, st.st_size);
            sqlite3_bind_int64(ins_stmt, 4, st.st_ctime);
            sqlite3_bind_int64(ins_stmt, 5, st.st_mtime);
            sqlite3_bind_text(ins_stmt, 6, owner, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins_stmt, 7, checksum, -1, SQLITE_STATIC);
            sqlite3_step(ins_stmt);
            sqlite3_finalize(ins_stmt);
        }
        ctx->new++;
    }
    sqlite3_finalize(stmt);
}

void traverse_directory(ThreadContext *ctx, const char *dir_path, sqlite3 *db) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (is_ignored(entry->d_name)) {
            ctx->ignored++;
            continue;
        }
        char full_path[MAX_PATH];
        struct stat st;
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) traverse_directory(ctx, full_path, db);
            else if (strstr(full_path, ".DS_Store") == NULL) process_file(ctx, full_path, entry->d_name, db);
        }
    }
    closedir(dir);
}

void *path_worker(void *arg) {
    ThreadContext *ctx = (ThreadContext *)arg;
    sqlite3 *db;

    if (sqlite3_open(ctx->db_path, &db) != SQLITE_OK) return NULL;
    
    // Enable WAL mode for better concurrency
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", 0, 0, 0);
    sqlite3_busy_timeout(db, 30000);  // Increased timeout for concurrent access

    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY, file_name TEXT, full_path TEXT UNIQUE, size INTEGER, created INTEGER, last_modified INTEGER, owner TEXT, checksum TEXT, keywords TEXT);", 0, 0, 0);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS meta (id INTEGER PRIMARY KEY AUTOINCREMENT, last_checksum_verify_date TEXT, last_date_verify TEXT, verify_machine TEXT, num_unchanged INTEGER, num_changed INTEGER, num_new INTEGER, num_missing INTEGER, num_errors INTEGER);", 0, 0, 0);

    // Begin transaction for better performance and reduced lock contention
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, 0);

    traverse_directory(ctx, ctx->source_path, db);

    sqlite3_stmt *mStmt;
    sqlite3_prepare_v2(db, "SELECT full_path FROM files", -1, &mStmt, NULL);
    while (sqlite3_step(mStmt) == SQLITE_ROW) {
        const char *dp = (const char *)sqlite3_column_text(mStmt, 0);
        if (access(dp, F_OK) != 0) {
            ctx->missing++;
            log_message(ctx, "MISSING", dp);
            sqlite3_stmt *dStmt;
            sqlite3_prepare_v2(db, "DELETE FROM files WHERE full_path = ?", -1, &dStmt, NULL);
            sqlite3_bind_text(dStmt, 1, dp, -1, SQLITE_STATIC);
            sqlite3_step(dStmt);
            sqlite3_finalize(dStmt);
        }
    }
    sqlite3_finalize(mStmt);

    char hname[256];
    gethostname(hname, 256);
    char *sql;
    asprintf(&sql, "INSERT INTO meta (%s, verify_machine, num_unchanged, num_changed, num_new, num_missing, num_errors) VALUES (datetime('now','localtime'), ?, ?, ?, ?, ?, ?)",
             verifyChecksum ? "last_checksum_verify_date" : "last_date_verify");
    sqlite3_stmt *insMeta;
    sqlite3_prepare_v2(db, sql, -1, &insMeta, NULL);
    sqlite3_bind_text(insMeta, 1, hname, -1, SQLITE_STATIC);
    sqlite3_bind_int(insMeta, 2, ctx->unchanged);
    sqlite3_bind_int(insMeta, 3, ctx->changed);
    sqlite3_bind_int(insMeta, 4, ctx->new);
    sqlite3_bind_int(insMeta, 5, ctx->missing);
    sqlite3_bind_int(insMeta, 6, ctx->error);
    sqlite3_step(insMeta);
    sqlite3_finalize(insMeta);
    free(sql);

    // Commit transaction
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);

    sqlite3_close(db);
    // Note: log_fp is now closed in main() to allow appending the summary

    pthread_mutex_lock(&global_count_mutex);
    total_unchanged += ctx->unchanged;
    total_changed += ctx->changed;
    total_new += ctx->new;
    total_missing += ctx->missing;
    total_ignored += ctx->ignored;
    total_error += ctx->error;
    pthread_mutex_unlock(&global_count_mutex);

    return NULL;
}

int main(int argc, char *argv[]) {

    char *path_arg = NULL;

    int help_requested = 0;

    setlocale(LC_NUMERIC, "");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) path_arg = argv[++i];
        else if (strcmp(argv[i], "-c") == 0) verifyChecksum = 1;
        else if (strcmp(argv[i], "-u") == 0) update = 1;
        else if (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (strcmp(argv[i], "-h") == 0) help_requested = 1;
    }

    if( help_requested == 1 ) {
        fprintf(stderr, "Usage: %s -p /path1,/path2 [-c] [-u] [-v]\n", argv[0]);
	exit( 0 );
    }

    if (!path_arg) {
	fprintf(stderr, "Error: -p option is required\n");
        fprintf(stderr, "Usage: %s -p /path1,/path2 [-c] [-u] [-v]\n", argv[0]);
        exit(1);
    }

    load_ignore_list();
    const char *home = getenv("HOME");

    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/logs/FileTracker %s/db/FileTracker", home, home);
    system(cmd);

    char *token = strtok(path_arg, ",");
    ThreadContext contexts[64]; 
    pthread_t threads[64];
    int thread_count = 0;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", t);

    while (token && thread_count < 64) {
        strncpy(contexts[thread_count].source_path, token, MAX_PATH);
        char *path_copy = strdup(token);
        char *base = basename(path_copy);

        snprintf(contexts[thread_count].db_path, MAX_PATH, "%s/db/FileTracker/%s.db", home, base);

        snprintf(contexts[thread_count].log_path, MAX_PATH, "%s/logs/FileTracker/%s-%s.log", home, base, timestamp);

        contexts[thread_count].log_fp = fopen(contexts[thread_count].log_path, "w");
        contexts[thread_count].unchanged = contexts[thread_count].changed = 0;
        contexts[thread_count].new = contexts[thread_count].missing = 0;
        contexts[thread_count].ignored = contexts[thread_count].error = 0;

        pthread_create(&threads[thread_count], NULL, path_worker, &contexts[thread_count]);

        free(path_copy);
        thread_count++;
        token = strtok(NULL, ",");
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // Output and Log Summary
    const char *summary_header = "\n================ AGGREGATE SUMMARY ================\n";
    const char *summary_footer = "==================================================\n";

    printf("%s", summary_header);
    printf("Unchanged      : %'d\n", total_unchanged);
    printf("Changed        : %'d\n", total_changed);
    printf("New            : %'d\n", total_new);
    printf("Missing        : %'d\n", total_missing);
    printf("Ignored        : %'d\n", total_ignored);
    printf("Errors         : %'d\n", total_error);
    printf("%s", summary_footer);

    for (int i = 0; i < thread_count; i++) {
        if (contexts[i].log_fp) {
            fprintf(contexts[i].log_fp, "%s", summary_header);
            fprintf(contexts[i].log_fp, "Unchanged      : %d\n", total_unchanged);
            fprintf(contexts[i].log_fp, "Changed        : %d\n", total_changed);
            fprintf(contexts[i].log_fp, "New            : %d\n", total_new);
            fprintf(contexts[i].log_fp, "Missing        : %d\n", total_missing);
            fprintf(contexts[i].log_fp, "Ignored        : %d\n", total_ignored);
            fprintf(contexts[i].log_fp, "Errors         : %d\n", total_error);
            fprintf(contexts[i].log_fp, "%s", summary_footer);
            fclose(contexts[i].log_fp);
        }
    }

    for (int i = 0; i < ignore_count; i++) free(ignore_list[i]);
    return 0;
}
