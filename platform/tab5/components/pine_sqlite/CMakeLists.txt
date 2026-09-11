idf_component_register(SRCS "../../.deps/sqlite/sqlite3.c" "pine_vfs.cpp"
  INCLUDE_DIRS "../../.deps/sqlite" REQUIRES esp_system)
target_compile_definitions(${COMPONENT_LIB} PRIVATE NDEBUG=1 SQLITE_OS_OTHER=1 SQLITE_THREADSAFE=1
  SQLITE_MUTEX_PTHREADS=1 SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_OMIT_WAL=1
  SQLITE_TEMP_STORE=3 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DEFAULT_CACHE_SIZE=-512)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-value)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
