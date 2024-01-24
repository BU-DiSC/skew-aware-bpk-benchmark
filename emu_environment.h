/*
 *  Created on: May 13, 2019
 *  Author: Subhadeep
 */

#ifndef EMU_ENVIRONMENT_H_
#define EMU_ENVIRONMENT_H_

#include <iostream>
#include "rocksdb/table.h"

using namespace std;


class EmuEnv
{
private:
  EmuEnv(); 
  static EmuEnv *instance;

public:
  static EmuEnv* getInstance();

// Options set through command line 
  double size_ratio;                // T | used to set op->max_bytes_for_level_multiplier
  int buffer_size_in_pages;         // P
  int entries_per_page;             // B
  int entry_size;                   // E
  size_t buffer_size;               // M = P*B*E ; in Bytes
  int file_to_memtable_size_ratio;  // f
  uint64_t file_size;               // F
  int verbosity;                    // V

  // adding new parameters with Guanting
  uint16_t compaction_pri;                // C | 1:kMinOverlappingRatio, 2:kByCompensatedSize, 3:kOldestLargestSeqFirst, 4:kOldestSmallestSeqFirst
  double bits_per_key;                    // b
  uint16_t experiment_runs;               // R
  bool clear_sys_page_cache;              // cc | clear system page cache before experiment
  bool destroy;                           // dd | destroy db before experiments
  bool use_direct_reads;                  // dr
  bool use_direct_io_for_flush_and_compaction; // dw
  bool low_pri;                           // lp. If true, this write request is of lower priority if compaction is behind.

  uint32_t level0_file_num_compaction_trigger;
  int num_levels;                               // Maximum number of levels that a tree may have [RDB_default: 7]

  // TableOptions
  bool no_block_cache;    // TBC
  int block_cache_capacity; 
  double block_cache_high_priority_ratio; 
  bool cache_index_and_filter_blocks;
  bool cache_index_and_filter_blocks_with_high_priority;      // Deprecated by no_block_cache

  bool pin_top_level_index_and_filter;                        // TBC


  // Other DBOptions
  bool create_if_missing = true;
  bool allow_write_stall;
  bool level_compaction_dynamic_level_bytes = true;
  rocksdb::BitsPerKeyAllocationType bits_per_key_alloc_type = rocksdb::BitsPerKeyAllocationType::kDefaultBpkAlloc;

  // database path 
  string path;
  // workload path
  string ingestion_wpath;
  string query_wpath;
  string wpath;
  
  bool show_progress;

  bool measure_IOs;
  bool print_IOs_per_file;
  long total_IOs;
  bool clean_caches_for_experiments;
 
  bool print_sst_stat;              // ps | print sst stats
  bool dump_query_stats;
  string dump_query_stats_filename;
};

#endif /*EMU_ENVIRONMENT_H_*/

