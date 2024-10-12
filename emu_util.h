#ifndef EMU_UTIL_H_
#define EMU_UTIL_H_

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <cstdio>
#include "sys/times.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/convenience.h"
#include "db/db_impl/db_impl.h"
#include "util/cast_util.h"
#include "emu_environment.h"
#include "workload_stats.h"
#include "aux_time.h"


using namespace rocksdb;


// RocksDB-related helper functions

// Close DB in a way of detecting errors
// followed by deleting the database object when examined to determine if there were any errors. 
// Regardless of errors, it will release all resources and is irreversible.
// Flush the memtable before close 
Status CloseDB(DB *&db, const FlushOptions &flush_op);

// Reopen DB with configured options and a consistent dbptr
// use DB::Close()
Status ReopenDB(DB *&db, const Options &op, const FlushOptions &flush_op);

bool CompactionMayAllComplete(DB *db);
bool FlushMemTableMayAllComplete(DB *db); 
Status BackgroundJobMayAllCompelte(DB *&db);

void resetPointReadsStats(DB* db);

double getCurrentAverageBitsPerKey(DB* db, const Options *op);
void collectDbStats(DB* db, DbStats *stats, bool print_point_read_stats = false, uint64_t start_global_point_read_number=0, double learning_rate=1.0, bool estimate_flag=false);
Status createNewSstFile(const std::string filename_to_read, const std::string filename_to_write, const Options *op,
  EnvOptions *env_op, const ReadOptions *read_op);
Status createDbWithMonkeyPlus(const EmuEnv* _env, DB* db, DB* db_monkey,  Options *op, BlockBasedTableOptions *table_op, const WriteOptions *write_op,
  ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions *env_op, const DbStats & db_stats);
Status createDbWithMonkey(const EmuEnv* _env, DB* db, DB* db_monkey,  Options *op, BlockBasedTableOptions *table_op, const WriteOptions *write_op,
  ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions *env_op, const DbStats & db_stats);
Status createDbWithOptBpk(const EmuEnv* _env, DB* db, DB* db_optimal,Options *op, BlockBasedTableOptions *table_op, const WriteOptions *write_op,
  ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions *env_op, const DbStats & db_stats);

void getNaiveMonkeyBitsPerKey(size_t num_ingestion, size_t num_entries_per_table, double size_ratio, size_t max_files_in_L0,
		double overall_bits_per_key, std::vector<double>* naive_monkey_bits_per_key_list, bool dynamic_cmpct, bool optimize_L0_files);

// Other helper functions
void printBFBitsPerKey(DB *db);

void printEmulationOutput(const EmuEnv* _env, const QueryTracker *track, uint16_t n = 1);

void configOptions(EmuEnv* _env, Options *op, BlockBasedTableOptions *table_op, WriteOptions *write_op, ReadOptions *read_op, FlushOptions *flush_op);

void populateQueryTracker(QueryTracker *track, DB *_db, const BlockBasedTableOptions& table_options, EmuEnv* _env);

void db_point_lookup(DB* _db, const ReadOptions *read_op, const std::string key, const int verbosity, QueryTracker *query_track);

uint64_t GetTotalUsedDataBlocks(uint32_t num_levels, int verbosity);

void dump_query_stats(const DbStats & db_stats, const std::string & path);

void print_point_read_stats_distance_collector(std::vector<SimilarityResult >* point_reads_statistics_distance_collector);

void write_collected_throughput(std::vector<vector<std::pair<double, double>> > collected_throughputs, std::vector<std::string> names, std::string throughput_path, std::string bpk_path, uint32_t interval);

int runWorkload(DB* _db, const EmuEnv* _env, Options *op,
                const BlockBasedTableOptions *table_op, const WriteOptions *write_op, 
                const ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions* env_op, 
                const WorkloadDescriptor *wd, QueryTracker *query_track,
                std::vector<std::pair<double, double> >* throughput_collector = nullptr,
                std::vector<SimilarityResult >* point_reads_statistics_distance_collector = nullptr);   // run_workload internal

// utilities for cpu usage measuring COPIED FROM "https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process"
struct CpuUsage {
    double user_time_pct;
    double sys_time_pct;
};
static clock_t lastCPU, lastSysCPU, lastUserCPU;
void cpuUsageInit();
CpuUsage getCurrentCpuUsage();

std::vector<std::string> StringSplit(std::string &str, char delim);


// Print progress bar during workload execution
// n : total number of queries
// count : number of queries finished
// mini_count : keep track of current progress of percentage
inline void showProgress(const uint64_t &n, const uint64_t &count, uint64_t &mini_count) {
  if(count % (n/100) == 0){
  	if (count == n || n == 0) {
	    std::cout << ">OK!\n";
	    return;
  	} 
    if(count % (n/10) == 0) {
      std::cout << ">" << ++mini_count * 10 << "%<";
      fflush(stdout);
    }
  	std::cout << "=";
    fflush(stdout);
  }
}

// Hardcode command to clear system cache 
// May need password to get root access.
inline void clearPageCache() {
  system("sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches'");
	// sync();
	// std::ofstream ofs("/proc/sys/vm/drop_caches");
	// ofs << "3" << std::endl;
}

// Sleep program for millionseconds
inline void sleep_for_ms(uint32_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

#endif /*EMU_UTIL_H_*/
