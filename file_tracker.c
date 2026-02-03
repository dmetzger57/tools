#include <dirent.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <pwd.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#define HASH_SIZE 65
#define MAX_PATH 4096

// ==== Globals ====
int verbose = 0;
int number_of_threads = 4;
int update = 0;
int unchangedCount = 0, changedCount = 0, newCount = 0, missingCount = 0,
    ignoredCount = 0, errorCount = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct FileNode {
  char path[MAX_PATH];
  char name[256];
  struct FileNode *next;
} FileNode;

FileNode *queue_head = NULL, *queue_tail = NULL;
int done_traversal = 0;
char global_db_path[1024];
int verifyChecksum = 0;
FILE *log_fp = NULL;

// ==== Logging Helper ====
void log_message(const char *status, const char *path) {
    if (log_fp) {
        pthread_mutex_lock(&log_mutex);
        fprintf(log_fp, "[%-18s] %s\n", status, path);
        fflush(log_fp);
        pthread_mutex_unlock(&log_mutex);
	if( verbose ) {
            fprintf(stdout, "[%-18s] %s\n", status, path);
	}
    }
}

// ==== Queue functions ====
void enqueue(const char *path, const char *name) {
  FileNode *node = malloc(sizeof(FileNode));
  strncpy(node->path, path, MAX_PATH);
  strncpy(node->name, name, 256);
  node->next = NULL;
  pthread_mutex_lock(&queue_mutex);
  if (!queue_tail) { queue_head = queue_tail = node; }
  else { queue_tail->next = node; queue_tail = node; }
  pthread_cond_signal(&queue_cond);
  pthread_mutex_unlock(&queue_mutex);
}

FileNode *dequeue() {
  pthread_mutex_lock(&queue_mutex);
  while (!queue_head && !done_traversal) { pthread_cond_wait(&queue_cond, &queue_mutex); }
  if (!queue_head) { pthread_mutex_unlock(&queue_mutex); return NULL; }
  FileNode *node = queue_head;
  queue_head = queue_head->next;
  if (!queue_head) queue_tail = NULL;
  pthread_mutex_unlock(&queue_mutex);
  return node;
}

void get_owner(uid_t uid, char *owner, size_t size) {
  struct passwd *pw = getpwuid(uid);
  if (pw) snprintf(owner, size, "%s", pw->pw_name);
  else snprintf(owner, size, "%d", uid);
}

void compute_sha256(const char *path, char *outputBuffer) {
  FILE *file = fopen(path, "rb");
  if (!file) { strcpy(outputBuffer, ""); return; }
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
  const int bufSize = 32768;
  unsigned char *buffer = malloc(bufSize);
  int bytesRead;
  while ((bytesRead = fread(buffer, 1, bufSize, file))) { EVP_DigestUpdate(mdctx, buffer, bytesRead); }
  EVP_DigestFinal_ex(mdctx, hash, &hash_len);
  for (unsigned int i = 0; i < hash_len; i++) { sprintf(outputBuffer + (i * 2), "%02x", hash[i]); }
  outputBuffer[hash_len * 2] = '\0';
  EVP_MD_CTX_free(mdctx);
  fclose(file);
  free(buffer);
}

void process_file(const char *path, const char *name, sqlite3 *db) {
  struct stat st;
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return;
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT last_modified, checksum FROM files WHERE full_path = ? LIMIT 1", -1, &stmt, NULL);
  sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    time_t db_mtime = sqlite3_column_int64(stmt, 0);
    const char *db_checksum = (const char *)sqlite3_column_text(stmt, 1);

    int mtime_match = (db_mtime == st.st_mtime);

    if (!verifyChecksum && mtime_match) {
      log_message("UNCHANGED", path);
      pthread_mutex_lock(&count_mutex); unchangedCount++; pthread_mutex_unlock(&count_mutex);
    } else {
      char checksum[HASH_SIZE];
      compute_sha256(path, checksum);
      int checksum_match = (strcmp(checksum, db_checksum) == 0);

      if (verifyChecksum && checksum_match) {
        log_message("UNCHANGED", path);
        pthread_mutex_lock(&count_mutex); unchangedCount++; pthread_mutex_unlock(&count_mutex);
      } else {
        const char *reason = (!mtime_match) ? "CHANGED (Metadata)" : "CHANGED (Checksum)";
        log_message(reason, path);

        if (update) {
          sqlite3_stmt *up_stmt;
          sqlite3_prepare_v2(db, "UPDATE files SET checksum = ?, last_modified = ? WHERE full_path = ?", -1, &up_stmt, NULL);
          sqlite3_bind_text(up_stmt, 1, checksum, -1, SQLITE_STATIC);
          sqlite3_bind_int64(up_stmt, 2, st.st_mtime);
          sqlite3_bind_text(up_stmt, 3, path, -1, SQLITE_STATIC);
          sqlite3_step(up_stmt); sqlite3_finalize(up_stmt);
        }
        pthread_mutex_lock(&count_mutex); changedCount++; pthread_mutex_unlock(&count_mutex);
      }
    }
  } else {
    log_message("NEW", path);
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
      sqlite3_step(ins_stmt); sqlite3_finalize(ins_stmt);
    }
    pthread_mutex_lock(&count_mutex); newCount++; pthread_mutex_unlock(&count_mutex);
  }
  sqlite3_finalize(stmt);
}

void traverse_directory(const char *dir_path) {
  DIR *dir = opendir(dir_path);
  if (!dir) return;
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    char full_path[MAX_PATH];
    struct stat st;
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) traverse_directory(full_path);
      else if (strstr(full_path, ".DS_Store") == NULL) enqueue(full_path, entry->d_name);
    }
  }
  closedir(dir);
}

void *worker() {
  sqlite3 *db;
  sqlite3_open(global_db_path, &db);
  sqlite3_busy_timeout(db, 10000);
  while (1) {
    FileNode *node = dequeue();
    if (!node) break;
    process_file(node->path, node->name, db);
    free(node);
  }
  sqlite3_close(db);
  return NULL;
}

int main(int argc, char *argv[]) {
  char *path = NULL, *db_name = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-p") == 0) path = argv[++i];
    else if (strcmp(argv[i], "-d") == 0) db_name = argv[++i];
    else if (strcmp(argv[i], "-c") == 0) verifyChecksum = 1;
    else if (strcmp(argv[i], "-u") == 0) update = 1;
    else if (strcmp(argv[i], "-v") == 0) verbose = 1;
    else if (strcmp(argv[i], "-h") == 0) fprintf( stderr, "Usage: [-c] [-u] [-v] [-l] [-t ThreadCount] -d DbName -p Path\n");
    else if (strcmp(argv[i], "-t") == 0) number_of_threads = atoi(argv[++i]);
  }
  if (!path || !db_name) exit(1);

  const char *home = getenv("HOME");

  // Create $HOME/logs/file_tracker
  char log_dir[MAX_PATH], log_path[MAX_PATH], mkdir_cmd[MAX_PATH];
  snprintf(log_dir, sizeof(log_dir), "%s/logs/file_tracker", home);
  snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", log_dir);
  system(mkdir_cmd);

  // File Name: dbName-YYYY-MM-DD-HH-MM-SS.log
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", t);
  snprintf(log_path, sizeof(log_path), "%s/%s-%s.log", log_dir, db_name, timestamp);
  log_fp = fopen(log_path, "w");

  snprintf(global_db_path, sizeof(global_db_path), "%s/db/FileTracker/%s.db", home, db_name);

  sqlite3 *db;
  sqlite3_open(global_db_path, &db);
  sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY, file_name TEXT, full_path TEXT UNIQUE, size INTEGER, created INTEGER, last_modified INTEGER, owner TEXT, checksum TEXT, keywords TEXT);", 0, 0, 0);
  sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS meta (id INTEGER PRIMARY KEY AUTOINCREMENT, last_checksum_verify_date TEXT, last_date_verify TEXT, verify_machine TEXT, num_unchanged INTEGER, num_changed INTEGER, num_new INTEGER, num_missing INTEGER, num_errors INTEGER);", 0, 0, 0);

  int pU = 0, pC = 0, pN = 0, pM = 0, pE = 0;
  char pDate[64] = "None";
  sqlite3_stmt *pStmt;
  sqlite3_prepare_v2(db, "SELECT last_checksum_verify_date, last_date_verify, num_unchanged, num_changed, num_new, num_missing, num_errors FROM meta ORDER BY id DESC LIMIT 1", -1, &pStmt, NULL);
  if (sqlite3_step(pStmt) == SQLITE_ROW) {
    const char *cDate = (const char*)sqlite3_column_text(pStmt, 0);
    const char *vDate = (const char*)sqlite3_column_text(pStmt, 1);
    strncpy(pDate, cDate ? cDate : (vDate ? vDate : "N/A"), 64);
    pU = sqlite3_column_int(pStmt, 2); pC = sqlite3_column_int(pStmt, 3);
    pN = sqlite3_column_int(pStmt, 4); pM = sqlite3_column_int(pStmt, 5); pE = sqlite3_column_int(pStmt, 6);
  }
  sqlite3_finalize(pStmt);
  sqlite3_close(db);

  pthread_t threads[number_of_threads];
  for (int i = 0; i < number_of_threads; i++) pthread_create(&threads[i], NULL, worker, NULL);
  traverse_directory(path);
  pthread_mutex_lock(&queue_mutex); done_traversal = 1; pthread_cond_broadcast(&queue_cond); pthread_mutex_unlock(&queue_mutex);
  for (int i = 0; i < number_of_threads; i++) pthread_join(threads[i], NULL);

  sqlite3_open(global_db_path, &db);
  sqlite3_stmt *mStmt;
  sqlite3_prepare_v2(db, "SELECT full_path FROM files", -1, &mStmt, NULL);
  while (sqlite3_step(mStmt) == SQLITE_ROW) {
    const char *dp = (const char *)sqlite3_column_text(mStmt, 0);
    if (access(dp, F_OK) != 0) {
      missingCount++;
      log_message("MISSING", dp);
      sqlite3_stmt *dStmt;
      sqlite3_prepare_v2(db, "DELETE FROM files WHERE full_path = ?", -1, &dStmt, NULL);
      sqlite3_bind_text(dStmt, 1, dp, -1, SQLITE_STATIC);
      sqlite3_step(dStmt); sqlite3_finalize(dStmt);
    }
  }
  sqlite3_finalize(mStmt);

  char hname[256]; gethostname(hname, 256);
  const char *col = verifyChecksum ? "last_checksum_verify_date" : "last_date_verify";
  char *sql; asprintf(&sql, "INSERT INTO meta (%s, verify_machine, num_unchanged, num_changed, num_new, num_missing, num_errors) VALUES (datetime('now','localtime'), ?, ?, ?, ?, ?, ?)", col);
  sqlite3_stmt *insMeta;
  sqlite3_prepare_v2(db, sql, -1, &insMeta, NULL);
  sqlite3_bind_text(insMeta, 1, hname, -1, SQLITE_STATIC);
  sqlite3_bind_int(insMeta, 2, unchangedCount); sqlite3_bind_int(insMeta, 3, changedCount);
  sqlite3_bind_int(insMeta, 4, newCount); sqlite3_bind_int(insMeta, 5, missingCount);
  sqlite3_bind_int(insMeta, 6, errorCount);
  sqlite3_step(insMeta); sqlite3_finalize(insMeta); free(sql);
  sqlite3_close(db);

  if (log_fp) fclose(log_fp);

  printf("\n================ RUN SUMMARY ================\n");
  printf("Statistic      | Previous (%-10s) | Current\n", pDate);
  printf("---------------|----------------------|----------\n");
  printf("Unchanged      | %-20d | %d\n", pU, unchangedCount);
  printf("Changed        | %-20d | %d\n", pC, changedCount);
  printf("New            | %-20d | %d\n", pN, newCount);
  printf("Missing        | %-20d | %d\n", pM, missingCount);
  printf("Errors         | %-20d | %d\n", pE, errorCount);
  printf("=============================================\n");
  printf("Detailed log created: %s\n", log_path);

  return 0;
}
