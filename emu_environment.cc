#include "emu_environment.h"




/*Set up the singleton object with the experiment wide options*/
EmuEnv* EmuEnv::instance = 0;


EmuEnv::EmuEnv() 
{
// Options set through command line 
 level0_file_num_compaction_trigger = 1;
  size_ratio = 2;
  buffer_size_in_pages = 128;
  entries_per_page = 128;
  entry_size = 128;                                                    // in Bytes 
  buffer_size = buffer_size_in_pages * entries_per_page * entry_size;  // M = P*B*E = 128 * 128 * 128 B = 2 MB 
  file_to_memtable_size_ratio = 1; // f
  file_size = buffer_size * file_to_memtable_size_ratio;
  verbosity = 0;

  compaction_pri = 1;                 // c | 1:kMinOverlappingRatio, 2:kByCompensatedSize, 3:kOldestLargestSeqFirst, 4:kOldestSmallestSeqFirst
  bits_per_key = 0;                   // b

  experiment_runs = 1;                // run
  clear_sys_page_cache = false;       // cc
  destroy = true;                     // dd
  use_direct_reads = false;           // dr
  use_direct_io_for_flush_and_compaction = false; //dw

  // TableOptions
  no_block_cache = false;                                        // TBC
  block_cache_capacity = 8*1024*1024;				// default 8 MB
  block_cache_high_priority_ratio = 1.0;
  cache_index_and_filter_blocks = true;
  cache_index_and_filter_blocks_with_high_priority = true;      // Deprecated by no_block_cache
  pin_top_level_index_and_filter = true;
  low_pri = false;

  // Other DBOptions
  create_if_missing = true;
  level_compaction_dynamic_level_bytes = true;

  // Flush Options
  allow_write_stall = true;

  path = ""; 
  ingestion_wpath = ""; 
  query_wpath = ""; 

  string experiment_name = ""; 
  string experiment_starting_time = "";

  show_progress=false;
  eval_point_read_statistics_accuracy_interval = 100;
  throughput_collect_interval = 0;
  measure_IOs=false; 
  total_IOs=0;
  
  clean_caches_for_experiments=false;
  print_IOs_per_file=false;
  dump_query_stats=false;
  string dump_query_stats_filename = "dump_query_stats.txt";

  bits_per_key_alloc_type = rocksdb::BitsPerKeyAllocationType::kDefaultBpkAlloc;
  point_reads_track_method = rocksdb::PointReadsTrackMethod::kNaiiveTrack;
}

EmuEnv* EmuEnv::getInstance()
{
  if (instance == 0)
    instance = new EmuEnv();

  return instance;
}
