#ifndef PACKAGE_H
#define PACKAGE_H

#define USE_ZLIB

#include <string>
#include <map>
#include <vector>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

#define MAX_CHUNK_NAME_LENGTH 255

typedef uint32_t len_t;

class package;

class chunk_writer
{
private:
    package *pkg;
    string name;
    len_t first_block;
    len_t cur_block;
    len_t block_len;
#ifdef USE_ZLIB
    z_stream zs;
    Bytef *z_buffer;
#endif
    void raw_write(const void *data, len_t len);
    void finish_block(len_t next);
public:
    chunk_writer(package *parent, const string _name);
    ~chunk_writer();
    void write(const void *data, len_t len);
    friend class package;
};

class chunk_reader
{
private:
    chunk_reader(package *parent, len_t start);
    void init(len_t start);
    package *pkg;
    len_t first_block, next_block;
    len_t off, block_left;
#ifdef USE_ZLIB
    bool eof;
    z_stream zs;
    Bytef z_buffer[32768];
#endif
    len_t raw_read(void *data, len_t len);
public:
    chunk_reader(package *parent, const string _name);
    ~chunk_reader();
    len_t read(void *data, len_t len);
    void read_all(vector<char> &data);
    friend class package;
};

class package
{
public:
    package(const char* file, bool writeable, bool empty = false);
    package();
    ~package();
    chunk_writer* writer(const string name);
    chunk_reader* reader(const string name);
    void commit();
    void delete_chunk(const string name);
    bool has_chunk(const string name);
    vector<string> list_chunks();
    void abort();
    void unlink();

    // statistics
    len_t get_slack();
    len_t get_size() const { return file_len; };
    len_t get_chunk_fragmentation(const string name);
    len_t get_chunk_compressed_length(const string name);
private:
    string filename;
    bool rw;
    int fd;
    len_t file_len;
    int n_users;
    bool dirty;
    bool aborted;
    bool tmp;
    map<string, len_t> directory;
    map<len_t, len_t> free_blocks;
    vector<len_t> unlinked_blocks;
    map<len_t, pair<len_t, len_t> > block_map;
    set<len_t> new_chunks;
    map<len_t, uint32_t> reader_count;
    len_t extend_block(len_t at, len_t size, len_t by);
    len_t alloc_block(len_t &size);
    void finish_chunk(const string name, len_t at);
    void free_chunk(const string name);
    len_t write_directory();
    void collect_blocks();
    void free_block_chain(len_t at);
    void free_block(len_t at, len_t size);
    void seek(len_t to);
    void fsck();
    void read_directory(len_t start, uint8_t version);
    void trace_chunk(len_t start);
    void load();
    void load_traces();
    friend class chunk_writer;
    friend class chunk_reader;
};

#endif
