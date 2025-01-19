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
std::string workloadPath = "./workload.txt";
std::string kDBPath = "./db_working_home";
std::string query_statsPath = "./dump_query_stats.txt";
std::string throughputPath = "./throughputs.txt";
std::string bpkPath = "./tracked_avg_bpk.txt";
QueryTracker global_query_tracker;
QueryTracker global_monkey_top_down_query_tracker;
QueryTracker global_monkey_bottom_up_query_tracker;
QueryTracker global_mnemosyne_query_tracker;



DB* db = nullptr;
DB* db_monkey_bottom_up = nullptr;
DB* db_monkey_top_down = nullptr;
DB* db_mnemosyne = nullptr;


int runExperiments(EmuEnv* _env);    // API
int parse_arguments2(int argc, char *argv[], EmuEnv* _env);
void merge_tput_vectors(std::vector<std::pair<double, double>>* origin_tput, std::vector<std::pair<double, double>>* new_tput);


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
  //std::cout << "==========================================================" << std::endl;
  printEmulationOutput(_env, &global_monkey_top_down_query_tracker, _env->experiment_runs);
  std::cout << "==========================================================" << std::endl;
  printEmulationOutput(_env, &global_monkey_bottom_up_query_tracker, _env->experiment_runs);
  std::cout << "==========================================================" << std::endl;
  printEmulationOutput(_env, &global_mnemosyne_query_tracker, _env->experiment_runs);
  
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
 
  WorkloadDescriptor query_wd(_env->query_wpath);
 
   EnvOptions env_options (options);
  // parsing workload
  loadWorkload(&query_wd);
      
  uint64_t bloom_false_positives;
  std::vector<std::pair<double, double>> throughput_and_bpk_collector;
  std::vector<std::pair<double, double>> monkey_top_down_throughput_and_bpk_collector;
  std::vector<std::pair<double, double>> monkey_bottom_up_throughput_and_bpk_collector;
  std::vector<std::pair<double, double>> mnemosyne_throughput_and_bpk_collector;
  std::vector<std::pair<double, double>> temp_collector;

  // Starting experiments
  assert(_env->experiment_runs >= 1);
  for (int i = 0; i < _env->experiment_runs; ++i) {

    //WorkloadDescriptor wd(workloadPath);
    // init RocksDB configurations and experiment settings
    configOptions(_env, &options, &table_options, &write_options, &read_options, &flush_options);

    // Reopen DB 
    if (_env->destroy || i > 0) {
      DestroyDB(_env->path, options);
      DestroyDB(_env->path + "-monkey-top-down", options);
      DestroyDB(_env->path + "-monkey-bottom-up", options);
      DestroyDB(_env->path + "-mnemosyne", options);
    }
    Status s;
    // Prepare Perf and I/O stats
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    
    options.level_compaction_dynamic_level_bytes = _env->level_compaction_dynamic_level_bytes;
   
    QueryTracker *query_track = new QueryTracker();
   
    s = DB::Open(options, _env->path, &db);
    db->GetOptions().db_paths.emplace_back(_env->path, _env->file_size);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    assert(s.ok());
   
    
    get_perf_context()->ClearPerLevelPerfContext();
    get_perf_context()->Reset();
    get_iostats_context()->Reset();
    
    // Run workload
    db->GetOptions().statistics->Reset();
    get_perf_context()->EnablePerLevelPerfContext();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);

    if (_env->throughput_collect_interval == 0) { 
      runWorkload(db, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, query_track);
    } else {
      temp_collector.clear();
      runWorkload(db, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, query_track, &temp_collector);
      merge_tput_vectors(&throughput_and_bpk_collector, &temp_collector);
    }
    
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
    std::cout << "read bytes: " << query_track->bytes_read << std::endl;
    std::cout << "overall false positives: " << bloom_false_positives  << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate: " << 
      bloom_false_positives*100.0/(bloom_false_positives + query_track->bloom_sst_miss_count) << "%" << std::endl;
    if (query_track->point_lookups_completed + query_track->zero_point_lookups_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "point query latency: " <<  static_cast<double>(query_track->point_lookups_cost +
      query_track->zero_point_lookups_cost)/(query_track->point_lookups_completed + query_track->zero_point_lookups_completed)/1000000 << " (ms/query)" << std::endl;
    }
    if (query_track->inserts_completed + query_track->updates_completed + query_track->point_deletes_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "ingestion latency: " <<  static_cast<double>(query_track->inserts_cost +
      query_track->updates_cost + query_track->point_deletes_cost)/(query_track->inserts_completed + query_track->updates_completed + query_track->point_deletes_completed)/1000000 << " (ms/ops)" << std::endl;
    }
    if (query_track->total_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "avg operation latency: " <<  static_cast<double>(query_track->workload_exec_time)/query_track->total_completed/1000000 << " (ms/ops)" << std::endl;
    }
    delete query_track;
    get_perf_context()->ClearPerLevelPerfContext();
    CloseDB(db, flush_options);

    if (_env->block_cache_capacity == 0) {
      ;// do nothing
    } else {
      table_options.block_cache.reset();
      table_options.block_cache = NewLRUCache(_env->block_cache_capacity*1024, -1, false, _env->block_cache_high_priority_ratio);
      ;// invoke manual block_cache
    }
    options.create_if_missing = true;
    table_options.bpk_alloc_type = rocksdb::BitsPerKeyAllocationType::kNaiveMonkeyBpkAlloc;
    table_options.modular_filters = false;
    std::vector<double> bpk_list (_env->num_levels, _env->bits_per_key);
    long space_amp = std::min((long)query_wd.update_num, (long)(query_wd.insert_num*(1.0 - 1.0/_env->size_ratio)));
    getNaiveMonkeyBitsPerKey(query_wd.insert_num + space_amp, floor(_env->buffer_size/_env->entry_size), _env->size_ratio, 
            _env->level0_file_num_compaction_trigger, _env->bits_per_key, &bpk_list, true, false);
    table_options.naive_monkey_bpk_list = bpk_list;
    table_options.modular_filters = false;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    options.level_compaction_dynamic_level_bytes = true;
    s = DB::Open(options, _env->path + "-monkey-bottom-up", &db_monkey_bottom_up);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    get_iostats_context()->Reset();
    get_perf_context()->Reset();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);
    get_perf_context()->EnablePerLevelPerfContext();
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    
    QueryTracker *monkey_bottom_up_query_track = new QueryTracker();
    db_monkey_bottom_up->GetOptions().statistics->Reset();
    
    if (_env->throughput_collect_interval == 0) { 
      runWorkload(db_monkey_bottom_up, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, monkey_bottom_up_query_track);
    } else {
      temp_collector.clear();
      runWorkload(db_monkey_bottom_up, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, monkey_bottom_up_query_track, &temp_collector);
      merge_tput_vectors(&monkey_bottom_up_throughput_and_bpk_collector, &temp_collector);
    }
    
    SetPerfLevel(kDisable);
    populateQueryTracker(monkey_bottom_up_query_track, db_monkey_bottom_up, table_options, _env);
    dumpStats(&global_monkey_bottom_up_query_tracker, monkey_bottom_up_query_track); 
    if (_env->verbosity > 1) {
      std::string state;
      db->GetProperty("rocksdb.cfstats-no-file-histogram", &state);
      std::cout << state << std::endl;
    }
    std::cout << "monkey-bottom-up sst hit : " << monkey_bottom_up_query_track->bloom_sst_miss_count << "\t monkey tp : " << monkey_bottom_up_query_track->bloom_sst_true_positive_count << std::endl;
    bloom_false_positives = monkey_bottom_up_query_track->bloom_sst_hit_count - monkey_bottom_up_query_track->bloom_sst_true_positive_count;
    std::cout << "accessed data blocks (monkey-bottom-up): " << monkey_bottom_up_query_track->data_block_read_count << std::endl;
    std::cout << "read bytes (monkey-bottom-up): " << monkey_bottom_up_query_track->bytes_read << std::endl;
    std::cout << "overall false positives (monkey-bottom-up): " << bloom_false_positives << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate (monkey-bottom-up): " << 
      bloom_false_positives*100.0/(bloom_false_positives + monkey_bottom_up_query_track->bloom_sst_miss_count) << "%" << std::endl;
    if (monkey_bottom_up_query_track->point_lookups_completed + monkey_bottom_up_query_track->zero_point_lookups_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "point query latency (monkey-bottom-up): " <<  static_cast<double>(monkey_bottom_up_query_track->point_lookups_cost +
      monkey_bottom_up_query_track->zero_point_lookups_cost)/(monkey_bottom_up_query_track->point_lookups_completed + monkey_bottom_up_query_track->zero_point_lookups_completed)/1000000 << " (ms/query)" << std::endl;
    }
    if (monkey_bottom_up_query_track->inserts_completed + monkey_bottom_up_query_track->updates_completed + monkey_bottom_up_query_track->point_deletes_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "ingestion latency (monkey-bottom-up): " <<  static_cast<double>(monkey_bottom_up_query_track->inserts_cost +
      monkey_bottom_up_query_track->updates_cost + monkey_bottom_up_query_track->point_deletes_cost)/(monkey_bottom_up_query_track->inserts_completed + monkey_bottom_up_query_track->updates_completed + monkey_bottom_up_query_track->point_deletes_completed)/1000000 << " (ms/ops)" << std::endl;
    }
    if (monkey_bottom_up_query_track->total_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "avg operation latency (monkey-bottom-up): " <<  static_cast<double>(monkey_bottom_up_query_track->workload_exec_time)/monkey_bottom_up_query_track->total_completed/1000000 << " (ms/ops)" << std::endl;
    }
    delete monkey_bottom_up_query_track;
    CloseDB(db_monkey_bottom_up, flush_options);
 
    if (_env->block_cache_capacity == 0) {
      ;// do nothing
    } else {
      table_options.block_cache.reset();
      table_options.block_cache = NewLRUCache(_env->block_cache_capacity*1024, -1, false, _env->block_cache_high_priority_ratio);
      ;// invoke manual block_cache
    }

    table_options.bpk_alloc_type = rocksdb::BitsPerKeyAllocationType::kNaiveMonkeyBpkAlloc;
    bpk_list.resize(bpk_list.size(), _env->bits_per_key);
    space_amp = std::min((long)query_wd.update_num, (long)(query_wd.insert_num*(1.0 - 1.0/_env->size_ratio)));
    getNaiveMonkeyBitsPerKey(query_wd.insert_num + space_amp, floor(_env->buffer_size/_env->entry_size), _env->size_ratio, 
            _env->level0_file_num_compaction_trigger, _env->bits_per_key, &bpk_list, false, false);
    table_options.naive_monkey_bpk_list = bpk_list;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    options.level_compaction_dynamic_level_bytes = false;
    s = DB::Open(options, _env->path + "-monkey-top-down", &db_monkey_top_down);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    get_iostats_context()->Reset();
    get_perf_context()->Reset();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);
    get_perf_context()->EnablePerLevelPerfContext();
    options.statistics = ROCKSDB_NAMESPACE::CreateDBStatistics();
    QueryTracker *monkey_top_down_query_track = new QueryTracker();
    db_monkey_top_down->GetOptions().statistics->Reset();

    if (_env->throughput_collect_interval == 0) { 
      runWorkload(db_monkey_top_down, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, monkey_top_down_query_track);
    } else {
      temp_collector.clear();
      runWorkload(db_monkey_top_down, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, monkey_top_down_query_track, &temp_collector);
      merge_tput_vectors(&monkey_top_down_throughput_and_bpk_collector, &temp_collector);
    }
    
    SetPerfLevel(kDisable);
    populateQueryTracker(monkey_top_down_query_track, db_monkey_top_down, table_options, _env);
    dumpStats(&global_monkey_top_down_query_tracker, monkey_top_down_query_track); 
    if (_env->verbosity > 1) {
      std::string state;
      db_monkey_top_down->GetProperty("rocksdb.cfstats-no-file-histogram", &state);
      std::cout << state << std::endl;
    }
    std::cout << "monkey-top-down sst hit : " << monkey_top_down_query_track->bloom_sst_miss_count << "\t monkey-top-down tp : " << monkey_top_down_query_track->bloom_sst_true_positive_count << std::endl;
    bloom_false_positives = monkey_top_down_query_track->bloom_sst_hit_count - monkey_top_down_query_track->bloom_sst_true_positive_count;
    std::cout << "accessed data blocks (monkey-top-down): " << monkey_top_down_query_track->data_block_read_count << std::endl;
    std::cout << "read bytes (monkey-top-down): " << monkey_top_down_query_track->bytes_read << std::endl;
    std::cout << "overall false positives (monkey-top-down): " << bloom_false_positives << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate (monkey-top-down): " << 
      bloom_false_positives*100.0/(bloom_false_positives + monkey_top_down_query_track->bloom_sst_miss_count) << "%" << std::endl;
    if (monkey_top_down_query_track->point_lookups_completed + monkey_top_down_query_track->zero_point_lookups_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "point query latency (monkey-top-down): " <<  static_cast<double>(monkey_top_down_query_track->point_lookups_cost +
      monkey_top_down_query_track->zero_point_lookups_cost)/(monkey_top_down_query_track->point_lookups_completed + monkey_top_down_query_track->zero_point_lookups_completed)/1000000 << " (ms/query)" << std::endl;
    }
    if (monkey_top_down_query_track->inserts_completed + monkey_top_down_query_track->updates_completed + monkey_top_down_query_track->point_deletes_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "ingestion latency (monkey-top-down): " <<  static_cast<double>(monkey_top_down_query_track->inserts_cost +
      monkey_top_down_query_track->updates_cost + monkey_top_down_query_track->point_deletes_cost)/(monkey_top_down_query_track->inserts_completed + monkey_top_down_query_track->updates_completed + monkey_top_down_query_track->point_deletes_completed)/1000000 << " (ms/ops)" << std::endl;
    }
    if (monkey_top_down_query_track->total_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "avg operation latency (monkey-top-down): " <<  static_cast<double>(monkey_top_down_query_track->workload_exec_time)/monkey_top_down_query_track->total_completed/1000000 << " (ms/ops)" << std::endl;
    }
    delete monkey_top_down_query_track;
    CloseDB(db_monkey_top_down, flush_options);
  
    if (_env->block_cache_capacity == 0) {
      ;// do nothing
    } else {
      table_options.block_cache.reset();
      table_options.block_cache = NewLRUCache(_env->block_cache_capacity*1024, -1, false, _env->block_cache_high_priority_ratio);
      ;// invoke manual block_cache
    }
    table_options.bpk_alloc_type = rocksdb::BitsPerKeyAllocationType::kMnemosyneBpkAlloc;
    table_options.modular_filters = true;
    table_options.max_bits_per_key_granularity = _env->bits_per_key;
    table_options.max_modulars = 6;
    options.table_factory.reset(NewBlockBasedTableFactory(table_options));
    options.level_compaction_dynamic_level_bytes = _env->level_compaction_dynamic_level_bytes;
    //options.track_point_read_number_window_size = 16;
    s = DB::Open(options, _env->path + "-mnemosyne", &db_mnemosyne);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;
    db_mnemosyne->GetOptions().db_paths.emplace_back(_env->path + "-mnemosyne", _env->file_size);
    get_iostats_context()->Reset();
    get_perf_context()->Reset();
    get_perf_context()->ClearPerLevelPerfContext();
    get_perf_context()->EnablePerLevelPerfContext();
    SetPerfLevel(rocksdb::PerfLevel::kEnableTime);
    QueryTracker *mnemosyne_query_track = new QueryTracker();
    db_mnemosyne->GetOptions().statistics->Reset();
    
    if (_env->throughput_collect_interval == 0) { 
      runWorkload(db_mnemosyne, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, mnemosyne_query_track);
    } else {
      temp_collector.clear();
      runWorkload(db_mnemosyne, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, mnemosyne_query_track, &temp_collector);
      merge_tput_vectors(&mnemosyne_throughput_and_bpk_collector, &temp_collector);
    }
    SetPerfLevel(kDisable);
    populateQueryTracker(mnemosyne_query_track, db_mnemosyne, table_options, _env);
    dumpStats(&global_mnemosyne_query_tracker, mnemosyne_query_track); 
    if (_env->verbosity > 1) {
      std::string state;
      db->GetProperty("rocksdb.cfstats-no-file-histogram", &state);
      std::cout << state << std::endl;
    }
    bloom_false_positives = mnemosyne_query_track->bloom_sst_hit_count - mnemosyne_query_track->bloom_sst_true_positive_count;
    std::cout << "accessed data blocks (mnemosyne): " << mnemosyne_query_track->data_block_read_count << std::endl;
    std::cout << "read bytes (mnemosyne): " << mnemosyne_query_track->bytes_read << std::endl;
    std::cout << "overall false positives (mnemosyne): " << bloom_false_positives << std::endl;
    std::cout << std::fixed << std::setprecision(6) << "overall false positive rate (mnemosyne): " << 
      bloom_false_positives*100.0/(bloom_false_positives + mnemosyne_query_track->bloom_sst_miss_count) << "%" << std::endl;
    if (mnemosyne_query_track->point_lookups_completed + mnemosyne_query_track->zero_point_lookups_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "point query latency (mnemosyne): " <<  static_cast<double>(mnemosyne_query_track->point_lookups_cost +
      mnemosyne_query_track->zero_point_lookups_cost)/(mnemosyne_query_track->point_lookups_completed + mnemosyne_query_track->zero_point_lookups_completed)/1000000 << " (ms/query)" << std::endl;
    }
    if (mnemosyne_query_track->inserts_completed + mnemosyne_query_track->updates_completed + mnemosyne_query_track->point_deletes_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "ingestion latency (mnemosyne): " <<  static_cast<double>(mnemosyne_query_track->inserts_cost +
      mnemosyne_query_track->updates_cost + mnemosyne_query_track->point_deletes_cost)/(mnemosyne_query_track->inserts_completed + mnemosyne_query_track->updates_completed + mnemosyne_query_track->point_deletes_completed)/1000000 << " (ms/ops)" << std::endl;
    }
    if (mnemosyne_query_track->total_completed > 0) {
      std::cout << std::fixed << std::setprecision(6) << "avg operation latency (mnemosyne): " <<  static_cast<double>(mnemosyne_query_track->workload_exec_time)/mnemosyne_query_track->total_completed/1000000 << " (ms/ops)" << std::endl;
    }
    s = BackgroundJobMayAllCompelte(db_mnemosyne);
    assert(s.ok());
    delete mnemosyne_query_track;

    CloseDB(db_mnemosyne, flush_options);

    if (_env->block_cache_capacity == 0) {
      ;// do nothing
    } else {
      table_options.block_cache.reset();
      table_options.block_cache = NewLRUCache(_env->block_cache_capacity*1024, -1, false, _env->block_cache_high_priority_ratio);
      ;// invoke manual block_cache
    }
    std::cout << "End of experiment run: " << i+1 << std::endl;
    std::cout << std::endl;
  }

  if (_env->throughput_collect_interval > 0) {
    for (int i = 0; i < throughput_and_bpk_collector.size(); i++) {
       throughput_and_bpk_collector[i].first /= _env->experiment_runs;
       throughput_and_bpk_collector[i].second /= _env->experiment_runs;
    }
    for (int i = 0; i < monkey_top_down_throughput_and_bpk_collector.size(); i++) {
      monkey_top_down_throughput_and_bpk_collector[i].first /= _env->experiment_runs;
      monkey_top_down_throughput_and_bpk_collector[i].second /= _env->experiment_runs;
    }
    for (int i = 0; i < monkey_bottom_up_throughput_and_bpk_collector.size(); i++) {
      monkey_bottom_up_throughput_and_bpk_collector[i].first /= _env->experiment_runs;
      monkey_bottom_up_throughput_and_bpk_collector[i].second /= _env->experiment_runs;
    }
    for (int i = 0; i < mnemosyne_throughput_and_bpk_collector.size(); i++) {
     mnemosyne_throughput_and_bpk_collector[i].first /= _env->experiment_runs;
     mnemosyne_throughput_and_bpk_collector[i].second /= _env->experiment_runs;
    }
    write_collected_throughput({throughput_and_bpk_collector, monkey_bottom_up_throughput_and_bpk_collector, monkey_top_down_throughput_and_bpk_collector, mnemosyne_throughput_and_bpk_collector}, {"uniform", "monkey-bottom-up", "monkey-top-down", "mnemosyne"}, throughputPath, bpkPath, _env->throughput_collect_interval);
    //write_collected_throughput({monkey_bottom_up_throughput_and_bpk_collector, monkey_top_down_throughput_and_bpk_collector}, {"monkey-bottom-up", "monkey-top-down"}, throughputPath, bpkPath, _env->throughput_collect_interval);
    //write_collected_throughput({throughput_and_bpk_collector}, {"uniform"}, throughputPath, bpkPath, _env->throughput_collect_interval);
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
  args::ValueFlag<long> level0_file_num_compaction_trigger_cmd(group1, "#files in level0", "The number of files to trigger level-0 compaction. [def: 4`]", {"l0_files", "level0_files_cmpct_trigger"});
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
  args::ValueFlag<std::string> query_wpath_cmd(group4, "wpath", "path for query workload files", {"qwp", "query-wpath"});
  args::ValueFlag<std::string> query_stats_path_cmd(group4, "query_stats_path", "path for dumping query stats", {"qsp", "query-stats-path"});
  args::ValueFlag<int> num_levels_cmd(group1, "L", "The number of levels to fill up with data [def: -1]", {'L', "num_levels"});

  args::Flag print_sst_stat_cmd(group4, "print_sst_stat", "print the stat of SST files", {"ps", "print_sst"});
  args::Flag dump_query_stats_cmd(group4, "dump_query_stats", "print the stats of queries", {"dqs", "dump_query_stats"});

  args::ValueFlag<uint32_t> collect_throughput_interval_cmd(group4, "collect_throughput_interval", "The interval of collecting the overal throughput", {"clct-tputi", "collect-throughput_interval"});
  args::ValueFlag<std::string> throughput_path_cmd(group4, "throughput_path", "path for dumping the collected throughputs when executing the workload", {"tput-op", "throughput-output-path"});
  args::ValueFlag<std::string> bpk_path_cmd(group4, "bpk_path", "path for dumping the collected average bpk when executing the workload", {"bpk-op", "bpk-output-path"});
  

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
  _env->level0_file_num_compaction_trigger = level0_file_num_compaction_trigger_cmd ? args::get(level0_file_num_compaction_trigger_cmd) : 4;
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
  _env->query_wpath = query_wpath_cmd ? args::get(query_wpath_cmd) : workloadPath;
  _env->num_levels = num_levels_cmd ? args::get(num_levels_cmd) : 7;
  _env->print_sst_stat = print_sst_stat_cmd ? true : false;
  _env->dump_query_stats = dump_query_stats_cmd ? true : false;
  _env->dump_query_stats_filename = query_stats_path_cmd ? args::get(query_stats_path_cmd) : query_statsPath;
  _env->throughput_collect_interval = collect_throughput_interval_cmd ? args::get(collect_throughput_interval_cmd) : 0;
  throughputPath = throughput_path_cmd ? args::get(throughput_path_cmd) : "./throughputs.txt";
  bpkPath = bpk_path_cmd ? args::get(bpk_path_cmd) : "./tracked_avg_bpk.txt";
  return 0;
}

void merge_tput_vectors(std::vector<std::pair<double, double>>* origin_tput, std::vector<std::pair<double, double>>* new_tput) {
  if (origin_tput == nullptr || new_tput == nullptr) return;
  if (origin_tput->size() == 0) {
    *origin_tput = *new_tput;
  } else {
    for (int i = 0; i < origin_tput->size() && i < new_tput->size(); i++) {
      origin_tput->at(i).first += new_tput->at(i).first;
      origin_tput->at(i).second += new_tput->at(i).second;
    }
  }
}


