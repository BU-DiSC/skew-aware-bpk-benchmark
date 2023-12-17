#include "workload_stats.h"

void loadWorkload(WorkloadDescriptor *wd) {
	std::cout << "Loading workload ..." << std::endl;
  assert(wd != nullptr);
  assert(wd->queries.size() == 0);
	std::ifstream f;
	//uint32_t key, start_key, end_key;
	std::string key, start_key, end_key;
	std::string value;
	char mode;
  BaseEntry *tmp = nullptr;
  f.open(wd->path_);
  assert(f);
  while(f >> mode) {
    QueryDescriptor qd;
    switch (mode) {
      case 'I':
        f >> key >> value;
        // create a entry
        tmp = new Entry(key, value);
        qd.seq = wd->queries.size() + 1;
        qd.type = INSERT;
        qd.entry_ptr = tmp;
        wd->queries.push_back(qd);
        ++wd->insert_num;
        ++wd->total_num;
        break;
      case 'U':
        f >> key >> value;
        tmp = new Entry(key, value);
        qd.seq = wd->queries.size() + 1;
        qd.type = UPDATE;
        qd.entry_ptr = tmp; 
        wd->queries.push_back(qd);
        ++wd->update_num;
        ++wd->total_num;
        break;
      case 'D':
        f >> key;
        tmp = new BaseEntry(key);
        qd.seq = wd->queries.size() + 1;
        qd.type = DELETE;
        qd.entry_ptr = tmp; 
        wd->queries.push_back(qd);
        ++wd->pdelete_num;
        ++wd->total_num;
        break;
      case 'R':
        f >> start_key >> end_key;
        //tmp = new RangeEntry(start_key, end_key - start_key);
        tmp = new RangeEntry(start_key, end_key);
        qd.seq = wd->queries.size() + 1;
        qd.type = RANGE_DELETE;
        qd.entry_ptr = tmp; 
        wd->queries.push_back(qd);
        ++wd->rdelete_num;
        ++wd->total_num;
        break;
      case 'Q':
        f >> key;
        tmp = new BaseEntry(key);
        qd.seq = wd->queries.size() + 1;
        qd.type = LOOKUP;
        qd.entry_ptr = tmp; 
        wd->queries.push_back(qd);
        ++wd->plookup_num;
        ++wd->total_num;
        break;
      case 'S':
        f >> start_key >> end_key;
        //tmp = new RangeEntry(start_key, end_key - start_key);
        tmp = new RangeEntry(start_key, end_key);
        qd.seq = wd->queries.size() + 1;
        qd.type = RANGE_LOOKUP;
        qd.entry_ptr = tmp; 
        wd->queries.push_back(qd);
        ++wd->rlookup_num;
        ++wd->total_num;
        break;
      default:
        std::cerr << "Unconfigured query mode: " << mode << std::endl;
        break;
    }
  }
  // for creating pseudo zeor-result point lookup
  // wd->actual_insert_num = static_cast<uint64_t> (wd->plookup_num * wd->plookup_hit_rate);
  // wd->actual_total_num  =  wd->total_num - wd->insert_num + wd->actual_insert_num;
  wd->actual_insert_num = wd->insert_num;   
  wd->actual_total_num = wd->total_num;    
  f.close();
  std::cout << "Load complete ..." << std::endl;
}

void dumpStats(QueryTracker *sample, const QueryTracker *single) {
  if (sample == nullptr) sample = new QueryTracker();
  sample->total_completed += single->total_completed;
  sample->inserts_completed += single->inserts_completed;
  sample->updates_completed += single->updates_completed;
  sample->point_deletes_completed += single->point_deletes_completed;
  sample->range_deletes_completed += single->range_deletes_completed;
  sample->point_lookups_completed += single->point_lookups_completed;
  sample->zero_point_lookups_completed += single->zero_point_lookups_completed;
  sample->range_lookups_completed += single->range_lookups_completed;

  sample->inserts_cost += single->inserts_cost;
  sample->updates_cost += single->updates_cost;
  sample->point_deletes_cost += single->point_deletes_cost;
  sample->range_deletes_cost += single->range_deletes_cost;
  sample->point_lookups_cost += single->point_lookups_cost;
  sample->zero_point_lookups_cost += single->zero_point_lookups_cost;
  sample->range_lookups_cost += single->range_lookups_cost;

  sample->workload_exec_time += single->workload_exec_time;

  sample->bloom_sst_true_positive_count += single->bloom_sst_true_positive_count;
  sample->bloom_memtable_hit_count += single->bloom_memtable_hit_count;    
  sample->bloom_memtable_miss_count += single->bloom_memtable_miss_count;   
  sample->bloom_sst_hit_count += single->bloom_sst_hit_count;      
  sample->bloom_sst_miss_count += single->bloom_sst_miss_count;   
  sample->get_from_memtable_time += single->get_from_memtable_time;          
  sample->get_from_memtable_count += single->get_from_memtable_count; 
  sample->get_from_output_files_time += single->get_from_output_files_time; 
  sample->filter_block_read_count += single->filter_block_read_count;  
  sample->data_block_read_count += single->data_block_read_count;
  sample->index_block_read_count += single->index_block_read_count;  
  sample->block_cache_index_hit_count += single->block_cache_index_hit_count;  
  sample->block_cache_filter_hit_count += single->block_cache_filter_hit_count;  
  sample->bytes_read += single->bytes_read; 
  sample->read_nanos += single->read_nanos; 
  sample->cpu_read_nanos += single->cpu_read_nanos;
  sample->bytes_written += single->bytes_written;  
  sample->write_nanos += single->write_nanos;  
  sample->cpu_write_nanos += single->cpu_write_nanos;
  sample->get_cpu_nanos += single->get_cpu_nanos;

  sample->space_amp += single->space_amp;
  sample->write_amp += single->write_amp;
  sample->read_amp += single->read_amp;
  sample->stalls += single->stalls;
  sample->read_table_mem += single->read_table_mem;
  sample->block_cache_usage += single->block_cache_usage;


  sample->ucpu_pct += single->ucpu_pct;
  sample->scpu_pct += single->scpu_pct;

}
