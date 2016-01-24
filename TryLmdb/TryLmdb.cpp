// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "lmdb.h"
#include "histogram.h"
#include "random.h"
#include "testutil.h"
#include <time.h>

#include <time.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <dirent.h> 
#endif

#ifdef WIN32
int
gettimeofday(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;

	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tp->tv_sec = clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;

	return (0);
}


typedef struct _dirdesc {
	int     dd_fd;      /** file descriptor associated with directory */
	long    dd_loc;     /** offset in current buffer */
	long    dd_size;    /** amount of data returned by getdirentries */
	char    *dd_buf;    /** data buffer */
	int     dd_len;     /** size of data buffer */
	long    dd_seek;    /** magic cookie returned by getdirentries */
} DIR;

# define __dirfd(dp)    ((dp)->dd_fd)

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void rewinddir(DIR *);
int closedir(DIR *);

#include <sys/types.h>

struct dirent
{
	long d_ino;              /* inode number*/
	off_t d_off;             /* offset to this dirent*/
	unsigned short d_reclen; /* length of this d_name*/
	unsigned char d_type;    /* the type of d_name*/
	char d_name[1];          /* file name (null-terminated)*/
};


static HANDLE hFind;

DIR *opendir(const char *name)
{
	DIR *dir;
	WIN32_FIND_DATA FindData;
	char namebuf[512];

	sprintf(namebuf, "%s\\*.*", name);

	hFind = FindFirstFile(namebuf, &FindData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("FindFirstFile failed (%d)\n", GetLastError());
		return 0;
	}

	dir = (DIR *)malloc(sizeof(DIR));
	if (!dir)
	{
		printf("DIR memory allocate fail\n");
		return 0;
	}

	memset(dir, 0, sizeof(DIR));
	dir->dd_fd = 0;   // simulate return  

	return dir;
}

struct dirent *readdir(DIR *d)
{
	int i;
	static struct dirent dirent;
	BOOL bf;
	WIN32_FIND_DATA FileData;
	if (!d)
	{
		return 0;
	}

	bf = FindNextFile(hFind, &FileData);
	//fail or end  
	if (!bf)
	{
		return 0;
	}

	for (i = 0; i < 256; i++)
	{
		dirent.d_name[i] = FileData.cFileName[i];
		if (FileData.cFileName[i] == '\0') break;
	}
	dirent.d_reclen = i;
	dirent.d_reclen = FileData.nFileSizeLow;

	//check there is file or directory  
	if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		dirent.d_type = 2;
	}
	else
	{
		dirent.d_type = 1;
	}


	return (&dirent);
}

int closedir(DIR *d)
{
	if (!d) return -1;
	hFind = 0;
	free(d);
	return 0;
}
#endif
uint64_t NowMicros() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}
void GetTestDirectory(std::string* result)
{
	*result = "./testdb";
}
int DeleteFile(char * filename)
{
	return remove(filename);
}

void GetChildren(const std::string& dir,
	std::vector<std::string>* result) {
	result->clear();
	DIR* d = opendir(dir.c_str());
	if (d == NULL) {
		return;
	}
	struct dirent* entry;
	while ((entry = readdir(d)) != NULL) {
		result->push_back(entry->d_name);
	}
	closedir(d);
}
// Comma-separated list of operations to run in the specified order
//   Actual benchmarks:
//
//   fillseq       -- write N values in sequential key order in async mode
//   fillrandom    -- write N values in random key order in async mode
//   fillrandint   -- write N values in random binary key order in async mode
//   overwrite     -- overwrite N values in random key order in async mode
//   fillseqsync   -- write N/100 values in sequential key order in sync mode
//   fillseqbatch  -- batch write N values in sequential key order in async mode
//   fillrandsync  -- write N/100 values in random key order in sync mode
//   fillrandbatch  -- batch write N values in random key order in async mode
//   fillrandibatch  -- batch write N values in random binary key order in async mode
//   fillrand100K  -- write N/1000 100K values in random order in async mode
//   fillseq100K   -- write N/1000 100K values in seq order in async mode
//   readseq       -- read N times sequentially
//   readreverse   -- read N times in reverse order
//   readseq100K   -- read N/1000 100K values in sequential order in async mode
//   readrand100K  -- read N/1000 100K values in sequential order in async mode
//   readrandom    -- read N times in random order
static const char* FLAGS_benchmarks =
"fillseqsync,"
"fillrandsync,"
"fillrandbatchsync,"
"fillseq,"
"fillseqbatch,"
"fillrandom,"
"fillrandbatch,"
"overwrite,"
#if 0
"overwritebatch,"
#endif
"readrandom,"
"readseq,"
"readreverse,"
#if 0
"fillrand100K,"
"fillseq100K,"
"readseq100K,"
"readrand100K,"
#endif
;

// Number of key/values to place in database
static int FLAGS_num = 200000;

// Number of read operations to do.  If negative, do FLAGS_num reads.
static int FLAGS_reads = -1;

// Size of each value
static int FLAGS_value_size = 100;

// Number of key/values to place in database
static int FLAGS_batch = 100;

// Arrange to generate values that shrink to this fraction of
// their original size after compression
static double FLAGS_compression_ratio = 0.5;

// Print histogram of operation timings
static bool FLAGS_histogram = false;

// Page size. Default 1 KB
static int FLAGS_page_size = 1024;

// If true, do not destroy the existing database.  If you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool FLAGS_use_existing_db = false;

// If true, we allow batch writes to occur
static bool FLAGS_transaction = true;

// If false, skip sync of meta pages on synchronous writes
static bool FLAGS_metasync = true;

// If true, use writable mmap
static bool FLAGS_writemap = true;

// Use the db with the following name.
static const char* FLAGS_db = NULL;

static int *shuff = NULL;

namespace leveldb {

	// Helper for quickly generating random data.
	namespace {
		class RandomGenerator {
		private:
			std::string data_;
			int pos_;

		public:
			RandomGenerator() {
				// We use a limited amount of data over and over again and ensure
				// that it is larger than the compression window (32KB), and also
				// large enough to serve all typical value sizes we want to write.
				Random rnd(301);
				std::string piece;
				while (data_.size() < 1048576) {
					// Add a short fragment that is as compressible as specified
					// by FLAGS_compression_ratio.
					test::CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
					data_.append(piece);
				}
				pos_ = 0;
			}

			Slice Generate(int len) {
				if (pos_ + len > data_.size()) {
					pos_ = 0;
					assert(len < data_.size());
				}
				pos_ += len;
				return Slice(data_.data() + pos_ - len, len);
			}
		};

		static Slice TrimSpace(Slice s) {
			int start = 0;
			while (start < s.size() && isspace(s[start])) {
				start++;
			}
			int limit = s.size();
			while (limit > start && isspace(s[limit - 1])) {
				limit--;
			}
			return Slice(s.data() + start, limit - start);
		}

	}  // namespace

	class Benchmark {
	private:
		MDB_env *db_;
		MDB_dbi dbi_;
		int db_num_;
		int num_;
		int reads_;
		double start_;
		double last_op_finish_;
		int64_t bytes_;
		std::string message_;
		Histogram hist_;
		RandomGenerator gen_;
		Random rand_;

		// State kept for progress messages
		int done_;
		int next_report_;     // When to report next

		void PrintHeader() {
			const int kKeySize = 16;
			PrintEnvironment();
			fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
			fprintf(stdout, "Values:     %d bytes each (%d bytes after compression)\n",
				FLAGS_value_size,
				static_cast<int>(FLAGS_value_size * FLAGS_compression_ratio + 0.5));
			fprintf(stdout, "Entries:    %d\n", num_);
			fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
				((static_cast<int64_t>(kKeySize + FLAGS_value_size) * num_)
				/ 1048576.0));
			fprintf(stdout, "FileSize:   %.1f MB (estimated)\n",
				(((kKeySize + FLAGS_value_size * FLAGS_compression_ratio) * num_)
				/ 1048576.0));
			PrintWarnings();
			fprintf(stdout, "------------------------------------------------\n");
		}

		void PrintWarnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
			fprintf(stdout,
				"WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
				);
#endif
#ifndef NDEBUG
			fprintf(stdout,
				"WARNING: Assertions are enabled; benchmarks unnecessarily slow\n");
#endif
		}

		void PrintEnvironment() {
			fprintf(stderr, "MDB:    version %s\n", mdb_version(NULL, NULL, NULL));

#if defined(__linux)
			time_t now = time(NULL);
			fprintf(stderr, "Date:           %s", ctime(&now));  // ctime() adds newline

			FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
			if (cpuinfo != NULL) {
				char line[1000];
				int num_cpus = 0;
				std::string cpu_type;
				std::string cache_size;
				while (fgets(line, sizeof(line), cpuinfo) != NULL) {
					const char* sep = strchr(line, ':');
					if (sep == NULL) {
						continue;
					}
					Slice key = TrimSpace(Slice(line, sep - 1 - line));
					Slice val = TrimSpace(Slice(sep + 1));
					if (key == "model name") {
						++num_cpus;
						cpu_type = val.ToString();
					}
					else if (key == "cache size") {
						cache_size = val.ToString();
					}
				}
				fclose(cpuinfo);
				fprintf(stderr, "CPU:            %d * %s\n", num_cpus, cpu_type.c_str());
				fprintf(stderr, "CPUCache:       %s\n", cache_size.c_str());
			}
#endif
		}

		void Start() {
			start_ = NowMicros() * 1e-6;
			bytes_ = 0;
			message_.clear();
			last_op_finish_ = start_;
			hist_.Clear();
			done_ = 0;
			next_report_ = 100;
		}

		void FinishedSingleOp() {
			if (FLAGS_histogram) {
				double now = NowMicros() * 1e-6;
				double micros = (now - last_op_finish_) * 1e6;
				hist_.Add(micros);
				if (micros > 20000) {
					fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
					fflush(stderr);
				}
				last_op_finish_ = now;
			}

			done_++;
			if (done_ >= next_report_) {
				if (next_report_ < 1000)   next_report_ += 100;
				else if (next_report_ < 5000)   next_report_ += 500;
				else if (next_report_ < 10000)  next_report_ += 1000;
				else if (next_report_ < 50000)  next_report_ += 5000;
				else if (next_report_ < 100000) next_report_ += 10000;
				else if (next_report_ < 500000) next_report_ += 50000;
				else                            next_report_ += 100000;
				fprintf(stderr, "... finished %d ops%30s\r", done_, "");
				fflush(stderr);
			}
		}

		void Stop(const Slice& name) {
			double finish = NowMicros() * 1e-6;

			// Pretend at least one op was done in case we are running a benchmark
			// that does not call FinishedSingleOp().
			if (done_ < 1) done_ = 1;

			if (bytes_ > 0) {
				char rate[100];
				snprintf(rate, sizeof(rate), "%6.1f MB/s",
					(bytes_ / 1048576.0) / (finish - start_));
				if (!message_.empty()) {
					message_ = std::string(rate) + " " + message_;
				}
				else {
					message_ = rate;
				}
			}

			fprintf(stdout, "%-12s : %11.3f micros/op; %11.3f op/s; %s%s\n",
				name.ToString().c_str(),
				(finish - start_) * 1e6 / done_,
				done_ / (finish- start_),
				(message_.empty() ? "" : " "),
				message_.c_str());
			if (FLAGS_histogram) {
				fprintf(stdout, "Microseconds per op:\n%s\n", hist_.ToString().c_str());
			}
			fflush(stdout);
		}

	public:
		enum Order {
			SEQUENTIAL,
			RANDOM
		};
		enum DBState {
			FRESH,
			EXISTING
		};
		enum DBFlags {
			NONE = 0,
			SYNC,
			INT
		};

		Benchmark()
			: db_(NULL),
			db_num_(0),
			num_(FLAGS_num),
			reads_(FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads),
			bytes_(0),
			rand_(301) {
			std::vector<std::string> files;
			std::string test_dir;
			GetTestDirectory(&test_dir);
			GetChildren(test_dir.c_str(), &files);
			if (!FLAGS_use_existing_db) {
				for (int i = 0; i < files.size(); i++) {
					if (Slice(files[i]).starts_with("dbbench_mdb")) {
						std::string file_name(test_dir);
						file_name += "/";
						file_name += files[i];
						DeleteFile(file_name.c_str());
					}
				}
			}
		}

		~Benchmark() {
			mdb_env_close(db_);
		}

		void Run() {
			PrintHeader();

			const char* benchmarks = FLAGS_benchmarks;
			while (benchmarks != NULL) {
				const char* sep = strchr(benchmarks, ',');
				Slice name;
				if (sep == NULL) {
					name = benchmarks;
					benchmarks = NULL;
				}
				else {
					name = Slice(benchmarks, sep - benchmarks);
					benchmarks = sep + 1;
				}

				num_ = FLAGS_num;
				Start();

				bool known = true, writer = false;
				DBFlags flags = NONE;
				if (name == Slice("fillseq")) {
					writer = true;
					Write(flags, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1);
				}
				else if (name == Slice("fillseqbatch")) {
					writer = true;
					Write(flags, SEQUENTIAL, FRESH, num_, FLAGS_value_size, FLAGS_batch);
				}
				else if (name == Slice("fillrandom")) {
					writer = true;
					Write(flags, RANDOM, FRESH, num_, FLAGS_value_size, 1);
				}
				else if (name == Slice("fillrandbatch")) {
					writer = true;
					Write(flags, RANDOM, FRESH, num_, FLAGS_value_size, FLAGS_batch);
				}
				else if (name == Slice("fillrandint")) {
					writer = true;
					flags = INT;
					Write(flags, RANDOM, FRESH, num_, FLAGS_value_size, 1);
				}
				else if (name == Slice("fillrandibatch")) {
					writer = true;
					flags = INT;
					Write(flags, RANDOM, FRESH, num_, FLAGS_value_size, FLAGS_batch);
				}
				else if (name == Slice("overwrite")) {
					writer = true;
					Write(flags, RANDOM, EXISTING, num_, FLAGS_value_size, 1);
				}
				else if (name == Slice("overwritebatch")) {
					writer = true;
					Write(flags, RANDOM, EXISTING, num_, FLAGS_value_size, FLAGS_batch);
				}
				else if (name == Slice("fillrandsync")) {
					writer = true;
					flags = SYNC;
#if 1
					num_ /= 1000;
					if (num_<10) num_ = 10;
#endif
					Write(flags, RANDOM, FRESH, num_, FLAGS_value_size, 1);
				}
				else if (name == Slice("fillseqsync")) {
					writer = true;
					flags = SYNC;
#if 1
					num_ /= 1000;
					if (num_<10) num_ = 10;
#endif
					Write(flags, SEQUENTIAL, FRESH, num_, FLAGS_value_size, 1);
				} else
				if (name == Slice("fillrandbatchsync")) {
					writer = true;
					flags = SYNC;
#if 1
					num_ /= 100;
					if (num_<10) num_ = 10;
#endif
					Write(flags, RANDOM, FRESH, num_, FLAGS_value_size, FLAGS_batch);
				}
				else if (name == Slice("fillrand100K")) {
					writer = true;
					Write(flags, RANDOM, FRESH, num_ / 1000, 100 * 1000, 1);
				}
				else if (name == Slice("fillseq100K")) {
					writer = true;
					Write(flags, SEQUENTIAL, FRESH, num_ / 1000, 100 * 1000, 1);
				}
				else if (name == Slice("readseq")) {
					ReadSequential();
				}
				else if (name == Slice("readreverse")) {
					ReadReverse();
				}
				else if (name == Slice("readrandom")) {
					ReadRandom();
				}
				else if (name == Slice("readrand100K")) {
					int n = reads_;
					reads_ /= 1000;
					ReadRandom();
					reads_ = n;
				}
				else if (name == Slice("readseq100K")) {
					int n = reads_;
					reads_ /= 1000;
					ReadSequential();
					reads_ = n;
				}
				else {
					known = false;
					if (name != Slice()) {  // No error message for empty name
						fprintf(stderr, "unknown benchmark '%s'\n", name.ToString().c_str());
					}
				}
				if (known) {
					Stop(name);
					if (writer) {
						/*char cmd[200];
						std::string test_dir;
						GetTestDirectory(&test_dir);
						sprintf(cmd, "du %s", test_dir.c_str());
						system(cmd);*/
					}
				}
			}
		}

	private:
		void Open(DBFlags flags) {
			assert(db_ == NULL);
			int rc;
			MDB_txn *txn;

			char file_name[100], cmd[200];
			db_num_++;
			std::string test_dir;
			GetTestDirectory(&test_dir);
			snprintf(file_name, sizeof(file_name),
				"%s/dbbench_mdb-%d",
				test_dir.c_str(),
				db_num_);

			/*sprintf(cmd, "mkdir -p %s", file_name);
			system(cmd);*/

			int env_opt = 0;
			if (flags != SYNC)
				env_opt = MDB_NOSYNC;
			else if (!FLAGS_metasync)
				env_opt = MDB_NOMETASYNC;

			if (FLAGS_writemap)
				env_opt |= MDB_WRITEMAP;

			// Create tuning options and open the database
			rc = mdb_env_create(&db_);
			rc = mdb_env_set_mapsize(db_, FLAGS_num*FLAGS_value_size * 32L / 10);
			rc = mdb_env_open(db_, file_name, env_opt | MDB_NOSUBDIR, 0664);
			if (rc) {
				fprintf(stderr, "open error: %s\n", mdb_strerror(rc));
			}
			rc = mdb_txn_begin(db_, NULL, 0, &txn);
			rc = mdb_open(txn, NULL, flags == INT ? MDB_INTEGERKEY : 0, &dbi_);
			rc = mdb_txn_commit(txn);
		}

		void Write(DBFlags flags, Order order, DBState state,
			int num_entries, int value_size, int entries_per_batch) {
			// Create new database if state == FRESH
			if (state == FRESH) {
				if (FLAGS_use_existing_db) {
					message_ = "skipping (--use_existing_db is true)";
					return;
				}
				if (db_) {
					/*char cmd[200];
					sprintf(cmd, "del ./%s/*", FLAGS_db);
					mdb_env_close(db_);
					system(cmd);
					db_ = NULL;*/
				}
				Open(flags);
			}
			if (order == RANDOM)
				std::random_shuffle(shuff, shuff+ num_entries);

			Start();  // Do not count time taken to destroy/open

			if (num_entries != num_) {
				char msg[100];
				snprintf(msg, sizeof(msg), "(%d ops)", num_entries);
				message_ = msg;
			}

			MDB_val mkey, mval;
			MDB_txn *txn;
			char key[100];
			int ikey, flag = 0;
			if (flags == INT) {
				mkey.mv_data = &ikey;
				mkey.mv_size = sizeof(ikey);
			}
			else {
				mkey.mv_data = key;
			}
			mval.mv_size = value_size;
			if (order == SEQUENTIAL)
				flag = MDB_APPEND;
			//	flag |= MDB_RESERVE;
			// Write to database
			for (int i = 0; i < num_entries; i += entries_per_batch)
			{
				MDB_cursor *mc;
				mdb_txn_begin(db_, NULL, 0, &txn);
				mdb_cursor_open(txn, dbi_, &mc);

				for (int j = 0; j < entries_per_batch; j++) {

					const int k = (order == SEQUENTIAL) ? i + j : shuff[i + j];
					int rc;
					if (flags == INT)
						ikey = k;
					else
						mkey.mv_size = snprintf(key, sizeof(key), "%016d", k);
					bytes_ += value_size + mkey.mv_size;

					mval.mv_data = (void *)gen_.Generate(value_size).data();
					mval.mv_size = value_size;
					rc = mdb_cursor_put(mc, &mkey, &mval, flag);
					if (rc) {
						fprintf(stderr, "set error: %s\n", mdb_strerror(rc));
					}
					FinishedSingleOp();
				}
				mdb_cursor_close(mc);
				mdb_txn_commit(txn);
			}
		}

		void ReadReverse() {
			MDB_txn *txn;
			MDB_cursor *cursor;
			MDB_val key, data;

			mdb_txn_begin(db_, NULL, MDB_RDONLY, &txn);
			mdb_cursor_open(txn, dbi_, &cursor);
			while (mdb_cursor_get(cursor, &key, &data, MDB_PREV) == 0) {
				bytes_ += key.mv_size + data.mv_size;
				FinishedSingleOp();
			}
			mdb_cursor_close(cursor);
			mdb_txn_abort(txn);
		}

		void ReadSequential() {
			MDB_txn *txn;
			MDB_cursor *cursor;
			MDB_val key, data;

			mdb_txn_begin(db_, NULL, MDB_RDONLY, &txn);
			mdb_cursor_open(txn, dbi_, &cursor);
			while (mdb_cursor_get(cursor, &key, &data, MDB_NEXT) == 0) {
				bytes_ += key.mv_size + data.mv_size;
				FinishedSingleOp();
			}
			mdb_cursor_close(cursor);
			mdb_txn_abort(txn);
		}

		void ReadRandom() {
			MDB_txn *txn;
			MDB_cursor *cursor;
			MDB_val key, data;
			char ckey[100];
			key.mv_data = ckey;
			mdb_txn_begin(db_, NULL, MDB_RDONLY, &txn);
			mdb_cursor_open(txn, dbi_, &cursor);
			for (int i = 0; i < reads_; i++) {
				const int k = rand_.Next() % reads_;
				key.mv_size = snprintf(ckey, sizeof(ckey), "%016d", k);
				mdb_cursor_get(cursor, &key, &data, MDB_SET);
				FinishedSingleOp();
			}
			mdb_cursor_close(cursor);
			mdb_txn_abort(txn);
		}
	};

}  // namespace leveldb

int main(int argc, char** argv) {
	mdb_init();
	std::string default_db_path;
	for (int i = 1; i < argc; i++) {
		double d;
		int n;
		char junk;
		if (leveldb::Slice(argv[i]).starts_with("--benchmarks=")) {
			FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
		}
		else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
			FLAGS_compression_ratio = d;
		}
		else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
			(n == 0 || n == 1)) {
			FLAGS_histogram = n;
		}
		else if (sscanf(argv[i], "--metasync=%d%c", &n, &junk) == 1 &&
			(n == 0 || n == 1)) {
			FLAGS_metasync = n;
		}
		else if (sscanf(argv[i], "--writemap=%d%c", &n, &junk) == 1 &&
			(n == 0 || n == 1)) {
			FLAGS_writemap = n;
		}
		else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
			FLAGS_num = n;
		}
		else if (sscanf(argv[i], "--batch=%d%c", &n, &junk) == 1) {
			FLAGS_batch = n;
		}
		else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
			FLAGS_reads = n;
		}
		else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
			FLAGS_value_size = n;
		}
		else if (strncmp(argv[i], "--db=", 5) == 0) {
			FLAGS_db = argv[i] + 5;
		}
		else {
			fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
			exit(1);
		}
	}

	// Choose a location for the test database if none given with --db=<path>
	if (FLAGS_db == NULL) {
		GetTestDirectory(&default_db_path);
		default_db_path += "/dbbench";
		FLAGS_db = default_db_path.c_str();
	}

	shuff = (int *)malloc(FLAGS_num * sizeof(int));
	for (int i = 0; i<FLAGS_num; i++)
		shuff[i] = i;
	leveldb::Benchmark benchmark;
	benchmark.Run();
	return 0;
}
