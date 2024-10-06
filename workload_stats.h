#ifndef WORKLOAD_STATS_H_
#define WORKLOAD_STATS_H_

#include <cstdint>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <utility>
#include <unordered_map>
#include <assert.h>

enum QueryType : char {
  INSERT = 'I',
  UPDATE = 'U',
  DELETE = 'D',
  LOOKUP = 'Q',
  RANGE_DELETE = 'R',
  RANGE_LOOKUP = 'S',
  NONE = 0x00
};
// 4 Bytes per point lookup/delete entry
struct BaseEntry{
  //uint32_t key;
  std::string key;
  //BaseEntry() : key(0) {}
  BaseEntry() : key("") {}
  virtual ~BaseEntry() = default;
  //explicit BaseEntry(uint32_t k) : key(k){}
  explicit BaseEntry(std::string k) : key(k){}
};
// 4 + value size per insert/update query
struct Entry : public BaseEntry{
  std::string value;
  Entry() : BaseEntry(), value("") {}
  //explicit Entry(uint32_t k) : BaseEntry(k), value("") {}
  explicit Entry(std::string k) : BaseEntry(k), value("") {}
  //explicit Entry(uint32_t k,  std::string v) : BaseEntry(k), value(v) {}
  explicit Entry(std::string k,  std::string v) : BaseEntry(k), value(v) {}
};
// 8 Bytes per range query;
struct RangeEntry : public BaseEntry {
  //uint32_t range;
  std::string end_key;
  RangeEntry() : BaseEntry(), end_key("") {}
/*
  explicit RangeEntry(uint32_t start_key, uint32_t diff)
   : BaseEntry(start_key), range(diff) {
    assert(range >= 0 && "Key range must >= 0");
  }
*/
  explicit RangeEntry(std::string start_key, std::string _end_key)
   : BaseEntry(start_key), end_key(_end_key) {
    //assert(range >= 0 && "Key range must >= 0");
  }
};

// Store descriptions and apointer to an query entry for efficient use 
// Maximal overhead: 17 Bytes per query;
struct QueryDescriptor {
  uint64_t seq;   // >0 && =index + 1;
  QueryType type;
  BaseEntry *entry_ptr;
  QueryDescriptor() : seq(0), type(NONE), entry_ptr(nullptr) {}
  explicit QueryDescriptor(uint64_t seq_, QueryType ktype, BaseEntry *entry) 
   : seq(seq_), type(ktype), entry_ptr(entry) {}
};

// Store in-memory workload and relative stats 
struct WorkloadDescriptor {
  std::string path_;       // workload path
  uint64_t total_num = 0;
  uint64_t actual_total_num = 0;	
  uint64_t insert_num = 0;
  uint64_t actual_insert_num = 0;		// for pseudo zero result point lookup
  uint64_t update_num = 0;
  uint64_t plookup_num = 0;
  uint64_t rlookup_num = 0;
  uint64_t pdelete_num = 0;
  uint64_t rdelete_num = 0;
  // double plookup_hit_rate = 0.2;		// percentage of zero result point lookup
  std::vector<QueryDescriptor> queries;
  WorkloadDescriptor() : path_("workload.txt") {}
  explicit WorkloadDescriptor(std::string path) : path_(path) {}
};

struct DbStats {
  double bits_per_key = 0;
  uint32_t fst_level_with_entries = 0;
  uint32_t num_levels = 0;
  uint32_t num_entries = 0;
  uint32_t num_files = 0;
  uint64_t num_total_empty_queries = 0;
  std::vector<uint64_t> level2entries;
  std::vector<uint64_t> entries_in_level0;
  std::unordered_map<uint64_t, uint64_t> fileID2empty_queries;
  std::unordered_map<uint64_t, uint64_t> fileID2queries;
  std::unordered_map<uint64_t, uint64_t> fileID2entries;
  std::vector<std::pair<size_t, std::unordered_map<uint64_t, uint64_t>>> leveled_fileID2queries;
  std::vector<std::pair<size_t, std::unordered_map<uint64_t, uint64_t>>> leveled_fileID2empty_queries;
};

struct SimilarityResult {
  std::pair<double, double> euclidean_distance;
  std::pair<double, double> cosine_similarity;
  std::vector<std::pair<double, double>> leveled_cosine_similarity;
  std::vector<std::pair<double, double>> leveled_euclidean_distance;
  SimilarityResult(std::pair<double, double> _euclidean_distance,
  		std::pair<double, double> _cosine_similarity,
		std::vector<std::pair<double, double>> _leveled_euclidean_distance,
                std::vector<std::pair<double, double>> _leveled_cosine_similarity) {
    euclidean_distance = _euclidean_distance;
    cosine_similarity = _cosine_similarity;
    leveled_euclidean_distance = _leveled_euclidean_distance;
    leveled_cosine_similarity = _leveled_cosine_similarity;
  }
  void add(const SimilarityResult& other) {
    euclidean_distance.first += other.euclidean_distance.first;
    euclidean_distance.second += other.euclidean_distance.second;
    cosine_similarity.first += other.cosine_similarity.first;
    cosine_similarity.second += other.cosine_similarity.second;
    for (size_t i = 0; i < std::min(leveled_cosine_similarity.size(), other.leveled_cosine_similarity.size()); i++) {
      leveled_cosine_similarity[i].first += other.leveled_cosine_similarity[i].first;
      leveled_cosine_similarity[i].second += other.leveled_cosine_similarity[i].second;
    }
    for (size_t i = 0; i < std::min(leveled_euclidean_distance.size(), other.leveled_euclidean_distance.size()); i++) {
      leveled_euclidean_distance[i].first += other.leveled_euclidean_distance[i].first;
      leveled_euclidean_distance[i].second += other.leveled_euclidean_distance[i].second;
    }
  }
  void dividedBy(double divisor) {
    if (divisor == 0) return;
    euclidean_distance.first /= divisor;
    euclidean_distance.second /= divisor;
    cosine_similarity.first /= divisor;
    cosine_similarity.second /= divisor;
    for (size_t i = 0; i < leveled_cosine_similarity.size(); i++) {
      leveled_cosine_similarity[i].first /= divisor;
      leveled_cosine_similarity[i].second /= divisor;
    }
    for (size_t i = 0; i < leveled_euclidean_distance.size(); i++) {
      leveled_euclidean_distance[i].first /= divisor;
      leveled_euclidean_distance[i].second /= divisor;
    }
  }
  double getAvgCosineSimilarityForNumPointReads() {
    double result = 0.0;
    for (size_t i = 0; i < leveled_cosine_similarity.size(); i++) {
	    result += leveled_cosine_similarity[i].first;
    }
    if (leveled_cosine_similarity.size() > 0) {
	    result /= leveled_cosine_similarity.size();
    }
    return result;
  }

  double getAvgCosineSimilarityForNumExistingPointReads() {
    double result = 0.0;
    for (size_t i = 0; i < leveled_cosine_similarity.size(); i++) {
    	result += leveled_cosine_similarity[i].second;
    }
    if (leveled_cosine_similarity.size() > 0) {
	    result /= leveled_cosine_similarity.size();
    }
    return result;
  }
};

// using cosine similarity
std::pair<double, double> ComputePointQueriesStatisticsByCosineSimilarity(DbStats& stats1, DbStats& stats2);
// using level-wise euclidean distance (i.e., calculate euclidean distance per level)
std::vector<std::pair<double, double>> ComputePointQueriesStatisticsByLevelwiseDistanceType(DbStats& stats1, DbStats& stats2, int distance_type);
// using Euclidean distance
std::pair<double, double> ComputePointQueriesStatisticsByEuclideanDistance(DbStats& stats1, DbStats& stats2);

// Keep track of all performance metrics during queries execution
struct QueryTracker {
  uint64_t total_completed = 0;
  uint64_t inserts_completed = 0;
  uint64_t updates_completed = 0;
  uint64_t point_deletes_completed = 0;
  uint64_t range_deletes_completed = 0;
  uint64_t point_lookups_completed = 0;
  uint64_t zero_point_lookups_completed = 0;
  uint64_t range_lookups_completed = 0;

  // Cumulative latency cost
  uint64_t inserts_cost = 0;
  uint64_t updates_cost = 0;
  uint64_t point_deletes_cost = 0;
  uint64_t range_deletes_cost = 0;
  uint64_t point_lookups_cost = 0;
  uint64_t zero_point_lookups_cost = 0;
  uint64_t range_lookups_cost = 0;
  uint64_t workload_exec_time = 0;
  uint64_t experiment_exec_time = 0;

  // Lookup related, bloom, IO
  uint64_t bloom_memtable_hit_count = 0;    // retrieve filter block from memory and key exists (could be false positive)
  uint64_t bloom_memtable_miss_count = 0;   // retrieve filter block from memory and key not exist
  uint64_t bloom_sst_true_positive_count = 0; // retrieve filter block from sst and key exists
  uint64_t bloom_sst_hit_count = 0;         // retrieve filter block from sst(I/O) and key exists (could be false positive)
  uint64_t bloom_sst_miss_count = 0;        // retrieve filter block from sst(I/O) and key not exist
  uint64_t get_from_memtable_time = 0;          // total nanos spent on querying memtables
  uint64_t get_from_memtable_count = 0;         // number of mem tables queried
  uint64_t get_from_output_files_time = 0;      // total nanos reading from output files
  uint64_t filter_block_read_count = 0;         // total number of filter block reads
  uint64_t index_block_read_count = 0;         // total number of index block reads
  uint64_t data_block_read_count = 0; // total number of data block reads
  uint64_t data_block_cached_count = 0; // total number of data blocks added to cache
  uint64_t block_cache_index_hit_count = 0;         
  uint64_t block_cache_filter_hit_count = 0;         
  uint64_t bytes_read = 0;                      // total bytes by IO read
  uint64_t read_nanos = 0;                      // total time by IO read
  uint64_t bytes_written = 0;                   // total bytes by IO write
  uint64_t write_nanos = 0;                     // total time by IO wirte
  uint64_t cpu_write_nanos = 0;
  uint64_t cpu_read_nanos = 0;
  uint64_t get_cpu_nanos = 0;

  uint64_t read_table_mem = 0;			// total bytes by index and filter size in memory during reading
  uint64_t block_cache_usage = 0;

  // Compaction cost
  double read_amp = 0;
  double write_amp = 0;
  double space_amp = 0;
  uint64_t stalls = 0;

  double ucpu_pct = 0;
  double scpu_pct = 0;
};

// Preload workload into memory,
// which is stored in a WorkloadDescriptor
void loadWorkload(WorkloadDescriptor *wd);

// Dump stats from a single track into a cumulative sample
// to compute cumulative and average result
void dumpStats(QueryTracker *sample, const QueryTracker *single);

#endif /*WORKLOAD_STATS_H_*/
