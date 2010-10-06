#ifndef PACKAGE_H
#define PACKAGE_H

#define USE_ZLIB

#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include <stack>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

typedef uint32_t len_t;

class package;

class chunk_writer
{
private:
    package *pkg;
    std::string name;
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
    chunk_writer(package *parent, const std::string _name);
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
    len_t next_block;
    len_t off, block_left;
#ifdef USE_ZLIB
    bool eof;
    z_stream zs;
    Bytef z_buffer[32768];
#endif
    len_t raw_read(void *data, len_t len);
public:
    chunk_reader(package *parent, const std::string _name);
    ~chunk_reader();
    len_t read(void *data, len_t len);
    void read_all(std::vector<char> &data);
    friend class package;
};

class package
{
public:
    package(const char* file, bool writeable, bool empty = false);
    ~package();
    chunk_writer* writer(const std::string name);
    chunk_reader* reader(const std::string name);
    void commit();
    void delete_chunk(const std::string name);
    bool has_chunk(const std::string name);
    std::vector<std::string> list_chunks();
    void abort();
    void unlink();
private:
    std::string filename;
    bool rw;
    int fd;
    len_t file_len;
    int n_users;
    bool dirty;
    bool aborted;
    std::map<std::string, len_t> directory;
    std::map<len_t, len_t> free_blocks;
    std::stack<len_t> unlinked_blocks;
    std::map<len_t, std::pair<len_t, len_t> > block_map;
    len_t extend_block(len_t at, len_t size, len_t by);
    len_t alloc_block();
    void finish_chunk(const std::string name, len_t at);
    void free_chunk(const std::string name);
    len_t write_directory();
    void collect_blocks();
    void free_block(len_t at, len_t size);
    void seek(len_t to);
    void fsck();
    void read_directory(len_t start);
    void trace_chunk(len_t start);
    void load();
    friend class chunk_writer;
    friend class chunk_reader;
};

#endif
