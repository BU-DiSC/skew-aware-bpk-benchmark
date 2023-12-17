rocksdbpath=../skew-aware-rocksdb-8.9.1
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

all: bpk_benchmark 

bpk_benchmark: librocksdb emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	echo ${rocksdbpath}
	$(CXX) $(CXXFLAGS) $@.cc -o$@ emu_environment.cc emu_util.cc workload_stats.cc aux_time.cc ${rocksdbpath}/librocksdb.a -I${rocksdbpath}/include -I${rocksdbpath} -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)

bpk_benchmark_debug: librocksdb_debug bpk_benchmark.cc emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc
	$(CXX) $(CXXFLAGS) bpk_benchmark.cc -o$@ emu_environment.cc workload_stats.cc aux_time.cc emu_util.cc ${rocksdbpath}/librocksdb_debug.a -I${rocksdbpath}/include -I${rocksdbpath} -O0 -ggdb -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)

clean:
	rm -rf bpk_benchmark bpk_benchmark_debug bpk_benchmark_debug.* 

librocksdb:
	cd ${rocksdbpath} && $(MAKE) static_lib

librocksdb_debug:
	cd ${rocksdbpath} && $(MAKE) dbg_hack
