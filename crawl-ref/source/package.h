#pragma once

#define USE_ZLIB

#include <map>
#include <string>
#include <vector>
#ifdef USE_ZLIB
#include <zlib.h>
#endif

#if !defined(DGAMELAUNCH) && !defined(__ANDROID__) && !defined(DEBUG_DIAGNOSTICS)
#define DO_FSYNC
#endif

#define MAX_CHUNK_NAME_LENGTH 255

typedef uint32_t plen_t;

class package;

class chunk_writer
{
private:
    package *pkg;
    string name;
    plen_t first_block;
    plen_t cur_block;
    plen_t block_len;
#ifdef USE_ZLIB
    z_stream zs;
    Bytef *z_buffer;
#endif
    void raw_write(const void *data, plen_t len);
    void finish_block(plen_t next);
public:
    chunk_writer(package *parent, const string &_name);
    ~chunk_writer();
    void write(const void *data, plen_t len);
    friend class package;
};

class chunk_reader
{
private:
    chunk_reader(package *parent, plen_t start);
    void init(plen_t start);
    package *pkg;
    plen_t first_block, next_block;
    plen_t off, block_left;
#ifdef USE_ZLIB
    bool eof;
    z_stream zs;
    Bytef z_buffer[32768];
#endif
    plen_t raw_read(void *data, plen_t len);
public:
    chunk_reader(package *parent, const string &_name);
    ~chunk_reader();
    plen_t read(void *data, plen_t len);
    void read_all(vector<char> &data);
    friend class package;
};

class package
{
public:
    package(const char* file, bool writeable, bool empty = false);
    package();
    ~package();
    chunk_writer* writer(const string &name);
    chunk_reader* reader(const string &name);
    void commit();
    void delete_chunk(const string &name);
    bool has_chunk(const string &name);
    vector<string> list_chunks();
    void abort();
    void unlink();

    // statistics
    plen_t get_slack();
    plen_t get_size() const { return file_len; };
    plen_t get_chunk_fragmentation(const string &name);
    plen_t get_chunk_compressed_length(const string &name);
private:
    string filename;
    bool rw;
    int fd;
    plen_t file_len;
    int n_users;
    bool dirty;
    bool aborted;
#ifdef DO_FSYNC
    bool tmp;
#endif
    map<string, plen_t> directory;
    map<plen_t, plen_t> free_blocks;
    vector<plen_t> unlinked_blocks;
    map<plen_t, pair<plen_t, plen_t> > block_map;
    set<plen_t> new_chunks;
    map<plen_t, uint32_t> reader_count;
    plen_t extend_block(plen_t at, plen_t size, plen_t by);
    plen_t alloc_block(plen_t &size);
    void finish_chunk(const string &name, plen_t at);
    void free_chunk(const string &name);
    plen_t write_directory();
    void collect_blocks();
    void free_block_chain(plen_t at);
    void free_block(plen_t at, plen_t size);
    void seek(plen_t to);
    void fsck();
    void read_directory(plen_t start, uint8_t version);
    void trace_chunk(plen_t start);
    void load();
    void load_traces();
    friend class chunk_writer;
    friend class chunk_reader;
};

