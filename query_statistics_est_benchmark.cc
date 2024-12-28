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
#include <fstream>
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

std::string num_point_reads_statistics_diff_path = "./num_point_reads_stats_diff.txt";
std::string num_empty_point_reads_statistics_diff_path = "./num_empty_point_reads_stats_diff.txt";
DB* db = nullptr;

int runExperiments(EmuEnv* _env, std::vector<SimilarityResult >* point_reads_statistics_distance_collector);
void prepareExperiment(EmuEnv* _env);
int parse_arguments2(int argc, char *argv[], EmuEnv* _env);
void writePointReadStatsDiff(const std::vector<std::vector<SimilarityResult >* > point_reads_stats_collectors, const std::vector<string> name, uint32_t interval);

int main(int argc, char *argv[]) {
  // check emu_environment.h for the contents of EmuEnv and also the definitions of the singleton experimental environment 
  EmuEnv* _env = EmuEnv::getInstance();
  // parse the command line arguments
  if (parse_arguments2(argc, argv, _env)) {
    exit(1);
  }

  prepareExperiment(_env);

  std::vector<SimilarityResult> point_reads_statistics_distance_naiive_track_collector;
  std::vector<SimilarityResult> point_reads_statistics_distance_dynamic_compaction_aware_track_collector;
  _env->point_reads_track_method = rocksdb::PointReadsTrackMethod::kNaiiveTrack;
  runExperiments(_env, &point_reads_statistics_distance_naiive_track_collector);
  _env->bits_per_key_alloc_type = rocksdb::BitsPerKeyAllocationType::kMnemosynePlusBpkAlloc;
  _env->point_reads_track_method = rocksdb::PointReadsTrackMethod::kDynamicCompactionAwareTrack;
  runExperiments(_env, &point_reads_statistics_distance_dynamic_compaction_aware_track_collector); 
  writePointReadStatsDiff({&point_reads_statistics_distance_naiive_track_collector,
   &point_reads_statistics_distance_dynamic_compaction_aware_track_collector}, {"naiive","dynamic_compaction_aware"}, _env->eval_point_read_statistics_accuracy_interval);
  //writePointReadStatsDiff({
  //  &point_reads_statistics_distance_dynamic_compaction_aware_track_collector}, {"dynamic_compaction_aware"}, _env->eval_point_read_statistics_accuracy_interval);
  return 0;
}

void prepareExperiment(EmuEnv* _env) {
  Options options;
  WriteOptions write_options;
  ReadOptions read_options;
  BlockBasedTableOptions table_options;
  FlushOptions flush_options;
  EnvOptions env_options (options);
 
  WorkloadDescriptor ingestion_wd(_env->ingestion_wpath);
  configOptions(_env, &options, &table_options, &write_options, &read_options, &flush_options);
  loadWorkload(&ingestion_wd);
  options.create_if_missing = true;
  Status destroy_status = DestroyDB(_env->path, options);
  if (!destroy_status.ok()) std::cout << destroy_status.ToString() << std::endl;

  Status s;
  s = DB::Open(options, _env->path, &db);
  if (!s.ok()) std::cerr << s.ToString() << std::endl;

  // Prepare Perf and I/O stats
  QueryTracker *query_track = new QueryTracker();   // stats tracker for each run
  runWorkload(db, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &ingestion_wd, query_track);

  s = CloseDB(db, flush_options);
  assert(s.ok());
  delete query_track;   
}

// Run rocksdb experiments for experiment_runs
// 1.Initiate experiments environment and rocksDB options
// 2.Preload workload into memory
// 3.Run workload and collect stas for each run
int runExperiments(EmuEnv* _env, std::vector<SimilarityResult >* point_reads_statistics_distance_collector) {
 
  Options options;
  WriteOptions write_options;
  ReadOptions read_options;
  BlockBasedTableOptions table_options;
  FlushOptions flush_options;
 
  WorkloadDescriptor query_wd(_env->query_wpath);
 
  //WorkloadDescriptor wd(workloadPath);
  // init RocksDB configurations and experiment settings
  configOptions(_env, &options, &table_options, &write_options, &read_options, &flush_options);
   EnvOptions env_options (options);
  // parsing workload
  loadWorkload(&query_wd);


  
  // Starting experiments
  assert(_env->experiment_runs >= 1);
  std::vector<SimilarityResult > temp_point_reads_statistics_distance_collector;
  std::string copy_db_cmd = "mkdir -p " +  _env->path + "-to-be-eval && rm " + _env->path + "-to-be-eval/* && cp " + _env->path + "/* " + _env->path + "-to-be-eval/";
  for (int i = 0; i < _env->experiment_runs; ++i) {
    system(copy_db_cmd.c_str());
    // Reopen DB
    Status s;
    s = DB::Open(options, _env->path + "-to-be-eval", &db);
    if (!s.ok()) std::cerr << s.ToString() << std::endl;

    // Prepare Perf and I/O stats
    QueryTracker *query_track = new QueryTracker();   // stats tracker for each run
    
    runWorkload(db, _env, &options, &table_options, &write_options, &read_options, &flush_options, &env_options, &query_wd, query_track, nullptr, &temp_point_reads_statistics_distance_collector);
    if (point_reads_statistics_distance_collector->empty()) {
        *point_reads_statistics_distance_collector = temp_point_reads_statistics_distance_collector;
    } else {
        for (size_t j = 0; j < point_reads_statistics_distance_collector->size(); j++) {
            point_reads_statistics_distance_collector->at(j).add(temp_point_reads_statistics_distance_collector[j]);
        }
        temp_point_reads_statistics_distance_collector.clear();
    }

    s = CloseDB(db, flush_options);
    assert(s.ok());
    delete query_track;

    std::cout << "End of experiment run: " << i+1 << std::endl;
    std::cout << std::endl;
  }

  if (!point_reads_statistics_distance_collector->empty()) {
    for (size_t j = 0; j < point_reads_statistics_distance_collector->size(); j++) {
      point_reads_statistics_distance_collector->at(j).dividedBy(_env->experiment_runs);
    }
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
  args::ValueFlag<long> level0_file_num_compaction_trigger_cmd(group1, "#files in level0", "The number of files to trigger level-0 compaction. [def: 4]", {"l0_files", "level0_files_cmpct_trigger"});
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
  args::Flag direct_reads_cmd(group4, "use_direct_reads", "Use direct reads", {"dr", "use_direct_reads"});
  args::Flag direct_writes_cmd(group4, "use_direct_writes", "Use direct writes", {"dw", "use_direct_writes"});
  args::Flag low_pri_cmd(group4, "low_pri", "write request is of lower priority if compaction is behind", {"lp", "low_priority"});
  args::ValueFlag<int> verbosity_cmd(group4, "verbosity", "The verbosity level of execution [0,1,2; def: 0]", {'V', "verbosity"});

  args::ValueFlag<std::string> path_cmd(group4, "path", "path for writing the DB and all the metadata files", {'p', "path"});
  args::ValueFlag<std::string> ingestion_wpath_cmd(group4, "wpath", "path for ingestion workload files", {"iwp", "ingestion-wpath"});
  args::ValueFlag<std::string> query_wpath_cmd(group4, "wpath", "path for query workload files", {"qwp", "query-wpath"});
  args::ValueFlag<std::string> query_stats_path_cmd(group4, "query_stats_path", "path for dumping query stats", {"qsp", "query-stats-path"});
  args::ValueFlag<int> num_levels_cmd(group1, "L", "The number of levels to fill up with data [def: -1]", {'L', "num_levels"});

  args::ValueFlag<std::string> num_point_reads_stats_diff_path_cmd(group4, "num_point_reads_stats_diff_path", "path for dumping the difference of the estimated number of point reads for different tracking method", {"qrsdp", "point-reads-stats-diff-path"});
  args::ValueFlag<std::string> num_empty_point_reads_stats_diff_path_cmd(group4, "num_empty_point_reads_stats_diff_path", "path for dumping the difference of the estimated number of the empty point reads for different tracking method", {"eqrsdp", "empty-point-reads-stats-diff-path"});

  args::Flag print_sst_stat_cmd(group4, "print_sst_stat", "print the stat of SST files", {"ps", "print_sst"});
  args::Flag dump_query_stats_cmd(group4, "dump_query_stats", "print the stats of queries", {"dqs", "dump_query_stats"});

  args::ValueFlag<uint32_t> eval_point_read_statistics_accuracy_interval_cmd(group4, "eval_point_read_interval", "The interval of insert operations that compares the statistics of point reads", {"epri", "eval_point_read_interval"});


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
  _env->use_direct_reads = direct_reads_cmd ? true : false;
  _env->use_direct_io_for_flush_and_compaction = direct_writes_cmd ? true : false;
  _env->path = path_cmd ? args::get(path_cmd) : kDBPath;
  _env->ingestion_wpath = ingestion_wpath_cmd ? args::get(ingestion_wpath_cmd) : ingestion_workloadPath;
  _env->query_wpath = query_wpath_cmd ? args::get(query_wpath_cmd) : query_workloadPath;
  _env->num_levels = num_levels_cmd ? args::get(num_levels_cmd) : 7;
  _env->print_sst_stat = print_sst_stat_cmd ? true : false;
  _env->dump_query_stats = dump_query_stats_cmd ? true : false;
  _env->dump_query_stats_filename = query_stats_path_cmd ? args::get(query_stats_path_cmd) : query_statsPath;
  _env->eval_point_read_statistics_accuracy_interval = eval_point_read_statistics_accuracy_interval_cmd ? args::get(eval_point_read_statistics_accuracy_interval_cmd) : 100;
  assert(_env->eval_point_read_statistics_accuracy_interval > 0);

  num_point_reads_statistics_diff_path = num_point_reads_stats_diff_path_cmd ? args::get(num_point_reads_stats_diff_path_cmd) : num_point_reads_statistics_diff_path;
  num_empty_point_reads_statistics_diff_path = num_empty_point_reads_stats_diff_path_cmd ? args::get(num_empty_point_reads_stats_diff_path_cmd) : num_empty_point_reads_statistics_diff_path;
  return 0;
}

void writePointReadStatsDiff(std::vector<std::vector<SimilarityResult >* > point_reads_stats_collectors, std::vector<string> names, uint32_t interval) {
    for (size_t i = 0; i < point_reads_stats_collectors.size(); i++) {
        assert(point_reads_stats_collectors[i]);
        assert(point_reads_stats_collectors[i]->size() > 0);
    }
    ofstream num_point_reads_stats_diff_ofs(num_point_reads_statistics_diff_path.c_str());
    num_point_reads_stats_diff_ofs << "ops";
    for(const string& name:names) {
        num_point_reads_stats_diff_ofs << ",euclidean-distance-" << name;
    }
    for(const string& name:names) {
        num_point_reads_stats_diff_ofs << ",cosine-similarity-" << name;
    }
    for(const string& name:names) {
        num_point_reads_stats_diff_ofs << ",avg-leveled-cosine-similarity-" << name;
    }
    int num_actual_levels = point_reads_stats_collectors[0]->back().leveled_cosine_similarity.size();
    for (size_t l = 0; l < num_actual_levels; l++) {
      for(const string& name:names) {
        num_point_reads_stats_diff_ofs << ",Lvl" << l << "-leveled-cosine-similarity-" << name;
      }
    }
    for (size_t l = 0; l < num_actual_levels; l++) {
      for(const string& name:names) {
        num_point_reads_stats_diff_ofs << ",Lvl" << l << "-leveled-euclidean-distance-" << name;
      }
    }
    
    num_point_reads_stats_diff_ofs << std::endl;
    int length = point_reads_stats_collectors[0]->size();
    for (size_t i = 0; i < length; i++) {
        num_point_reads_stats_diff_ofs << (i+1)*interval;
        for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            num_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).euclidean_distance.first;
        }
        for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            num_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).cosine_similarity.first;
        }
        for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            num_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).getAvgCosineSimilarityForNumPointReads();
        }
	for (size_t l = 0; l < num_actual_levels; l++) {
          for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            if (point_reads_stats_collectors[j]->at(i).leveled_cosine_similarity.size() <= l) {
              num_point_reads_stats_diff_ofs << ",0.0";
            } else {
              num_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).leveled_cosine_similarity[l].first;
            }
            
          }
	}
        for (size_t l = 0; l < num_actual_levels; l++) {
          for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            if (point_reads_stats_collectors[j]->at(i).leveled_euclidean_distance.size() <= l) {
              num_point_reads_stats_diff_ofs << ",0.0";
            } else {
              num_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).leveled_euclidean_distance[l].first;
            }
            
          }
	}
        num_point_reads_stats_diff_ofs << std::endl;
    }
    num_point_reads_stats_diff_ofs.close();

    std::cout << "Finished writing the tracked statistics difference for the number of point reads." << std::endl;

    ofstream num_empty_point_reads_stats_diff_ofs(num_empty_point_reads_statistics_diff_path.c_str());
    num_empty_point_reads_stats_diff_ofs << "ops";
    for(const string& name:names) {
        num_empty_point_reads_stats_diff_ofs << ",euclidean-distance-" << name;
    }
    for(const string& name:names) {
        num_empty_point_reads_stats_diff_ofs << ",cosine-similarity-" << name;
    }
    for(const string& name:names) {
        num_empty_point_reads_stats_diff_ofs << ",avg-leveled-cosine-similarity-" << name;
    }
    for (size_t l = 0; l < num_actual_levels; l++) {
      for(const string& name:names) {
        num_empty_point_reads_stats_diff_ofs << ",Lvl" << l << "-leveled-cosine-similarity-" << name;
      }
    }
    for (size_t l = 0; l < num_actual_levels; l++) {
      for(const string& name:names) {
        num_empty_point_reads_stats_diff_ofs << ",Lvl" << l << "-leveled-euclidean-distance-" << name;
      }
    }
    num_empty_point_reads_stats_diff_ofs << std::endl;
    for (size_t i = 0; i < length; i++) {
        num_empty_point_reads_stats_diff_ofs << (i+1)*interval;
        for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            num_empty_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).euclidean_distance.second;
        }
        for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            num_empty_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).cosine_similarity.second;
        }
        for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            num_empty_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).getAvgCosineSimilarityForNumExistingPointReads();
        }
	for (size_t l = 0; l < num_actual_levels; l++) {
          for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            if (point_reads_stats_collectors[j]->at(i).leveled_euclidean_distance.size() <= l) {
              num_empty_point_reads_stats_diff_ofs << ",0.0";
	    } else {
              num_empty_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).leveled_cosine_similarity[l].second;
	    }
          }
	}
        for (size_t l = 0; l < num_actual_levels; l++) {
          for (size_t j = 0; j < point_reads_stats_collectors.size(); j++) {
            if (point_reads_stats_collectors[j]->at(i).leveled_euclidean_distance.size() <= l) {
              num_empty_point_reads_stats_diff_ofs << ",0.0";
            } else {
              num_empty_point_reads_stats_diff_ofs << "," << point_reads_stats_collectors[j]->at(i).leveled_euclidean_distance[l].second;
            }
            
          }
	}
        num_empty_point_reads_stats_diff_ofs << std::endl;
    }
    num_empty_point_reads_stats_diff_ofs.close();

    std::cout << "Finished writing the tracked statistics difference for the number of empty point reads." << std::endl;
}
