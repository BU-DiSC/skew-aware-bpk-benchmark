// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <math.h>
#include <limits.h>
#include <atomic>
#include <mutex>
//#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <iomanip>
#include "rocksdb/iostats_context.h"
#include "rocksdb/perf_context.h"
#include "args.hxx"
#include "aux_time.h"
#include "emu_environment.h"
#include "workload_stats.h"
#include "emu_util.h"


using namespace rocksdb;

// Specify your path of workload file here
std::string ingestion_workloadPath = "./workload.txt";
std::string query_workloadPath = "./workload.txt";
std::string kDBPath = "./db_working_home";
std::string query_statsPath = "./dump_query_stats.txt";
QueryTracker global_query_tracker;
QueryTracker global_monkey_query_tracker;
QueryTracker global_workloadaware_query_tracker;



DB* db = nullptr;
DB* db_monkey = nullptr;
DB* db_monkey_plus = nullptr;
DB* db_workloadaware = nullptr;


int runExperiments(EmuEnv* _env);    // API
int parse_arguments2(int argc, char *argv[], EmuEnv* _env);

int main(int argc, char *argv[]) {
  // check emu_environment.h for the contents of EmuEnv and also the definitions of the singleton experimental environment 
  EmuEnv* _env = EmuEnv::getInstance();
  // parse the command line arguments
  if (parse_arguments2(argc, argv, _env)) {
    exit(1);
  }
    
  my_clock start_time, end_time;
  std::cout << "Starting experiments ..."<<std::endl;
  if (my_clock_get_time(&start_time) == -1) {
    std::cerr << "Failed to get experiment start time" << std::endl;
  }
  int s = runExperiments(_env); 
  if (my_clock_get_time(&end_time) == -1) {
    std::cerr << "Failed to get experiment end time" << std::endl;
  }
  double experiment_exec_time = getclock_diff_ns(start_time, end_time);

  std::cout << std::endl << std::fixed << std::setprecision(2) 
            << "===== End of all experiments in "
            << experiment_exec_time/1000000 << "ms !! ===== "<< std::endl;
  
  // show average results for the number of experiment runs
  printEmulationOutput(_env, &global_query_tracker, _env->experiment_runs);
  std::cout << "==========================================================" << std::endl;
  printEmulationOutput(_env, &global_monkey_query_tracker, _env->experiment_runs);
  std::cout << "==========================================================" << std::endl;
  printEmulationOutput(_env, &global_workloadaware_query_tracker, _env->experiment_runs);
  
  std::cout << "===== Average stats of " << _env->experiment_runs << " runs ====="  << std::endl;


  return 0;
}

// Run rocksdb experiments for experiment_runs
// 1.Initiate experiments environment and rocksDB options
// 2.Preload workload into memory
// 3.Run workload and collect stas for each run
int runExperiments(EmuEnv* _env) {
 
  Options options;
  WriteOptions write_options;
  ReadOptions read_options;
  BlockBasedTableOptions table_options;
  FlushOptions flush_options;
 
  WorkloadDescriptor ingestion_wd(_env->ingestion_wpath);
  WorkloadDescriptor query_wd(_env->query_wpath);
 
  //WorkloadDescriptor wd(workloadPath);
  // init RocksDB configurations and experiment settings
  configOptions(_env, &options, &table_options, &write_options, &read_options, &flush_options);
   EnvOptions env_options (options);
  // parsing workload
  loadWorkload(&ingestion_wd);
  loadWorkload(&query_wd);
      
  uint64_t bloom_false_positives;
  
  // Starting experiments
  assert(_env->experiment_runs >= 1);
  for (int i = 0; i < _env->experiment_runs; ++i) {
    // Reopen DB
    if (_env->destroy) {
      //DestroyDB(kDBPath, options);
      Status destroy_status = DestroyDB(_env->path, options);
      if (!destroy_status.ok()) std::cout << destroy_status.ToString() << std::endl;
    }
    DestroyDB(_env->path + "-monkey", options);
    DestroyDB(_env->path + "-monkey-plus", options);
    DestroyDB(_env->path + "-workloadaware", options);
    
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    Status s = DB::Open(options, _env->path, &db);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    assert(s.ok());

    // Prepare Perf and I/O stats
    QueryTracker *ingestion_query_track = new QueryTracker();   // stats tracker for each run
  
    QueryTracker *query_track = new QueryTracker();
   
    
    get_perf_context()->ClearPerLevelPerfContext();
    get_perf_context()->Reset();
    get_iostats_context()->Reset();
    
    // Run workload
    runWorkload(db, _env, &options, &table_options, &write_options, &read_options, &flush_options, &ingestion_wd, ingestion_query_track);
    s = CloseDB(db, flush_options);
    assert(s.ok());
    s= DB::Open(options, _env->path, &db);
    assert(s.ok());
    
    db->GetOptions().statistics->Reset();
    get_perf_context()->EnablePerLevelPerfContext();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);
    runWorkload(db, _env, &options, &table_options, &write_options, &read_options, &flush_options, &query_wd, query_track);

    // Collect stats after per run
    SetPerfLevel(kDisable);
    populateQueryTracker(query_track, db, table_options, _env);
    if (_env->verbosity > 1) {
      std::string state;
      db->GetProperty("rocksdb.cfstats-no-file-histogram", &state);
      std::cout << state << std::endl;
    }
  
    dumpStats(&global_query_tracker, query_track);    // dump stat of each run into acmulative stat
    bloom_false_positives = query_track->bloom_sst_hit_count - query_track->bloom_sst_true_positive_count;
    std::cout << "accessed data blocks: " << query_track->data_block_read_count << std::endl;
    std::cout << "overall false positives: " << bloom_false_positives  << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate: " << 
      bloom_false_positives*100.0/(bloom_false_positives + query_track->bloom_sst_miss_count) << "%" << std::endl;
    delete query_track;
    delete ingestion_query_track;
    ingestion_query_track = NULL;
    ReopenDB(db, options, flush_options);
    get_perf_context()->ClearPerLevelPerfContext();
    
    options.create_if_missing = true;
    table_options.bpk_alloc_type = rocksdb::BitsPerKeyAllocationType::kMonkeyBpkAlloc;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    s = DB::Open(options, _env->path + "-monkey", &db_monkey);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    ingestion_query_track = new QueryTracker();
    runWorkload(db_monkey, _env, &options, &table_options, &write_options, &read_options, &flush_options, &ingestion_wd, ingestion_query_track);
    s = ReopenDB(db_monkey, options, flush_options);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    get_iostats_context()->Reset();
    get_perf_context()->Reset();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);
    get_perf_context()->EnablePerLevelPerfContext();
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    QueryTracker *monkey_query_track = new QueryTracker();
    db_monkey->GetOptions().statistics->Reset();
    runWorkload(db_monkey, _env, &options, &table_options, &write_options, &read_options, &flush_options, &query_wd, monkey_query_track);
    SetPerfLevel(kDisable);
    populateQueryTracker(monkey_query_track, db_monkey, table_options, _env);
    dumpStats(&global_monkey_query_tracker, monkey_query_track); 
    if (_env->verbosity > 1) {
      std::string state;
      db->GetProperty("rocksdb.cfstats-no-file-histogram", &state);
      std::cout << state << std::endl;
    }
    std::cout << "monkey sst hit : " << monkey_query_track->bloom_sst_miss_count << "\t monkey tp : " << monkey_query_track->bloom_sst_true_positive_count << std::endl;
    bloom_false_positives = monkey_query_track->bloom_sst_hit_count - monkey_query_track->bloom_sst_true_positive_count;
    std::cout << "accessed data blocks (monkey): " << monkey_query_track->data_block_read_count << std::endl;
    std::cout << "overall false positives (monkey): " << bloom_false_positives << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate (monkey): " << 
      bloom_false_positives*100.0/(bloom_false_positives + monkey_query_track->bloom_sst_miss_count) << "%" << std::endl;
    printBFBitsPerKey(db_monkey);
    delete monkey_query_track;
    delete ingestion_query_track;
    ingestion_query_track = NULL;
    CloseDB(db_monkey, flush_options);
    
    table_options.bpk_alloc_type = rocksdb::BitsPerKeyAllocationType::kWorkloadAwareBpkAlloc;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    s = DB::Open(options, _env->path + "-workloadaware", &db_workloadaware);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    ingestion_query_track = new QueryTracker();
    runWorkload(db_workloadaware, _env, &options, &table_options, &write_options, &read_options, &flush_options, &ingestion_wd, ingestion_query_track);
    s = ReopenDB(db_workloadaware, options, flush_options);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    get_iostats_context()->Reset();
    get_perf_context()->Reset();
    get_perf_context()->ClearPerLevelPerfContext();
    get_perf_context()->EnablePerLevelPerfContext();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);
    QueryTracker *workloadaware_query_track = new QueryTracker();
    db_workloadaware->GetOptions().statistics->Reset();
    runWorkload(db_workloadaware, _env, &options, &table_options, &write_options, &read_options, &flush_options, &query_wd, workloadaware_query_track);
    SetPerfLevel(kDisable);
    populateQueryTracker(workloadaware_query_track, db_workloadaware, table_options, _env);
    dumpStats(&global_workloadaware_query_tracker, workloadaware_query_track); 
    if (_env->verbosity > 1) {
      std::string state;
      db->GetProperty("rocksdb.cfstats-no-file-histogram", &state);
      std::cout << state << std::endl;
    }
    bloom_false_positives = workloadaware_query_track->bloom_sst_hit_count - workloadaware_query_track->bloom_sst_true_positive_count;
    std::cout << "accessed data blocks (workloadaware): " << workloadaware_query_track->data_block_read_count << std::endl;
    std::cout << "overall false positives (workloadaware): " << bloom_false_positives << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate (workloadaware): " << 
      bloom_false_positives*100.0/(bloom_false_positives + workloadaware_query_track->bloom_sst_miss_count) << "%" << std::endl;
    s = BackgroundJobMayAllCompelte(db_workloadaware);
    assert(s.ok());
    printBFBitsPerKey(db_workloadaware);
    delete workloadaware_query_track;

    CloseDB(db_workloadaware, flush_options);
    std::cout << "End of experiment run: " << i+1 << std::endl;
    std::cout << std::endl;
    CloseDB(db, flush_options);
  }
  return 0;
}



int parse_arguments2(int argc, char *argv[], EmuEnv* _env) {
  args::ArgumentParser parser("RocksDB_parser.", "");

  args::Group group1(parser, "This group is all exclusive:", args::Group::Validators::DontCare);
  args::Group group4(parser, "Optional switches and parameters:", args::Group::Validators::DontCare);
  args::ValueFlag<int> size_ratio_cmd(group1, "T", "The size ratio of two adjacent levels  [def: 2]", {'T', "size_ratio"});
  args::ValueFlag<int> buffer_size_in_pages_cmd(group1, "P", "The number of pages that can fit into a buffer [def: 128]", {'P', "buffer_size_in_pages"});
  args::ValueFlag<int> entries_per_page_cmd(group1, "B", "The number of entries that fit into a page [def: 128]", {'B', "entries_per_page"});
  args::ValueFlag<int> entry_size_cmd(group1, "E", "The size of a key-value pair inserted into DB [def: 128 B]", {'E', "entry_size"});
  args::ValueFlag<long> buffer_size_cmd(group1, "M", "The size of a buffer that is configured manually [def: 2 MB]", {'M', "memory_size"});
  args::ValueFlag<int> file_to_memtable_size_ratio_cmd(group1, "file_to_memtable_size_ratio", "The size of a file over the size of configured buffer size [def: 1]", {'f', "file_to_memtable_size_ratio"});
  args::ValueFlag<long> file_size_cmd(group1, "file_size", "The size of a file that is configured manually [def: 2 MB]", {'F', "file_size"});
  args::ValueFlag<long> level0_file_num_compaction_trigger_cmd(group1, "#files in level0", "The number of files to trigger level-0 compaction. [def: 2]", {"l0_files", "level0_files_cmpct_trigger"});
  args::ValueFlag<int> compaction_pri_cmd(group1, "compaction_pri", "[Compaction priority: 1 for kMinOverlappingRatio, 2 for kByCompensatedSize, 3 for kOldestLargestSeqFirst, 4 for kOldestSmallestSeqFirst; def: 2]", {'C', "compaction_pri"});
  args::ValueFlag<int> compaction_style_cmd(group1, "compaction_style", "[Compaction style: 1 for kCompactionStyleLevel, 2 for kCompactionStyleUniversal, 3 for kCompactionStyleFIFO, 4 for kCompactionStyleNone; def: 1]", {'c', "compaction_style"});
  args::ValueFlag<double> bits_per_key_cmd(group1, "bits_per_key", "The number of bits per key assigned to Bloom filter [def: 10]", {'b', "bits_per_key"});
  args::Flag no_dynamic_compaction_level_bytes_cmd(group1, "dynamic level compaction", "disable dynamic level compaction bytes", {"no_dynamic_cmpct", "no_dynamic_compaction"});


  args::Flag no_block_cache_cmd(group1, "block_cache", "Disable block cache", {"dis_blk_cache", "disable_block_cache"});
  args::ValueFlag<int> block_cache_capacity_cmd(group1, "block_cache_capacity", "The capacity (kilobytes) of block cache [def: 8 MB]", {"BCC", "block_cache_capacity"});
  args::ValueFlag<double> block_cache_high_priority_ratio_cmd(group1, "block_cache_high_priority_ratio", "The ratio of capacity reserved for high priority blocks in block cache [def: 1.0 ]", {"BCHPR", "block_cache_high_priority_ratio"});
  args::ValueFlag<int> experiment_runs_cmd(group1, "experiment_runs", "The number of experiments repeated each time [def: 1]", {'R', "run"});
//  args::ValueFlag<long> num_inserts_cmd(group1, "inserts", "The number of unique inserts to issue in the experiment [def: 0]", {'i', "inserts"});
  args::Flag clear_sys_page_cache_cmd(group4, "clear_sys_page_cache", "Clear system page cache before experiments", {"cc", "clear_cache"});
  args::Flag destroy_cmd(group4, "destroy_db", "Destroy and recreate the database", {"dd", "destroy_db"});
  args::Flag direct_reads_cmd(group4, "use_direct_reads", "Use direct reads", {"dr", "use_direct_reads"});
  args::Flag direct_writes_cmd(group4, "use_direct_writes", "Use direct writes", {"dw", "use_direct_writes"});
  args::Flag low_pri_cmd(group4, "low_pri", "write request is of lower priority if compaction is behind", {"lp", "low_priority"});
  args::ValueFlag<int> verbosity_cmd(group4, "verbosity", "The verbosity level of execution [0,1,2; def: 0]", {'V', "verbosity"});

  args::ValueFlag<std::string> path_cmd(group4, "path", "path for writing the DB and all the metadata files", {'p', "path"});
  args::ValueFlag<std::string> ingestion_wpath_cmd(group4, "wpath", "path for ingestion workload files", {"iwp", "ingestion-wpath"});
  args::ValueFlag<std::string> query_wpath_cmd(group4, "wpath", "path for query workload files", {"qwp", "query-wpath"});
  args::ValueFlag<std::string> query_stats_path_cmd(group4, "query_stats_path", "path for dumping query stats", {"qsp", "query-stats-path"});
  args::ValueFlag<int> num_levels_cmd(group1, "L", "The number of levels to fill up with data [def: -1]", {'L', "num_levels"});

  args::Flag print_sst_stat_cmd(group4, "print_sst_stat", "print the stat of SST files", {"ps", "print_sst"});
  args::Flag dump_query_stats_cmd(group4, "dump_query_stats", "print the stats of queries", {"dqs", "dump_query_stats"});

  try {
      parser.ParseCLI(argc, argv);
  }
  catch (args::Help&) {
      std::cout << parser;
      exit(0);
      // return 0;
  }
  catch (args::ParseError& e) {
      std::cerr << e.what() << std::endl;
      std::cerr << parser;
      return 1;
  }
  catch (args::ValidationError& e) {
      std::cerr << e.what() << std::endl;
      std::cerr << parser;
      return 1;
  }

  _env->size_ratio = size_ratio_cmd ? args::get(size_ratio_cmd) : 2;
  _env->buffer_size_in_pages = buffer_size_in_pages_cmd ? args::get(buffer_size_in_pages_cmd) : 128;
  _env->entries_per_page = entries_per_page_cmd ? args::get(entries_per_page_cmd) : 128;
  _env->entry_size = entry_size_cmd ? args::get(entry_size_cmd) : 128;
  _env->buffer_size = buffer_size_cmd ? args::get(buffer_size_cmd) : _env->buffer_size_in_pages * _env->entries_per_page * _env->entry_size;
  _env->file_to_memtable_size_ratio = file_to_memtable_size_ratio_cmd ? args::get(file_to_memtable_size_ratio_cmd) : 1;
  _env->file_size = file_size_cmd ? args::get(file_size_cmd) : _env->file_to_memtable_size_ratio * _env-> buffer_size;
  _env->verbosity = verbosity_cmd ? args::get(verbosity_cmd) : 0;
  //num_lookup_threads = num_lookup_threads_cmd ? args::get(num_lookup_threads_cmd) : 1;
  _env->compaction_pri = compaction_pri_cmd ? args::get(compaction_pri_cmd) : 1;
  _env->level_compaction_dynamic_level_bytes = no_dynamic_compaction_level_bytes_cmd ? false : true;
  _env->level0_file_num_compaction_trigger = level0_file_num_compaction_trigger_cmd ? args::get(level0_file_num_compaction_trigger_cmd) : 2;
  _env->no_block_cache = no_block_cache_cmd ? true : false;
  _env->low_pri = low_pri_cmd ? true : false;
  _env->block_cache_capacity = block_cache_capacity_cmd ? args::get(block_cache_capacity_cmd) : 8*1024;
  _env->block_cache_high_priority_ratio = block_cache_high_priority_ratio_cmd ? args::get(block_cache_high_priority_ratio_cmd) : 1.0;
  _env->bits_per_key = bits_per_key_cmd ? args::get(bits_per_key_cmd) : 10;

  _env->experiment_runs = experiment_runs_cmd ? args::get(experiment_runs_cmd) : 1;
//  _env->num_inserts = num_inserts_cmd ? args::get(num_inserts_cmd) : 0;
  _env->clear_sys_page_cache = clear_sys_page_cache_cmd ? true : false;
  _env->destroy = destroy_cmd ? true : false;
  _env->use_direct_reads = direct_reads_cmd ? true : false;
  _env->use_direct_io_for_flush_and_compaction = direct_writes_cmd ? true : false;
  _env->path = path_cmd ? args::get(path_cmd) : kDBPath;
  _env->ingestion_wpath = ingestion_wpath_cmd ? args::get(ingestion_wpath_cmd) : ingestion_workloadPath;
  _env->query_wpath = query_wpath_cmd ? args::get(query_wpath_cmd) : query_workloadPath;
  _env->num_levels = num_levels_cmd ? args::get(num_levels_cmd) : 7;
  _env->print_sst_stat = print_sst_stat_cmd ? true : false;
  _env->dump_query_stats = dump_query_stats_cmd ? true : false;
  _env->dump_query_stats_filename = query_stats_path_cmd ? args::get(query_stats_path_cmd) : query_statsPath;
  return 0;
}




