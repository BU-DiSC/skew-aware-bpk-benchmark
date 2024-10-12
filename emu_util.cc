#include "emu_util.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <queue>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include "file/filename.h"
#include "rocksdb/env.h"
#include "rocksdb/db.h"
#include "rocksdb/table.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/cache.h"
#include "rocksdb/iostats_context.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/sst_file_reader.h"
#include "rocksdb/sst_file_writer.h"
#include "rocksdb/statistics.h"
#include "stdlib.h"
#include "stdio.h"
#include "sys/times.h"
#include "sys/vtimes.h"

#define WAIT_INTERVAL 100

Status ReopenDB(DB *&db, const Options &op, const FlushOptions &flush_op) {
	const std::string dbPath = db->GetName();
	Status s = CloseDB(db, flush_op);
	assert(s.ok());
	Status return_status = DB::Open(op, dbPath, &db);
	std::cout << return_status.ToString() << std::endl;
	assert(return_status.ok());
	return return_status;
}

Status BackgroundJobMayAllCompelte(DB *&db) {
  Status return_status = Status::Incomplete();
  if (FlushMemTableMayAllComplete(db)) {
    return_status = Status::OK();
  }

	if ((db->GetOptions().disable_auto_compactions == true || 
     (db->GetOptions().disable_auto_compactions == false && CompactionMayAllComplete(db)))) {
		return_status = Status::OK();
	}
 
  return return_status;
}

Status CloseDB(DB *&db, const FlushOptions &flush_op) {
	Status s = db->DropColumnFamily(db->DefaultColumnFamily());
	
	s = db->Flush(flush_op);
	assert(s.ok());
	
  s = BackgroundJobMayAllCompelte(db);
  assert(s.ok());

	s = db->Close();
	assert(s.ok());
	delete db;
	db = nullptr;
	return s;
}

// Not working, will trigger SegmentFault
// Wait until the compaction completes
// This function actually does not necessarily
// wait for compact. It actually waits for scheduled compaction
// OR flush to finish.
// Status WaitForCompaction(DB *db, bool waitUnscheduled) {
// 	return (static_cast_with_check<DBImpl, DB>(db->GetRootDB()))
//       		->WaitForCompactAPI(waitUnscheduled);
// }

// Need to select timeout carefully
// Completion not guaranteed
bool CompactionMayAllComplete(DB *db) {
	uint64_t pending_compact;
	uint64_t pending_compact_bytes; 
	uint64_t running_compact;
	bool success = db->GetIntProperty("rocksdb.compaction-pending", &pending_compact)
	 						 && db->GetIntProperty("rocksdb.estimate-pending-compaction-bytes", &pending_compact_bytes)
							 && db->GetIntProperty("rocksdb.num-running-compactions", &running_compact);
  do {
    sleep_for_ms(WAIT_INTERVAL);
    success = db->GetIntProperty("rocksdb.compaction-pending", &pending_compact)
	 						 && db->GetIntProperty("rocksdb.estimate-pending-compaction-bytes", &pending_compact_bytes)
							 && db->GetIntProperty("rocksdb.num-running-compactions", &running_compact);

  } while (pending_compact || pending_compact_bytes || running_compact || !success);
	return success;
}

// Need to select timeout carefully
// Completion not guaranteed
bool FlushMemTableMayAllComplete(DB *db) {
	uint64_t pending_flush;
	uint64_t running_flush;
	bool success = db->GetIntProperty("rocksdb.mem-table-flush-pending", &pending_flush)
							 && db->GetIntProperty("rocksdb.num-running-flushes", &running_flush);
  do {
    sleep_for_ms(WAIT_INTERVAL);
    success = db->GetIntProperty("rocksdb.mem-table-flush-pending", &pending_flush)
							 && db->GetIntProperty("rocksdb.num-running-flushes", &running_flush);
  } while (pending_flush || running_flush || !success);
	return ((static_cast_with_check<DBImpl, DB>(db->GetRootDB()))
      		->WaitForFlushMemTable(static_cast<ColumnFamilyHandleImpl*>(db->DefaultColumnFamily())->cfd())) == Status::OK();
}

void resetPointReadsStats(DB* db) {
  auto cfd = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(db->DefaultColumnFamily())->cfd();
  const auto* vstorage = cfd->current()->storage_info();
  for (int i = 0; i < cfd->NumberLevels(); i++) {
    std::vector<FileMetaData*> level_files = vstorage->LevelFiles(i);
    for (uint32_t j = 0; j < level_files.size(); j++) {
      level_files[j]->stats.num_point_reads.store(0);
      level_files[j]->stats.num_existing_point_reads.store(0);
      level_files[j]->stats.start_global_point_read_number = 0;
      level_files[j]->stats.global_point_read_number_window = std::queue<uint64_t>();
      level_files[j]->stats.point_read_result_in_window = 0;
    }
  }
}

double getCurrentAverageBitsPerKey(DB* db, const Options *op) {
  auto cfd = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(db->DefaultColumnFamily())->cfd();
  const auto* vstorage = cfd->current()->storage_info();

  uint64_t agg_BF_size = vstorage->GetCurrentTotalFilterSize();
  uint64_t agg_num_entries_in_BF = vstorage->GetCurrentTotalNumEntries();
  
  if (agg_num_entries_in_BF == 0) return 0.0;
  return (double) agg_BF_size*8.0/agg_num_entries_in_BF;
}

void collectDbStats(DB* db, DbStats *stats, bool print_point_read_stats, uint64_t start_global_point_read_number, double learning_rate, bool estimate_flag) {
  auto cfd = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(db->DefaultColumnFamily())->cfd();
  const auto* vstorage = cfd->current()->storage_info();
  
  uint64_t num_point_reads;
  uint64_t num_existing_point_reads;
  // Aggregate statistics
  bool fst_meet_entries = false;
  stats->level2entries = std::vector<uint64_t> (cfd->NumberLevels(), 0);
  for (int i = 0; i < cfd->NumberLevels(); i++) {
    stats->num_levels++;
    stats->leveled_fileID2queries.emplace_back(i, std::unordered_map<uint64_t, uint64_t>());
    stats->leveled_fileID2empty_queries.emplace_back(i, std::unordered_map<uint64_t, uint64_t>());
    if (vstorage->LevelFiles(i).empty()){
      if(!fst_meet_entries) stats->fst_level_with_entries++;
      continue;
    }
    fst_meet_entries = true;
    //if (i == 0) continue;
    
    std::vector<FileMetaData*> level_files = vstorage->LevelFiles(i);
    FileMetaData* level_file;
    stats->num_files += level_files.size();
    if (i == 0) {
      stats->entries_in_level0 = std::vector<uint64_t> (level_files.size(), 0);
    }
    for (uint32_t j = 0; j < level_files.size(); j++) {
      level_file = level_files[j];
      stats->level2entries[i] += level_file->num_entries - level_file->num_range_deletions;
      uint64_t min_num_point_reads = 0;
      if (i == 0) {
        stats->entries_in_level0[j] = level_file->num_entries - level_file->num_range_deletions;
	min_num_point_reads = round(level_file->stats.start_global_point_read_number*vstorage->GetAvgNumPointReadsPerLvl0File());
      }
      stats->num_entries += level_file->num_entries - level_file->num_range_deletions;
      if (estimate_flag) {
         std::pair<uint64_t, uint64_t> result = level_file->stats.GetEstimatedNumPointReads(start_global_point_read_number, learning_rate, -1, min_num_point_reads);
         num_point_reads = result.first;
         num_existing_point_reads = result.second;
      } else {
         num_point_reads = level_file->stats.GetNumPointReads();
         num_existing_point_reads = level_file->stats.GetNumExistingPointReads();
      }
      uint64_t filenumber = level_file->fd.GetNumber();
      if ( print_point_read_stats ){
        std::string filename = MakeTableFileName(filenumber);
        std::cout << filename << "(" << num_point_reads << ", " << num_existing_point_reads << ") " << std::endl;
      }
      stats->fileID2entries.emplace(filenumber, level_file->num_entries - level_file->num_range_deletions);
      stats->fileID2empty_queries.emplace(filenumber, num_point_reads  - num_existing_point_reads);
      //stats->fileID2empty_queries.second.emplace(filenumber, num_existing_point_reads);
      stats->fileID2queries.emplace(filenumber, num_point_reads);
      stats->leveled_fileID2empty_queries.back().second.emplace(filenumber, num_point_reads  - num_existing_point_reads);
      //stats->leveled_fileID2empty_queries.back().second.emplace(filenumber, num_existing_point_reads);
      stats->leveled_fileID2queries.back().second.emplace(filenumber, num_point_reads);
      stats->num_total_empty_queries += num_point_reads  - num_existing_point_reads;
    }
  }
}

Status createNewSstFile(const std::string filename_to_read, const std::string filename_to_write, const Options *op,
    EnvOptions *env_op, const ReadOptions *read_op) {
  SstFileReader reader (*op);
  SstFileWriter writer (*env_op, *op);
  Status s = reader.Open(filename_to_read);
  if (!s.ok()) return s;
  s = writer.Open(filename_to_write);
  if (!s.ok()) return s;
  Iterator* it = reader.NewIterator(*read_op);
  uint32_t entries = 0;
  it->SeekToFirst();
  while (it->Valid()) {
    s = writer.Put(it->key(), it->value());
    entries++;
    if (!s.ok()) std::cout << s.ToString() << std::endl;
    it->Next();
  }
  //std::cout << " inserted " << entries << " entries" << std::endl;
  delete it;
  return writer.Finish();
}

Status createDbWithMonkey(const EmuEnv* _env, DB* db, DB* db_monkey, Options *op, BlockBasedTableOptions *table_op, const WriteOptions *write_op,
  ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions *env_op, const DbStats & db_stats) {
  
  std::vector<double> bpk_list (_env->num_levels, _env->bits_per_key);
    getNaiveMonkeyBitsPerKey(db_stats.num_entries, floor(_env->buffer_size/_env->entry_size), _env->size_ratio, 
            _env->level0_file_num_compaction_trigger, _env->bits_per_key, &bpk_list, _env->level_compaction_dynamic_level_bytes, true);

  // if (db_stats.num_levels <= 1) return Status::OK(); // do nothing when L <= 1
  // // plain monkey does not optimizes for level 0 because the size ratio is specified between MemTable and Level 1
  // uint64_t num_entries = db_stats.num_entries;
  // uint64_t num_entries_in_level0 = 0;
  // for (uint32_t i = 0; i < db_stats.entries_in_level0.size(); i++) {
  //   num_entries_in_level0 += db_stats.entries_in_level0[i];
  // }
  // if (num_entries <= num_entries_in_level0) return Status::OK();
  // uint32_t L = db_stats.num_levels;
  // if (num_entries_in_level0 > 0) {
  //   L--;
  //   if (L <= 1) {
  //     return Status::OK();
  //   }
  // }
  // num_entries -= num_entries_in_level0;

  const double log_2_squared = std::pow(std::log(2), 2);
  // uint64_t total_filter_memory = num_entries * _env->bits_per_key;
  // std:: cout << "Total memory: " << total_filter_memory << std::endl;
  // double T = _env->size_ratio;
  // double X = std::log(T)/(log_2_squared*(T - 1));
  // uint32_t Y = 0;
  // if (X >= T*_env->bits_per_key) {
  //   Y =  (uint32_t) floor(std::log(X/_env->bits_per_key)/std::log(T));
  // }
  // if (Y >= L) return Status::OK();
  // double R = std::exp(-_env->bits_per_key*log_2_squared*std::pow(T, Y)) * std::pow(T, T/(T-1))/(T-1) + Y;
  
  // std::vector<double> bpk_by_level = std::vector<double> (L+1, 0.0);
  // double base = (R - Y)*(T - 1)/T;
  // double scale_ratio = 1.0;
  // double actual_memory = 0.0;
  // for (uint64_t i = 1; i <= L - Y ; i++) {
  //   bpk_by_level[i] =  ((L - Y - i)*std::log(T) - std::log(base))/log_2_squared;
  //   actual_memory += bpk_by_level[i]*db_stats.level2entries[i];
  // }
  // scale_ratio = total_filter_memory*1.0/actual_memory;
  // for (uint64_t i = 1; i <= L - Y ; i++) {
  //   bpk_by_level[i] *= scale_ratio;
  // }
  DBImpl::GetImplOptions get_impl_options;
  get_impl_options.column_family = db->DefaultColumnFamily();
  auto cfh = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(get_impl_options.column_family);
  auto cfd = cfh->cfd();
  const auto* vstorage = cfd->current()->storage_info();

  uint32_t level_counter = 0;
  IngestExternalFileOptions ingest_opts;
  ingest_opts.move_files = true;
  std::vector<std::string> filenames;
  uint64_t used_memory = 0;
  double objective_value = 0.0;

  std::string create_temp_dir_cmd = "mkdir -p " + _env->path + "-temp/";
  system(create_temp_dir_cmd.c_str());
  for (int i = 0; i < cfd->NumberLevels(); i++) {
      if (vstorage->LevelFiles(i).empty()){
        continue;
      }
      filenames.clear();
      //std::cout << i << std::endl;
      std::vector<FileMetaData*> level_files = vstorage->LevelFiles(i);
      FileMetaData* level_file;
      double bits_per_key = bpk_list[i];
      // if (i != 0) {
      //   bits_per_key = bpk_by_level[i];
      // } else {
      //   bits_per_key = _env->bits_per_key;
      // }
      if (bits_per_key <= 0.5) {
        table_op->filter_policy.reset();
      } else {
        table_op->filter_policy.reset(NewBloomFilterPolicy(bits_per_key));
      }
      op->table_factory.reset(NewBlockBasedTableFactory(*table_op));
      
      
      uint32_t num_entries_by_level = 0 ;
      for (uint32_t j = 0; j < level_files.size(); j++) {
        level_file = level_files[j];
        std::string filename = MakeTableFileName(level_file->fd.GetNumber());
        if (bits_per_key == 0) {
          objective_value += db_stats.fileID2empty_queries.at(level_file->fd.GetNumber());
        } else {
          objective_value += db_stats.fileID2empty_queries.at(level_file->fd.GetNumber())*std::exp(-log_2_squared*bits_per_key);
        }

        Status s = createNewSstFile(_env->path + "/" + filename, _env->path + "-temp/" + filename, op, env_op, read_op);
        if (!s.ok()) std::cout << s.ToString() << std::endl;
        num_entries_by_level += level_file->num_entries - level_file->num_range_deletions;
        filenames.push_back(_env->path + "-temp/" + filename);
      }
      ingest_opts.picked_level = i;
      db_monkey->IngestExternalFile(filenames, ingest_opts);
      std::cout << "level " << i << " bits_per_key : " << bits_per_key << " \t bits : " << bits_per_key*num_entries_by_level << " false positive rate : " << 100*exp(-log_2_squared*bits_per_key) << "%" << std::endl;
      used_memory += (uint64_t) bits_per_key*num_entries_by_level;
      level_counter++;
    }
  
  table_op->filter_policy.reset(NewBloomFilterPolicy(_env->bits_per_key));
  op->table_factory.reset(NewBlockBasedTableFactory(*table_op));

  std::cout << "Objective Value : " << objective_value << std::endl;
  std::cout << "Used memory: " << used_memory << std::endl;

  std::string delete_temp_dir_cmd = "rm -rf " + _env->path + "-temp/";
  system(delete_temp_dir_cmd.c_str());
  return Status::OK();
}

Status createDbWithMonkeyPlus(const EmuEnv* _env, DB* db, DB* db_monkey_plus, Options *op, BlockBasedTableOptions *table_op, const WriteOptions *write_op,
  ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions *env_op, const DbStats & db_stats) {
  if (db_stats.num_levels <= 1) return Status::OK(); // do nothing when L <= 1
  uint64_t total_filter_memory = db_stats.num_entries * _env->bits_per_key;
  std:: cout << "Total memory: " << total_filter_memory << std::endl;
  // solve min \sum p_i subject to \sum  - f_i * \ln p_i = N * bpk,
  // N is the number of total entries, apply Lagrange operator and we get f_i/p_i is a constant C
  // we try to solve C first. There could exist some levels which do not have filters (bpk = 0) in the optimal solution,

  double S = 0; // S = \sum f_i * \ln p_i
  std::vector<pair<uint64_t, uint64_t> > entries_with_levelID;
  uint64_t last_level_with_entries = 0;
  for (uint64_t i = 1; i < db_stats.level2entries.size(); i++) {
    if (db_stats.level2entries[i] > 0) {
      S += std::log(db_stats.level2entries[i])*db_stats.level2entries[i];
      entries_with_levelID.emplace_back(db_stats.level2entries[i], i);
      last_level_with_entries = i;
    }
  }

  for (uint64_t i = 0; i < db_stats.entries_in_level0.size(); i++) {
    if (db_stats.entries_in_level0[i] > 0) {
      S += std::log(db_stats.entries_in_level0[i])*db_stats.entries_in_level0[i];
      entries_with_levelID.emplace_back(db_stats.level2entries[i], last_level_with_entries + i + 1);
    }
  }
  if (entries_with_levelID.size() <= 1) return Status::OK();
  const double log_2_squared = std::pow(std::log(2), 2);
  double C = -(total_filter_memory*log_2_squared + S) / (double)db_stats.num_entries;
  std::sort(entries_with_levelID.begin(), entries_with_levelID.end(), std::greater<pair<uint64_t, uint64_t>>());
  
  // we use dual optimization and linear to find the number of non-filitered levels Y.
  // we may need to compare with binary search later (TBD)
  uint64_t Y = 0;
  uint64_t remaining_entries = db_stats.num_entries;
  
  while (std::log(entries_with_levelID[Y].first) + C > -log_2_squared && Y < entries_with_levelID.size()) {
    Y++;
    S -= std::log(entries_with_levelID[Y].first)*entries_with_levelID[Y].first;
    remaining_entries -= entries_with_levelID[Y].first;
    C = -(total_filter_memory*log_2_squared + S) / remaining_entries;
  }
  if (Y == entries_with_levelID.size()) return Status::OK();
 
  std::vector<double> bpk_by_level = std::vector<double> (last_level_with_entries + db_stats.entries_in_level0.size() + 1, 0.0);
  for (uint64_t i = Y; i < entries_with_levelID.size(); i++) {
    bpk_by_level[entries_with_levelID[i].second] = -(C + std::log(entries_with_levelID[i].first))/log_2_squared;
  }
  DBImpl::GetImplOptions get_impl_options;
  get_impl_options.column_family = db->DefaultColumnFamily();
  auto cfh = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(get_impl_options.column_family);
  auto cfd = cfh->cfd();
  const auto* vstorage = cfd->current()->storage_info();

  uint32_t level_counter = 0;
  IngestExternalFileOptions ingest_opts;
  ingest_opts.move_files = true;
  std::vector<std::string> filenames;
  double objective_value = 0.0;
  std::string create_temp_dir_cmd = "mkdir -p " + _env->path + "-temp/";
  system(create_temp_dir_cmd.c_str());
  for (int i = 0; i < cfd->NumberLevels(); i++) {
      if (vstorage->LevelFiles(i).empty()){
        continue;
      }
      filenames.clear();
      //std::cout << i << std::endl;
      std::vector<FileMetaData*> level_files = vstorage->LevelFiles(i);
      FileMetaData* level_file;
      double bits_per_key = 0;
      if (i != 0) {
        bits_per_key = bpk_by_level[i];
        if (bits_per_key <= 0.5) {
          table_op->filter_policy.reset();
        } else {
          table_op->filter_policy.reset(NewBloomFilterPolicy(bits_per_key));
        }
        op->table_factory.reset(NewBlockBasedTableFactory(*table_op));
      }
      
      uint32_t num_entries_by_level = 0 ;
      for (uint32_t j = 0; j < level_files.size(); j++) {
        if (i == 0) {
          bits_per_key = bpk_by_level[j + last_level_with_entries + 1];
          if (bits_per_key <= 0.5) {
            table_op->filter_policy.reset();
          } else {
            table_op->filter_policy.reset(NewBloomFilterPolicy(bits_per_key));
          }
          op->table_factory.reset(NewBlockBasedTableFactory(*table_op));
          std::cout << "Level 0: " << j << " file bpk: " << bits_per_key << std::endl;
        }
        level_file = level_files[j];
        std::string filename = MakeTableFileName(level_file->fd.GetNumber());
        if (bits_per_key == 0) {
            objective_value += db_stats.fileID2empty_queries.at(level_file->fd.GetNumber());
          } else {
            objective_value += db_stats.fileID2empty_queries.at(level_file->fd.GetNumber())*std::exp(-log_2_squared*bits_per_key);
          }
        num_entries_by_level += level_file->num_entries - level_file->num_range_deletions;
        Status s = createNewSstFile(_env->path + "/" + filename, _env->path + "-temp/" + filename, op, env_op, read_op);
        if (!s.ok()) std::cout << s.ToString() << std::endl;
        filenames.push_back(_env->path + "-temp/" + filename);
      }
      ingest_opts.picked_level = i;
      db_monkey_plus->IngestExternalFile(filenames, ingest_opts);
      std::cout << "level " << i << " bits_per_key : " << bits_per_key << " \t bits : " << bits_per_key*num_entries_by_level << " false positive rate : " << 100*exp(-log_2_squared*bits_per_key) << "%" << std::endl;
      level_counter++;
    }
  
  table_op->filter_policy.reset(NewBloomFilterPolicy(_env->bits_per_key));
  op->table_factory.reset(NewBlockBasedTableFactory(*table_op));

  std::cout << "Objective Value : " << objective_value << std::endl;

  std::string delete_temp_dir_cmd = "rm -rf " + _env->path + "-temp/";
  system(delete_temp_dir_cmd.c_str());
  return Status::OK();
}

Status createDbWithOptBpk(const EmuEnv* _env, DB* db, DB* db_optimal, Options *op, BlockBasedTableOptions *table_op, const WriteOptions *write_op,
  ReadOptions *read_op, const FlushOptions *flush_op, EnvOptions *env_op, const DbStats & db_stats) {
  if (db_stats.num_levels <= 1) return Status::OK(); // do nothing when L <= 1
  uint64_t total_filter_memory = db_stats.num_entries * _env->bits_per_key;
  // solve \sum - ln p_i / (ln 2)^2 * n_i = M where n_i is the number of entries per file
  // plugged with Lagrange multiplier we have p_i * z_i / n_i should be a constant C where z_i is the number of non-existing
  // queries per file. Let C = p_i * z_i / n_i, we try to solve C first and then calculate p_i per file
  std:: cout << "Total memory: " << total_filter_memory << std::endl;
  // let S = \sum (ln n_i / z_i) * n_i, we have - (\sum z_i) * C - S = M * (ln 2)^2

  // p_i refers to the false positive rate of the i-th file, empty_queries represent the number of empty queries
  double S = 0;
  uint64_t fileID = 0;
  uint64_t empty_queries = 0;
  uint64_t num_entries_with_empty_queries = 0;
  std::priority_queue<pair<double, uint64_t> > entries_over_empty_with_fileID;
  double tmp;
  // fileID2entries represents the map between fileID and the number of entries of the associated file
  // fileID2empty_enrties represents the number of empty queries of the associated file
  for(auto iter = db_stats.fileID2entries.begin(); iter != db_stats.fileID2entries.end(); iter++) {
    if (db_stats.fileID2empty_queries.find(iter->first) != db_stats.fileID2empty_queries.end()) {
      empty_queries = db_stats.fileID2empty_queries.at(iter->first);
      if (empty_queries != 0 && iter->second != 0) {
	      tmp = iter->second*1.0/db_stats.fileID2empty_queries.at(iter->first);
        //std::cout << " fileID: " << iter->first << " tmp: " << tmp << std::endl;
        S += std::log(tmp*db_stats.num_total_empty_queries*1.0)*iter->second;
	      num_entries_with_empty_queries += iter->second;
	      entries_over_empty_with_fileID.push(make_pair(tmp, iter->first));
      }      
    }
  }
  const double log_2_squared = pow(std::log(2), 2);
  double num_total_empty_queries = db_stats.num_total_empty_queries;
  double C = -(total_filter_memory*log_2_squared + S)*1.0/num_entries_with_empty_queries;

  // final bpk assignments are stored in fileID2bpk
  unordered_map<uint64_t, double> fileID2bpk;
  // fileID with no bpk will also be stored in fileIDwithNobpk
  unordered_set<uint64_t> fileIDwithNobpk;
  // Calculating S = \sum (ln n_i / z_i) * n_i
  while (!entries_over_empty_with_fileID.empty() && std::log(entries_over_empty_with_fileID.top().first*num_total_empty_queries) + C > -log_2_squared) {
     uint64_t fileID = entries_over_empty_with_fileID.top().second;
     S -= std::log(entries_over_empty_with_fileID.top().first*num_total_empty_queries*1.0)*db_stats.fileID2entries.at(fileID);
     num_entries_with_empty_queries -= db_stats.fileID2entries.at(fileID);
     S += num_entries_with_empty_queries*std::log((num_total_empty_queries - db_stats.fileID2empty_queries.at(fileID))*1.0/num_total_empty_queries);
     num_total_empty_queries -= db_stats.fileID2empty_queries.at(fileID);
     fileIDwithNobpk.insert(fileID);
     fileID2bpk.emplace(fileID, 0.0);
     C = -(total_filter_memory*log_2_squared + S)/num_entries_with_empty_queries;
     entries_over_empty_with_fileID.pop();
     //std::cout << "log(entries_over_empty_with_fileID.top().first) + C : " << std::log(entries_over_empty_with_fileID.top().first) + C << std::endl;
  }
  double bpk = 0.0;
  double final_total_memory = 0.0;
  for (auto iter = db_stats.fileID2entries.begin(); iter != db_stats.fileID2entries.end(); iter++) {
    if (db_stats.fileID2empty_queries.find(iter->first) != db_stats.fileID2empty_queries.end()) {
      empty_queries = db_stats.fileID2empty_queries.at(iter->first);
      if (empty_queries != 0 && fileIDwithNobpk.find(iter->first) == fileIDwithNobpk.end()) {
        bpk = -(std::log(iter->second*num_total_empty_queries*1.0/empty_queries) + C)/log_2_squared;
        fileID2bpk.emplace(iter->first, bpk);
        final_total_memory += bpk*iter->second;
      } else {
        fileID2bpk.emplace(iter->first, 0.0);
      }
    }
  }
  
  DBImpl::GetImplOptions get_impl_options;
  get_impl_options.column_family = db->DefaultColumnFamily();
  auto cfh = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(get_impl_options.column_family);
  auto cfd = cfh->cfd();
  const auto* vstorage = cfd->current()->storage_info();
  uint32_t level_counter = 0;
  IngestExternalFileOptions ingest_opts;
  ingest_opts.move_files = true;
  std::vector<std::string> filenames;
  uint64_t point_reads_by_level = 0;
  uint64_t existing_point_reads_by_level = 0;
  double total_memory_used = 0.0;
  double min_bpk = std::numeric_limits<double>::max();
  double max_bpk = 0.0;  
  double objective_value = 0.0;

  std::string create_temp_dir_cmd = "mkdir -p " + _env->path + "-temp/";
  system(create_temp_dir_cmd.c_str());
  for (int i = 0; i < cfd->NumberLevels(); i++) {
    point_reads_by_level = 0;
    existing_point_reads_by_level = 0;
    if (vstorage->LevelFiles(i).empty()){
      continue;
    }
    filenames.clear();
    //std::cout << i << std::endl;
    std::vector<FileMetaData*> level_files = vstorage->LevelFiles(i);
    FileMetaData* level_file;
    double bits_per_key = 0;
        
    uint32_t num_entries_by_level = 0 ;
    for (uint32_t j = 0; j < level_files.size(); j++) {
      level_file = level_files[j];
      //std::cout << "level " << i << "\tfileID:" << level_file->fd.GetNumber() << std::endl;
      auto iter = fileID2bpk.find(level_file->fd.GetNumber());
      if (iter == fileID2bpk.end() || iter->second <= 0.5) {
        bits_per_key = 0.0;
        //std::cout << " bitsPerKey : 0.0" << std::endl;
        table_op->filter_policy.reset();
        objective_value += db_stats.fileID2empty_queries.at(iter->first);
      } else {
        bits_per_key = iter->second;
        //std::cout << " bitsPerKey : " << iter->second << std::endl;
        max_bpk = std::max(bits_per_key, max_bpk);
        min_bpk = std::min(bits_per_key, min_bpk);
        table_op->filter_policy.reset(NewBloomFilterPolicy(bits_per_key));
        objective_value += db_stats.fileID2empty_queries.at(iter->first)*std::exp(-log_2_squared*bits_per_key);
      }
      total_memory_used += bits_per_key * level_file->num_entries;
	    point_reads_by_level += level_files[j]->stats.num_point_reads.load(std::memory_order_relaxed);
	    existing_point_reads_by_level += level_files[j]->stats.num_existing_point_reads.load(std::memory_order_relaxed);
      
      op->table_factory.reset(NewBlockBasedTableFactory(*table_op));
      std::string filename = MakeTableFileName(level_file->fd.GetNumber());
      num_entries_by_level += level_file->num_entries - level_file->num_range_deletions;
      Status s = createNewSstFile(_env->path + "/" + filename, _env->path + "-temp/" + filename, op, env_op, read_op);
      if (!s.ok()) std::cout << s.ToString() << std::endl;
      filenames.push_back(_env->path + "-temp/" + filename);
    }
    ingest_opts.picked_level = i;
    std::cout << " Level " << i << " access frequencies : num_point_reads (" << point_reads_by_level << "), num_tp_reads (" << existing_point_reads_by_level << ")" << std::endl;
    db_optimal->IngestExternalFile(filenames, ingest_opts);
    level_counter++;
  }

  table_op->filter_policy.reset(NewBloomFilterPolicy(_env->bits_per_key));
  op->table_factory.reset(NewBlockBasedTableFactory(*table_op));

  std::cout << "Objective Value : " << objective_value << std::endl;
  std::cout << "Total memory used : " << total_memory_used << std::endl;
  std::cout << "Maximum bpk : " << max_bpk << std::endl;
  std::cout << "Minimum bpk : " << min_bpk << std::endl;

  std::string delete_temp_dir_cmd = "rm -rf " + _env->path + "-temp/";
  system(delete_temp_dir_cmd.c_str());
  return Status::OK();
}

void printBFBitsPerKey(DB *db) {
  DBImpl::GetImplOptions get_impl_options;
  get_impl_options.column_family = db->DefaultColumnFamily();
  auto cfh = reinterpret_cast<rocksdb::ColumnFamilyHandleImpl*>(get_impl_options.column_family);
  auto cfd = cfh->cfd();
  const auto* vstorage = cfd->current()->storage_info();
    
  uint32_t num_general_levels = cfd->NumberLevels();
  uint64_t total_tail_size = 0;
  for (int i = 0; i < num_general_levels; i++) {
    if (vstorage->LevelFiles(i).empty()){
      continue;
    }
    std::cout << " Level " << i << " : "; 
    std::vector<FileMetaData*> level_files = vstorage->LevelFiles(i);
    FileMetaData* level_file;
    for (uint32_t j = 0; j < level_files.size(); j++) {
      level_file = level_files[j];
      uint64_t filenumber = level_file->fd.GetNumber();
      std::string filename = MakeTableFileName(filenumber);
      if (level_file->bpk != -1) {
        std::cout << filename << "(" << level_file->bpk << ", " << level_file->stats.num_point_reads.load(std::memory_order_relaxed) << ", " << level_file->stats.num_existing_point_reads.load(std::memory_order_relaxed) << ") ";
      }
      total_tail_size += level_file->tail_size;
    }
    std::cout << std::endl;
  }
  std::cout << "Total tail size : " << total_tail_size << std::endl;
}

void getNaiveMonkeyBitsPerKey(size_t num_entries, size_t num_entries_per_table, double size_ratio, size_t max_files_in_L0,
		double overall_bits_per_key, std::vector<double>* naive_monkey_bits_per_key_list, bool dynamic_cmpct, bool optimize_L0_files) {	
  assert(naive_monkey_bits_per_key_list);
  assert(naive_monkey_bits_per_key_list->size() > 0);
  const double log_2_squared = std::pow(std::log(2), 2);
  if (!optimize_L0_files) {
    num_entries -= num_entries_per_table*max_files_in_L0;
  }
  size_t num_files = (num_entries + num_entries_per_table - 1)/num_entries_per_table;
  uint64_t total_filter_memory = overall_bits_per_key*num_entries;
  if (num_files <= max_files_in_L0 || size_ratio == 1.0) {
    // assign the same bits-per-key if the workload fits in L0 (the size of each L0 file is roughly the same)
    naive_monkey_bits_per_key_list->resize(naive_monkey_bits_per_key_list->size(), overall_bits_per_key);
  }
  assert(size_ratio > 1);
  size_t num_levels = std::min((size_t) std::ceil(std::log((num_files - max_files_in_L0)* (size_ratio - 1.0)*1.0/size_ratio + 1)/std::log(size_ratio)) + 1,
      naive_monkey_bits_per_key_list->size()) - 1;

  std::cout << "num_levels : " << num_levels << "\tnum_files : " << num_files << std::endl;
  double S1 = 0;
  double S2 = 0;
  if (optimize_L0_files) {
      S1 = num_entries_per_table*1.0/log_2_squared;
      S2 = max_files_in_L0*S1*std::log(S1);
  }
  double base = size_ratio*num_entries_per_table*1.0/log_2_squared;
  for (size_t i = 0; i < num_levels; i++) {
    S1 += base;
    S2 += base*std::log(base);
    base *= size_ratio;
  }
  double log_lambda = -(total_filter_memory + S2)/S1;
  size_t max_level_with_filter = num_levels;
  while (log_lambda  + max_level_with_filter*std::log(size_ratio) + std::log(num_entries_per_table) > -log_2_squared &&
    max_level_with_filter >= 1) {
    S2 -= base*std::log(base);
    S1 -= base;
    max_level_with_filter--;
    log_lambda = -(total_filter_memory + S2)/S1;
  }
  if (optimize_L0_files) {
    naive_monkey_bits_per_key_list->at(0) = 
      -(log_lambda + std::log(num_entries_per_table/log_2_squared))/log_2_squared;
  }
  for (size_t i = max_level_with_filter; i + 1 < naive_monkey_bits_per_key_list->size(); i++) {
    naive_monkey_bits_per_key_list->at(i + 1) = 0.0;
  }
  base = size_ratio*num_entries_per_table;
  for (size_t i = 1; i <= max_level_with_filter; i++) {
    naive_monkey_bits_per_key_list->at(i) = 
    -(log_lambda + std::log(base*1.0/log_2_squared))/log_2_squared;
    if (naive_monkey_bits_per_key_list->at(i) < 1.0) {
      naive_monkey_bits_per_key_list->at(i) = 0.0;
    }
    base *= size_ratio;
  }
  if (dynamic_cmpct) {
    int movement = naive_monkey_bits_per_key_list->size() - 1 - num_levels;
    if (movement > 0) {
      for (int i = naive_monkey_bits_per_key_list->size() - 1; i >= movement + 1; i--) {
        naive_monkey_bits_per_key_list->at(i) = naive_monkey_bits_per_key_list->at(i - movement);
      }
      if (max_level_with_filter >= 1) {
        for (size_t i = 2; i <= movement && i < naive_monkey_bits_per_key_list->size(); i++) {
          naive_monkey_bits_per_key_list->at(i) = naive_monkey_bits_per_key_list->at(1);
        }
      }
    }
  }

  if(!optimize_L0_files) {
    naive_monkey_bits_per_key_list->at(0) = overall_bits_per_key;
  }
  
  std::cout << "Configuring naive Monkey bpk list: ";
  for (size_t i = 0; i + 1 < naive_monkey_bits_per_key_list->size(); i++) {
    std::cout << naive_monkey_bits_per_key_list->at(i) << ",";
  }
  std::cout << naive_monkey_bits_per_key_list->back() << std::endl;
}

void printEmulationOutput(const EmuEnv* _env, const QueryTracker *track, uint16_t runs) {
  int l = 16;
  std::cout << std::endl;
  std::cout << "-----LSM state-----" << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << "T" << std::setfill(' ') << std::setw(l) 
                                                  << "P" << std::setfill(' ') << std::setw(l) 
                                                  << "B" << std::setfill(' ') << std::setw(l) 
                                                  << "E" << std::setfill(' ') << std::setw(l) 
                                                  << "M" << std::setfill(' ') << std::setw(l) 
                                                  << "f" << std::setfill(' ') << std::setw(l) 
                                                  << "file_size" << std::setfill(' ') << std::setw(l) 
                                                  << "compaction_pri" << std::setfill(' ') << std::setw(l) 
                                                  << "bpk" << std::setfill(' ') << std::setw(l);
  std::cout << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << _env->size_ratio;
  std::cout << std::setfill(' ') << std::setw(l) << _env->buffer_size_in_pages;  
  std::cout << std::setfill(' ') << std::setw(l) << _env->entries_per_page;
  std::cout << std::setfill(' ') << std::setw(l) << _env->entry_size;
  std::cout << std::setfill(' ') << std::setw(l) << _env->buffer_size;
  std::cout << std::setfill(' ') << std::setw(l) << _env->file_to_memtable_size_ratio;
  std::cout << std::setfill(' ') << std::setw(l) << _env->file_size;
  std::cout << std::setfill(' ') << std::setw(l) << _env->compaction_pri;
  std::cout << std::setfill(' ') << std::setw(l) << _env->bits_per_key;
  std::cout << std::endl;

  std::cout << std::endl;
  std::cout << "----- Query summary -----" << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << "#I" << std::setfill(' ') << std::setw(l)
                                                << "#U" << std::setfill(' ') << std::setw(l)
                                                << "#D" << std::setfill(' ') << std::setw(l)
                                                << "#R" << std::setfill(' ') << std::setw(l)
                                                << "#Q" << std::setfill(' ') << std::setw(l)
                                                << "#Z" << std::setfill(' ') << std::setw(l)
                                                << "#S" << std::setfill(' ') << std::setw(l)
                                                << "#TOTAL" << std::setfill(' ') << std::setw(l);;            
  std::cout << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << track->inserts_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->updates_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->point_deletes_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->range_deletes_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->point_lookups_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->zero_point_lookups_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->range_lookups_completed/runs;
  std::cout << std::setfill(' ') << std::setw(l) << track->total_completed/runs;
  std::cout << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << "I" << std::setfill(' ') << std::setw(l)
                                                << "U" << std::setfill(' ') << std::setw(l)
                                                << "D" << std::setfill(' ') << std::setw(l)
                                                << "R" << std::setfill(' ') << std::setw(l)
                                                << "Q" << std::setfill(' ') << std::setw(l)
                                                << "Z" << std::setfill(' ') << std::setw(l)
                                                << "S" << std::setfill(' ') << std::setw(l)
                                                << "TOTAL" << std::setfill(' ') << std::setw(l);   

  std::cout << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->inserts_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->updates_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->point_deletes_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->range_deletes_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(4) 
                                              << static_cast<double>(track->point_lookups_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(4) 
                                              << static_cast<double>(track->zero_point_lookups_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->range_lookups_cost)/runs/1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->workload_exec_time)/runs/1000000;
  std::cout << std::endl;

  std::cout << "----- Latency(ms/op) -----" << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << "avg_I" << std::setfill(' ') << std::setw(l)
                                                << "avg_U" << std::setfill(' ') << std::setw(l)
                                                << "avg_D" << std::setfill(' ') << std::setw(l)
                                                << "avg_R" << std::setfill(' ') << std::setw(l)
                                                << "avg_Q" << std::setfill(' ') << std::setw(l)
                                                << "avg_Z" << std::setfill(' ') << std::setw(l)
                                                << "avg_S" << std::setfill(' ') << std::setw(l)
                                                << "avg_query" << std::setfill(' ') << std::setw(l);   

  std::cout << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->inserts_cost) / track->inserts_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->updates_cost) / track->updates_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->point_deletes_cost) / track->point_deletes_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->range_deletes_cost) / track->range_deletes_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(4) 
                                              << static_cast<double>(track->point_lookups_cost) / track->point_lookups_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(4) 
                                              << static_cast<double>(track->zero_point_lookups_cost) / track->zero_point_lookups_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->range_lookups_cost) / track->range_lookups_completed / 1000000;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) 
                                              << static_cast<double>(track->workload_exec_time) / track->total_completed / 1000000;
  std::cout << std::endl;

  std::cout << "----- Throughput(op/ms) -----" << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << "avg_I" << std::setfill(' ') << std::setw(l)
            << "avg_U" << std::setfill(' ') << std::setw(l)
            << "avg_D" << std::setfill(' ') << std::setw(l)
            << "avg_R" << std::setfill(' ') << std::setw(l)
            << "avg_Q" << std::setfill(' ') << std::setw(l)
            << "avg_Z" << std::setfill(' ') << std::setw(l)
            << "avg_S" << std::setfill(' ') << std::setw(l)
            << "avg_query" << std::setfill(' ') << std::setw(l);

  std::cout << std::endl;

  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << static_cast<double>(track->inserts_completed*1000000) / track->inserts_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << static_cast<double>(track->updates_completed*1000000) / track->updates_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << static_cast<double>(track->point_deletes_completed*1000000) / track->point_deletes_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << static_cast<double>(track->range_deletes_completed*1000000) / track->range_deletes_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(4)
            << static_cast<double>(track->point_lookups_completed*1000000) / track->point_lookups_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(4)
            << static_cast<double>(track->zero_point_lookups_completed*1000000) / track->zero_point_lookups_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << static_cast<double>(track->range_lookups_completed*1000000) / track->range_lookups_cost;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << static_cast<double>(track->total_completed*1000000) / track->workload_exec_time;
  std::cout << std::endl;

  std::cout << "----- Compaction costs -----" << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << "avg_space_amp" << std::setfill(' ') << std::setw(l)
            << "avg_write_amp" << std::setfill(' ') << std::setw(l)
            << "avg_read_amp" << std::setfill(' ') << std::setw(l)
            << "avg_stalls" << std::setfill(' ') << std::setw(l);

  std::cout << std::endl;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << track->space_amp/runs;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << track->write_amp/runs;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << track->read_amp/runs;
  std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2)
            << track->stalls/runs;
  std::cout << std::endl;

  if (_env->verbosity >= 1) {
    std::cout << std::endl;
    std::cout << "-----I/O stats-----" << std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << "mem_get_count" << std::setfill(' ') << std::setw(l)
                                                  << "mem_get_time" << std::setfill(' ') << std::setw(l)
                                                  << "sst_get_time" << std::setfill(' ') << std::setw(l)
                                                  << "mem_bloom_hit" << std::setfill(' ') << std::setw(l)
                                                  << "mem_bloom_miss" << std::setfill(' ') << std::setw(l)
                                                  << "sst_bloom_hit" << std::setfill(' ') << std::setw(l)
                                                  << "sst_bloom_miss" << std::setfill(' ') << std::setw(l)  
                                                  << "sst_bloom_tp_hit" << std::setfill(' ') << std::setw(l)  
                                                  << "get_cpu_nanos" << std::setfill(' ') << std::setw(l);  
    std::cout << std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << track->get_from_memtable_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->get_from_memtable_time/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->get_from_output_files_time/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->bloom_memtable_hit_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->bloom_memtable_miss_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->bloom_sst_hit_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->bloom_sst_miss_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->bloom_sst_true_positive_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->get_cpu_nanos/runs;
    std::cout << std::endl;


    std::cout << std::setfill(' ') << std::setw(l) << "bloom_accesses" << std::setfill(' ') << std::setw(l) 
                                                  << "index_accesses" << std::setfill(' ') << std::setw(l)
                                                  << "filter_blk_hit" << std::setfill(' ') << std::setw(l)
                                                  << "index_blk_hit" << std::setfill(' ') << std::setw(l)
                                                  << "accessed_data_blks" << std::setfill(' ') << std::setw(l)
                                                  << "cached_data_blks" << std::setfill(' ') << std::setw(l);
    std::cout << std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << track->filter_block_read_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->index_block_read_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->block_cache_filter_hit_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->block_cache_index_hit_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->data_block_read_count/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->data_block_cached_count/runs;
    std::cout << std::endl;

    std::cout << std::setfill(' ') << std::setw(l) << "read_bytes" << std::setfill(' ') << std::setw(l)
                                                  << "read_nanos" << std::setfill(' ') << std::setw(l)
                                                  << "cpu_read_nanos" << std::setfill(' ') << std::setw(l)
                                                  << "write_bytes" << std::setfill(' ') << std::setw(l)
                                                  << "write_nanos" << std::setfill(' ') << std::setw(l)
                                                  << "cpu_write_nanos" << std::setfill(' ') << std::setw(l);  
    std::cout << std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << track->bytes_read/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->read_nanos/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->cpu_read_nanos/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->bytes_written/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->write_nanos/runs;
    std::cout << std::setfill(' ') << std::setw(l) << track->cpu_write_nanos/runs;
    std::cout << std::endl;
  }
    std::cout << std::endl;
    std::cout << "-----Other stats-----" << std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << "read_table_mem" << std::setfill(' ') << std::setw(l) << "block_cache_usage" << std::setfill(' ') << std::setw(l)<< std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << track->read_table_mem/runs << std::setfill(' ') << std::setw(l) << track->block_cache_usage << std::setfill(' ') << std::setw(l) << std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << "user_cpu_usage" << std::setfill(' ') << std::setw(l) << "sys_cpu_usage" << std::setfill(' ') << std::setw(l)<< std::endl;
    std::cout << std::setfill(' ') << std::setw(l) << track->ucpu_pct/runs << std::setfill(' ') << std::setw(l) << track->scpu_pct/runs << std::setfill(' ') << std::setw(l) << std::endl;
}

void print_point_read_stats_distance_collector(std::vector<std::pair<double, double> >* point_reads_statistics_distance_collector) {
  assert(point_reads_statistics_distance_collector);
  size_t l = 32;
  // print the difference of estimated statistics
  for (size_t i = 0; i < 2*l + 3; i++) {
    std::cout << "-";
  }
  std::cout << std::endl;
  std::cout << "|";
  std::cout << std::setfill(' ') << std::setw(l) << "num_point_reads distance";
  std::cout << "|";
  std::cout << std::setfill(' ') << std::setw(l) << "num_existing_point_reads distance";
  std::cout << "|" << std::endl;
  for (size_t i = 0; i < 2*l + 3; i++) {
    std::cout << "-";
  }
  std::cout << std::endl;
  for (size_t i = 0; i < point_reads_statistics_distance_collector->size(); i++) {
    std::cout << "|";
    std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) << point_reads_statistics_distance_collector->at(i).first;
    std::cout << "|";
    std::cout << std::setfill(' ') << std::setw(l) << std::fixed << std::setprecision(2) << point_reads_statistics_distance_collector->at(i).second;
    std::cout << "|" << std::endl;
  }
  for (size_t i = 0; i < 2*l + 3; i++) {
    std::cout << "-";
  }
  std::cout << std::endl;
}

void dump_query_stats(const DbStats & db_stats, const std::string & path) {
  std::ofstream outfile (path.c_str());
  outfile << db_stats.num_entries << " " << db_stats.bits_per_key << " " << db_stats.num_files << std::endl;
  for (auto & e : db_stats.fileID2entries) {
    if (db_stats.fileID2empty_queries.find(e.first) != db_stats.fileID2empty_queries.end()) {
      outfile << e.first << " " << e.second << " " << db_stats.fileID2empty_queries.at(e.first) << std::endl;
    } else {
      outfile << e.first << " " << e.second << " 0" << std::endl;
    }
  }
  outfile.close();
}

void configOptions(EmuEnv* _env, Options *op, BlockBasedTableOptions *table_op, WriteOptions *write_op, ReadOptions *read_op, FlushOptions *flush_op) {
    // Experiment settings
    _env->experiment_runs = (_env->experiment_runs >= 1) ? _env->experiment_runs : 1;

    // *op = Options();
    op->write_buffer_size = _env->buffer_size; 
    op->point_reads_track_method = _env->point_reads_track_method;

    // Compaction
    switch (_env->compaction_pri) {
      case 1:
        op->compaction_pri = kMinOverlappingRatio; break;
      case 2:
        op->compaction_pri = kByCompensatedSize; break;
      case 3:
        op->compaction_pri = kOldestLargestSeqFirst; break;
      case 4:
        op->compaction_pri = kOldestSmallestSeqFirst; break;
      default:
        std::cerr << "error: compaction_pri" << std::endl;
    }

    op->max_bytes_for_level_multiplier = _env->size_ratio;
    //op->compaction_options_universal.size_ratio = _env->size_ratio;
    op->target_file_size_base = _env->file_size;
    
     
   
    op->max_bytes_for_level_base = _env->buffer_size * _env->size_ratio;
    op->num_levels = _env->num_levels;


  // table_options.enable_index_compression = kNoCompression;
 
  //Other DBOptions
  op->create_if_missing = _env->create_if_missing;
  op->use_direct_reads = _env->use_direct_reads;
  op->use_direct_io_for_flush_and_compaction = _env->use_direct_io_for_flush_and_compaction;
  op->level0_file_num_compaction_trigger = _env->level0_file_num_compaction_trigger;

  //TableOptions
  table_op->no_block_cache = _env->no_block_cache; // TBC
  if(table_op->no_block_cache){
     _env->block_cache_capacity = 0;
  }else{
     _env->cache_index_and_filter_blocks = true;
  }
  if (_env->block_cache_capacity == 0) {
      ;// do nothing
  } else {
      std::shared_ptr<Cache> cache = NewLRUCache(_env->block_cache_capacity*1024, -1, false, _env->block_cache_high_priority_ratio);
      table_op->block_cache = cache;
      ;// invoke manual block_cache
  }

  table_op->bpk_alloc_type = _env->bits_per_key_alloc_type;
  if (_env->bits_per_key == 0) {
      ;// do nothing
  } else {
    table_op->filter_policy.reset(NewBloomFilterPolicy(_env->bits_per_key, false));    // currently build full filter instead of blcok-based filter
  }

  table_op->cache_index_and_filter_blocks = _env->cache_index_and_filter_blocks;
  table_op->cache_index_and_filter_blocks_with_high_priority = _env->cache_index_and_filter_blocks_with_high_priority;    // Deprecated by no_block_cache
  
   
  table_op->block_size = _env->entries_per_page * _env->entry_size;
  table_op->pin_top_level_index_and_filter = _env->pin_top_level_index_and_filter;
   
  // Set all table options
  op->table_factory.reset(NewBlockBasedTableFactory(*table_op));

  write_op->low_pri = _env->low_pri;
 
    //FlushOptions
  flush_op->allow_write_stall = _env->allow_write_stall;

  op->level_compaction_dynamic_level_bytes = _env->level_compaction_dynamic_level_bytes;

}

void populateQueryTracker(QueryTracker *query_track, DB* _db, const BlockBasedTableOptions& table_options, EmuEnv* _env) {
  query_track->workload_exec_time = query_track->inserts_cost + query_track->updates_cost + query_track->point_deletes_cost 
                                    + query_track->range_deletes_cost + query_track->point_lookups_cost + query_track->zero_point_lookups_cost
                                    + query_track->range_lookups_cost;
  query_track->get_from_memtable_count += get_perf_context()->get_from_memtable_count;
  query_track->get_from_memtable_time += get_perf_context()->get_from_memtable_time;
  query_track->get_from_output_files_time += get_perf_context()->get_from_output_files_time;
  query_track->filter_block_read_count += get_perf_context()->filter_block_read_count;
  query_track->index_block_read_count += get_perf_context()->index_block_read_count;
  query_track->block_cache_filter_hit_count += get_perf_context()->block_cache_filter_hit_count;
  query_track->block_cache_index_hit_count += get_perf_context()->block_cache_index_hit_count;
  query_track->bloom_memtable_hit_count += get_perf_context()->bloom_memtable_hit_count;
  query_track->bloom_memtable_miss_count += get_perf_context()->bloom_memtable_miss_count;
  query_track->bloom_sst_true_positive_count += _db->GetOptions().statistics->getTickerCount(BLOOM_FILTER_FULL_TRUE_POSITIVE);
  query_track->bloom_sst_hit_count += get_perf_context()->bloom_sst_hit_count;
  query_track->bloom_sst_miss_count += get_perf_context()->bloom_sst_miss_count;
  query_track->get_cpu_nanos += get_perf_context()->get_cpu_nanos;
  query_track->bytes_read += get_iostats_context()->bytes_read;
  query_track->read_nanos += get_iostats_context()->read_nanos;
  query_track->cpu_read_nanos += get_iostats_context()->cpu_read_nanos;
  query_track->bytes_written += get_iostats_context()->bytes_written;
  query_track->write_nanos += get_iostats_context()->write_nanos;
  query_track->cpu_write_nanos += get_iostats_context()->cpu_write_nanos;
  query_track->data_block_cached_count += _db->GetOptions().statistics->getTickerCount(BLOCK_CACHE_DATA_ADD);

  // Space amp
  uint64_t live_sst_size = 0;
  
  uint64_t calculate_size = 1024 * (query_track->inserts_completed - query_track->point_deletes_completed);
  query_track->space_amp = static_cast<double>(live_sst_size) / calculate_size;
  std::map<std::string, std::string> cfstats;
  _db->GetMapProperty("rocksdb.cfstats", &cfstats);
//    for (std::map<std::string, std::string>::iterator it=cfstats.begin(); it !=cfstats.end(); ++it)
//    std::cout << it->first << " => " << it->second << '\n';

  // Write amp
  query_track->write_amp = std::stod(cfstats.find("compaction.Sum.WriteAmp")->second);

  // TODO:: wrong read amp
  query_track->read_amp = std::stod(cfstats.find("compaction.Sum.Rnp1GB")->second)
        / std::stod(cfstats.find("compaction.Sum.RnGB")->second);

  // stalls triggered by compactions
  query_track->stalls = std::stod(cfstats.find("total-stops")->second);

  string table_readers_mem;
  _db->GetProperty("rocksdb.estimate-table-readers-mem", &table_readers_mem);
  std::cout << "rocksdb.estimate-table-readers-mem:" << table_readers_mem << std::endl;
  query_track->read_table_mem += atoi(table_readers_mem.c_str());
  std::cout << std::endl;

  // block cache
  if(table_options.block_cache){
    query_track->block_cache_usage += table_options.block_cache->GetUsage();
  }

  query_track->data_block_read_count += GetTotalUsedDataBlocks(_env->num_levels, _env->verbosity);
}

void db_point_lookup(DB* _db, const ReadOptions *read_op, const std::string key, const int verbosity, QueryTracker *query_track){
    my_clock start_clock, end_clock;    // clock to get query time
    string value;
    Status s;

    if (verbosity == 2)
      std::cout << "Q " << key << "" << std::endl;
    if (my_clock_get_time(&start_clock) == -1)
      std::cerr << s.ToString() << "start_clock failed to get time" << std::endl;
    //s = db->Get(*read_op, ToString(key), &value);
    s = _db->Get(*read_op, key, &value);
    // assert(s.ok());
    if (my_clock_get_time(&end_clock) == -1) 
      std::cerr << s.ToString() << "end_clock failed to get time" << std::endl;
    
    if (!s.ok()) {    // zero_reuslt_point_lookup
        if (verbosity == 2) {

           std::cerr << s.ToString() << "key = " << key << std::endl;
        }
        query_track->zero_point_lookups_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->zero_point_lookups_completed;
    } else{
        query_track->point_lookups_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->point_lookups_completed;
    }
    ++query_track->total_completed;
}

void write_collected_throughput(std::vector<vector<std::pair<double, double> > > collected_throughputs, std::vector<std::string> names, std::string throughput_path, std::string bpk_path, uint32_t interval) {
  assert(collected_throughputs.size() == names.size());
  assert(collected_throughputs.size() > 0);
  ofstream throughput_ofs(throughput_path.c_str());
  ofstream bpk_ofs(bpk_path.c_str());
  throughput_ofs << "ops";
  bpk_ofs << "ops";
  for (int i = 0; i < names.size(); i++) {
    throughput_ofs << ",tput-" << names[i];
    bpk_ofs << ",bpk-" << names[i];
  }
  throughput_ofs << std::endl;
  bpk_ofs << std::endl;
  for (int j = 0; j < collected_throughputs[0].size(); j++) {
    throughput_ofs << j*interval;
    bpk_ofs << j*interval;
    for (int i = 0; i < collected_throughputs.size(); i++) {
      throughput_ofs << "," << collected_throughputs[i][j].first;
      bpk_ofs << "," << collected_throughputs[i][j].second;
    }
    throughput_ofs << std::endl;
    bpk_ofs << std::endl;
  }
  throughput_ofs.close();
  bpk_ofs.close();
}

// Run a workload from memory
// The workload is stored in WorkloadDescriptor
// Use QueryTracker to record performance for each query operation
int runWorkload(DB* _db, const EmuEnv* _env, Options *op, const BlockBasedTableOptions *table_op, 
                const WriteOptions *write_op, const ReadOptions *read_op, const FlushOptions *flush_op,
                EnvOptions* env_op, const WorkloadDescriptor *wd, QueryTracker *query_track,
                std::vector<std::pair<double, double> >* throughput_and_bpk_collector,
                std::vector<SimilarityResult >* point_reads_statistics_distance_collector) {

  Status s;
  Iterator *it = _db-> NewIterator(*read_op); // for range reads
  uint64_t counter = 0, mini_counter = 0; // tracker for progress bar. TODO: avoid using these two 
  uint32_t cpu_sample_counter = 0;
  bool eval_point_read_statistics_accuracy_flag = false;
  if (point_reads_statistics_distance_collector != nullptr) {
    eval_point_read_statistics_accuracy_flag = true;
    point_reads_statistics_distance_collector->clear();
  }
  bool collect_throughput_flag = false;
  if (throughput_and_bpk_collector != nullptr) {
    collect_throughput_flag = true;
    throughput_and_bpk_collector->clear();
  }
  my_clock start_clock, end_clock;    // clock to get query time
  // Clear System page cache before running
  if (_env->clear_sys_page_cache) { 
    std::cout << "\nClearing system page cache before experiment ..."; 
    fflush(stdout);
    clearPageCache();
    get_perf_context()->Reset();
    get_iostats_context()->Reset();
    std::cout << " OK!" << std::endl;
  }

  CpuUsage cpu_stat;
  cpuUsageInit();
  double ucpu_pct = 0.0;
  double scpu_pct = 0.0;
  std::vector<string> point_reads_collector;
  std::vector<double> num_point_reads_distance_vector;
  std::vector<double> num_existing_point_reads_distance_vector;
  uint64_t total_ingestion_num = wd->insert_num + wd->update_num + wd->pdelete_num + wd->rdelete_num;
  if (total_ingestion_num == 0 || wd->plookup_num == 0) {
    eval_point_read_statistics_accuracy_flag = false;
  }
  int eval_point_read_statistics_accuracy_count = 0;
  DB* ground_truth_db = nullptr;
  for (const auto &qd : wd->queries) { 
    std::string key, start_key, end_key;
    std::string value;
    int thread_index;
    Entry *entry_ptr = nullptr;
    RangeEntry *rentry_ptr = nullptr;

    switch (qd.type) {
      case INSERT:
        ++counter;
        assert(counter = qd.seq);
        if (query_track->inserts_completed == wd->actual_insert_num) break;
	/*
        for(size_t k = 0; k < lookup_threads_pool.size(); k++){
            if(lookup_threads_pool[k] != nullptr){
                lookup_threads_pool[k]->join();
     	        delete lookup_threads_pool[k];
                lookup_threads_pool[k] = nullptr;
            }
        }*/
        entry_ptr = static_cast<Entry*>(qd.entry_ptr);
        key = entry_ptr->key;
        value = entry_ptr->value;
        if (_env->verbosity == 2)
          std::cout << static_cast<char>(qd.type) << " " << key << " " << value << "" << std::endl;
        if (my_clock_get_time(&start_clock) == -1)
          std::cerr << s.ToString() << "start_clock failed to get time" << std::endl;
        s = _db->Put(*write_op, key, value);
        if (my_clock_get_time(&end_clock) == -1) 
          std::cerr << s.ToString() << "end_clock failed to get time" << std::endl;
        if (!s.ok()) std::cerr << s.ToString() << std::endl;
        assert(s.ok());
        query_track->inserts_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->total_completed;
        ++query_track->inserts_completed;
        break;

      case UPDATE:
        ++counter;
        assert(counter = qd.seq);
        entry_ptr = static_cast<Entry*>(qd.entry_ptr);
        key = entry_ptr->key;
        value = entry_ptr->value;
	/*
        for(size_t k = 0; k < lookup_threads_pool.size(); k++){
            if(lookup_threads_pool[k] != nullptr){
            lookup_threads_pool[k]->join();
     	    delete lookup_threads_pool[k];
            lookup_threads_pool[k] = nullptr;
		}
        }*/
        if (_env->verbosity == 2)
          std::cout << static_cast<char>(qd.type) << " " << key << " " << value << "" << std::endl;
        if (my_clock_get_time(&start_clock) == -1)
          std::cerr << s.ToString() << "start_clock failed to get time" << std::endl;
        //s = _db->Put(*write_op, ToString(key), value);
        s = _db->Put(*write_op, key, value);
        if (!s.ok()) std::cerr << s.ToString() << std::endl;
        if (my_clock_get_time(&end_clock) == -1) 
          std::cerr << s.ToString() << "end_clock failed to get time" << std::endl;
        assert(s.ok());
        query_track->updates_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->updates_completed;
        ++query_track->total_completed;
        break;

      case DELETE:
        ++counter;
        assert(counter = qd.seq);
        key = qd.entry_ptr->key;
	/*
        for(size_t k = 0; k < lookup_threads_pool.size(); k++){
            if(lookup_threads_pool[k] != nullptr){
                lookup_threads_pool[k]->join();
     	        delete lookup_threads_pool[k];
                lookup_threads_pool[k] = nullptr;
	    }
        } */
        if (_env->verbosity == 2)
          std::cout << static_cast<char>(qd.type) << " " << key << "" << std::endl;
        if (my_clock_get_time(&start_clock) == -1)
          std::cerr << s.ToString() << "start_clock failed to get time" << std::endl;
        //s = _db->Delete(*write_op, ToString(key));
        s = _db->Delete(*write_op, key);
        if (my_clock_get_time(&end_clock) == -1) 
          std::cerr << s.ToString() << "end_clock failed to get time" << std::endl;
        assert(s.ok());
        query_track->point_deletes_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->point_deletes_completed;
        ++query_track->total_completed;
        break;

      case RANGE_DELETE:
        ++counter;
        assert(counter = qd.seq);
	/*
        for(size_t k = 0; k < lookup_threads_pool.size(); k++){
            if(lookup_threads_pool[k] != nullptr){
                lookup_threads_pool[k]->join();
     	        delete lookup_threads_pool[k];
                lookup_threads_pool[k] = nullptr;
	    }
        }*/
        rentry_ptr = static_cast<RangeEntry*>(qd.entry_ptr);
        start_key = rentry_ptr->key;
        end_key = rentry_ptr->end_key;
        if (_env->verbosity == 2)
          std::cout << static_cast<char>(qd.type) << " " << start_key << " " << end_key << "" << std::endl;
        if (my_clock_get_time(&start_clock) == -1)
          std::cerr << s.ToString() << "start_clock failed to get time" << std::endl;
        //s = _db->DeleteRange(*write_op, _db->DefaultColumnFamily(), ToString(start_key), ToString(end_key));
        s = _db->DeleteRange(*write_op, _db->DefaultColumnFamily(), start_key, end_key);
        if (my_clock_get_time(&end_clock) == -1) 
          std::cerr << s.ToString() << "end_clock failed to get time" << std::endl;
        assert(s.ok());
        query_track->range_deletes_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->range_deletes_completed;
        ++query_track->total_completed;
        break;

      case LOOKUP:
        ++counter;
        assert(counter = qd.seq);
        // for pseudo zero-reuslt point lookup
        // if (query_track->point_lookups_completed + query_track->zero_point_lookups_completed >= 10) break;
        key = qd.entry_ptr->key;
        if (eval_point_read_statistics_accuracy_flag && query_track->inserts_completed + 
        query_track->updates_completed + query_track->point_deletes_completed + 
        query_track->range_deletes_completed > 0) {
          point_reads_collector.push_back(key);
        }
		    db_point_lookup(_db, read_op, key, _env->verbosity, query_track);
        break;

      case RANGE_LOOKUP:
        ++counter; 
        assert(counter = qd.seq);
        rentry_ptr = static_cast<RangeEntry*>(qd.entry_ptr);
        start_key = rentry_ptr->key;
        //end_key = start_key + rentry_ptr->range;
        end_key = rentry_ptr->end_key;
        if (_env->verbosity == 2)
          std::cout << static_cast<char>(qd.type) << " " << start_key << " " << end_key << "" << std::endl;
        it->Refresh();    // to update a stale iterator view
        assert(it->status().ok());
        if (my_clock_get_time(&start_clock) == -1)
          std::cerr << s.ToString() << "start_clock failed to get time" << std::endl;
       
        for (it->Seek(start_key); it->Valid(); it->Next()) {
          // std::cout << "found key = " << it->key().ToString() << std::endl;
          if(it->key() == end_key) {    // TODO: check correntness
            break;
          }
        }
        if (my_clock_get_time(&end_clock) == -1) 
          std::cerr << s.ToString() << "end_clock failed to get time" << std::endl;
        if (!it->status().ok()) {
          std::cerr << it->status().ToString() << std::endl;
        }
        query_track->range_lookups_cost += getclock_diff_ns(start_clock, end_clock);
        ++query_track->range_lookups_completed;
        ++query_track->total_completed;
        break;

      default:
          std::cerr << "Unknown query type: " << static_cast<char>(qd.type) << std::endl;
    }
    
    if(counter%(wd->total_num/50) == 0){ 
      cpu_stat = getCurrentCpuUsage();
      ucpu_pct += cpu_stat.user_time_pct;
      scpu_pct += cpu_stat.sys_time_pct;
      cpu_sample_counter++;
    }
    showProgress(wd->total_num, counter, mini_counter);

    if (collect_throughput_flag) {
      if (counter%_env->throughput_collect_interval == 0) {
         uint64_t exec_time = query_track->inserts_cost + query_track->updates_cost + query_track->point_deletes_cost 
                                    + query_track->range_deletes_cost + query_track->point_lookups_cost + query_track->zero_point_lookups_cost
                                    + query_track->range_lookups_cost;
        if (counter != 0 && exec_time != 0) {
          throughput_and_bpk_collector->emplace_back(counter*1.0/exec_time, getCurrentAverageBitsPerKey(_db, op));
           //throughput_and_bpk_collector->push_back(get_iostats_context()->bytes_read);
        } else {
          throughput_and_bpk_collector->emplace_back(0.0, _env->bits_per_key);
        }
      }
    }
    if (eval_point_read_statistics_accuracy_flag) {
      uint32_t ingestion_num = query_track->inserts_completed + 
        query_track->updates_completed + query_track->point_deletes_completed + 
        query_track->range_deletes_completed;
      string value;
      if (ingestion_num%_env->eval_point_read_statistics_accuracy_interval == 0 && 
        ingestion_num/_env->eval_point_read_statistics_accuracy_interval != 
          eval_point_read_statistics_accuracy_count) {
        s = _db->Flush(*flush_op);
        assert(s.ok());
        s = BackgroundJobMayAllCompelte(_db);
        assert(s.ok());
        std::string create_temp_dir_cmd = "mkdir -p " +  _env->path + "-temp && rm " + _env->path + "-temp/*";
        system(create_temp_dir_cmd.c_str());
        sleep_for_ms(WAIT_INTERVAL*10);
        std::string copy_db_cmd = "cp " + _env->path + "-to-be-eval/* " + _env->path + "-temp/";
        system(copy_db_cmd.c_str());
	op->point_reads_track_method = rocksdb::PointReadsTrackMethod::kNaiiveTrack;
        s = DB::Open(*op, _env->path + "-temp", &ground_truth_db);
        if (!s.ok()) std::cerr << s.ToString() << std::endl;
        resetPointReadsStats(ground_truth_db);
        for (const string & key: point_reads_collector) {
          ground_truth_db->Get(*read_op, key, &value);
        }
	op->point_reads_track_method = _env->point_reads_track_method;
        DbStats stats_to_be_evaluated;
        DbStats stats_ground_truth;
	if (op->point_reads_track_method == rocksdb::PointReadsTrackMethod::kDynamicCompactionAwareTrack) {
          collectDbStats(_db, &stats_to_be_evaluated, false, query_track->point_lookups_completed + query_track->zero_point_lookups_completed, _env->poiont_read_learning_rate, true);
	} else {
          collectDbStats(_db, &stats_to_be_evaluated, false, query_track->point_lookups_completed + query_track->zero_point_lookups_completed, _env->poiont_read_learning_rate, false);
	}
        collectDbStats(ground_truth_db, &stats_ground_truth, false, query_track->point_lookups_completed + query_track->zero_point_lookups_completed, 0, false);
	if (stats_ground_truth.num_entries == 0) {
		stats_ground_truth.num_entries = stats_to_be_evaluated.num_entries;
	}
        CloseDB(ground_truth_db, *flush_op);
        point_reads_statistics_distance_collector->push_back(SimilarityResult(
                                ComputePointQueriesStatisticsByEuclideanDistance(stats_to_be_evaluated, stats_ground_truth),
                                ComputePointQueriesStatisticsByCosineSimilarity(stats_to_be_evaluated, stats_ground_truth),
                                ComputePointQueriesStatisticsByLevelwiseDistanceType(stats_to_be_evaluated, stats_ground_truth, 1),
				ComputePointQueriesStatisticsByLevelwiseDistanceType(stats_to_be_evaluated, stats_ground_truth, 2)));
	
        eval_point_read_statistics_accuracy_count = ingestion_num/_env->eval_point_read_statistics_accuracy_interval;
      }
    }
  }

  if (collect_throughput_flag) {
    uint64_t exec_time = query_track->inserts_cost + query_track->updates_cost + query_track->point_deletes_cost 
                                    + query_track->range_deletes_cost + query_track->point_lookups_cost + query_track->zero_point_lookups_cost
                                    + query_track->range_lookups_cost;
        if (counter != 0 && exec_time != 0) {
          throughput_and_bpk_collector->emplace_back(counter*1.0/exec_time, getCurrentAverageBitsPerKey(_db, op));
        } else {
          throughput_and_bpk_collector->emplace_back(0.0, _env->bits_per_key);
        }
  }


  delete it; 
  it = nullptr;
   /* 
  for(std::thread* t: lookup_threads_pool){
     if(t != nullptr){
        t->join();
        delete t;
     }
  } 
  lookup_threads_pool.clear();*/
  if(cpu_sample_counter == 0) cpu_sample_counter = 1;
  query_track->ucpu_pct += ucpu_pct/cpu_sample_counter;
  query_track->scpu_pct += scpu_pct/cpu_sample_counter;
  //std::cout << std::endl << ucpu_pct << " " << scpu_pct << std::endl;
  ucpu_pct = 0.0;
  scpu_pct = 0.0;
  
  // flush and wait for the steady state
  _db->Flush(*flush_op);
  //FlushMemTableMayAllComplete(_db);
  //CompactionMayAllComplete(_db);

  return 0;
}

uint64_t GetTotalUsedDataBlocks(uint32_t num_levels, int verbosity) {
  if (num_levels == 1) return 0;
  std::map<uint32_t, PerfContextByLevel>* level_to_perf_context = nullptr;
  if (get_perf_context()->level_to_perf_context != nullptr) {
    level_to_perf_context = get_perf_context()->level_to_perf_context;
  }
  if (level_to_perf_context == nullptr) {
    return 0;
  }
  PerfContextByLevel perf_context_single_level; 
  uint64_t total_used_data_blocks = 0;
  uint64_t total_false_positives = 0;
  for (uint32_t i = 0; i < num_levels; i++) {
    if (level_to_perf_context->find(i) != level_to_perf_context->end()) {
      perf_context_single_level = level_to_perf_context->at(i);	  
      total_used_data_blocks += perf_context_single_level.used_data_block_count;
      total_false_positives += perf_context_single_level.bloom_filter_full_positive - perf_context_single_level.bloom_filter_full_true_positive;
      if (verbosity > 0) {
        std::cout << " Level " << i << ": ";
        std::cout << " used data blocks (" << perf_context_single_level.used_data_block_count << ") ";
        std::cout << " filter/index hits(" << perf_context_single_level.block_cache_hit_count << ") ";
        std::cout << " filter/index misses(" << perf_context_single_level.block_cache_miss_count << ") ";
        std::cout << " false positives (" << perf_context_single_level.bloom_filter_full_positive - perf_context_single_level.bloom_filter_full_true_positive << "). " << " with (full positive: " << perf_context_single_level.bloom_filter_full_positive << ")" << std::endl;
      }
    }
  }
  if (verbosity > 0) {
    std::cout << "Tree : total used data blocks (" << total_used_data_blocks << ") " << " total false positves (" << total_false_positives << ")." << std::endl;
  }
  return total_used_data_blocks;
}


void cpuUsageInit(){
    struct tms timeSample;
    lastCPU = times(&timeSample);
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;
}

CpuUsage getCurrentCpuUsage(){
    struct tms timeSample;
    clock_t now;
    double user_percent, sys_percent;
    CpuUsage result;

    now = times(&timeSample);
    if (now <= lastCPU || timeSample.tms_stime < lastSysCPU ||
        timeSample.tms_utime < lastUserCPU){
        //Overflow detection. Just skip this value.
                user_percent = 0.0;
                sys_percent = 0.0;
    } else{
        user_percent = timeSample.tms_utime - lastUserCPU;
        user_percent /= (now - lastCPU);
        user_percent *= 100;
        sys_percent = timeSample.tms_stime - lastSysCPU;
        sys_percent /= (now - lastCPU);
        sys_percent *= 100;
     }
     lastCPU = now;
     lastSysCPU = timeSample.tms_stime;
     lastUserCPU = timeSample.tms_utime;
     result.user_time_pct = user_percent;
     result.sys_time_pct = sys_percent; 
     return result;
}

std::vector<std::string> StringSplit(std::string &str, char delim){
  std::vector<std::string> splits;
  std::stringstream ss(str);
  std::string item;
  while(getline(ss, item, delim)){
    splits.push_back(item);
  }
  return splits;
}
