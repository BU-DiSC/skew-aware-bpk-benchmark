rocksdbpath=../skew-aware-rocksdb-8.9.1
#rocksdbpath=../rocksdb-8.9.1
include ${rocksdbpath}/make_config.mk


ifndef DISABLE_JEMALLOC
	ifdef JEMALLOC
		PLATFORM_CXXFLAGS += -DROCKSDB_JEMALLOC -DJEMALLOC_NO_DEMANGLE
	endif
	EXEC_LDFLAGS := $(JEMALLOC_LIB) $(EXEC_LDFLAGS) -lpthread
	PLATFORM_CXXFLAGS += $(JEMALLOC_INCLUDE)
endif

ifneq ($(USE_RTTI), 1)
	CXXFLAGS += -fno-rtti
endif

.PHONY: clean librocksdb

all: bpk_benchmark query_lat_exp query_statistics_est_benchmark runtime_tput_exp

query_statistics_est_benchmark: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) $@.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result

query_statistics_est_benchmark_debug: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) -g query_statistics_est_benchmark.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O0 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result


runtime_tput_exp: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) $@.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result

runtime_tput_exp_debug: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) -g runtime_tput_exp.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O0 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result


query_lat_exp: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) $@.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result

query_lat_exp_debug: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) -g query_lat_exp.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O0 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result


bpk_benchmark: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) $@.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result

bpk_benchmark_debug: librocksdb_debug emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) bpk_benchmark.cc -o$@ emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O0 -ggdb -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -Wno-unused-result

clean:
	rm -rf bpk_benchmark bpk_benchmark_debug query_lat_exp query_lat_exp_debug bpk_benchmark_debug.* query_statistics_est_benchmark query_statistics_est_benchmark_debug runtime_tput_exp runtime_tput_exp_debug

librocksdb:
	cd ${rocksdbpath} && $(MAKE) static_lib

librocksdb_debug:
	cd ${rocksdbpath} && $(MAKE) dbg
