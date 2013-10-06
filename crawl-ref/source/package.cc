/**
 * @file
 * @brief A container for chunks that make up the save file.
**/

/*
Guarantees:
* A crash at any moment may not cause corruption -- the save will return to
  the exact state it had at the last commit().

Notes:
* Unless DO_FSYNC is defined, crashes that put down the operating system
  may break the consistency guarantee.
* Incomplete writes don't have any effects, but don't break commits or reads
  (which both use the last complete write).
* Readers always get the last complete (but not necessarily committed) write
  (ie, READ_UNCOMMITTED) at the time they started; it is safe to continue
  reading even if the chunk has been changed since.
*/

#include "AppHdr.h"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>

#include "package.h"
#include "endianness.h"
#include "errors.h"
#include "syscalls.h"

#if !defined(DGAMELAUNCH) && !defined(__ANDROID__) && !defined(DEBUG_DIAGNOSTICS)
#define DO_FSYNC
#endif

// debugging defines
#undef  FSCK_VERBOSE
#undef  COSTLY_ASSERTS
#undef  DEBUG_PACKAGE

static plen_t htole(plen_t x)
{
    if (sizeof(plen_t) == 4)
        return htole32(x);
    if (sizeof(plen_t) == 8)
        return htole64(x);
    die("unsupported plen_t size");
}

#ifdef DEBUG_PACKAGE
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) do {} while (0)
#endif

#define PACKAGE_VERSION 1
#define PACKAGE_MAGIC   0x53534344 /* "DCSS" */

struct file_header
{
    uint32_t magic;
    uint8_t version;
    char padding[3];
    plen_t start;
};

struct block_header
{
    plen_t len;
    plen_t next;
};

typedef map<string, plen_t> directory_t;
typedef pair<plen_t, plen_t> bm_p;
typedef map<plen_t, bm_p> bm_t;
typedef map<plen_t, plen_t> fb_t;

package::package(const char* file, bool writeable, bool empty)
  : n_users(0), dirty(false), aborted(false), tmp(false)
{
    dprintf("package: initializing file=\"%s\" rw=%d\n", file, writeable);
    ASSERT(writeable || !empty);
    filename = file;
    rw = writeable;

    if (empty)
    {
        fd = open_u(file, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
        if (fd == -1)
            sysfail("can't create save file (%s)", file);

        if (!lock_file(fd, true))
        {
            close(fd);
            sysfail("failed to lock newly created save (%s)", file);
        }

        dirty = true;
        file_len = sizeof(file_header);
    }
    else
    {
        fd = open_u(file, (writeable? O_RDWR : O_RDONLY) | O_BINARY, 0666);
        if (fd == -1)
            sysfail("can't open save file (%s)", file);

        try
        {
            if (!lock_file(fd, writeable))
                fail("Another game is already in progress using this save!");

            load();
        }
        catch (exception &e)
        {
            close(fd);
            throw;
        }
    }
}

package::package()
  : rw(true), n_users(0), dirty(false), aborted(false), tmp(true)
{
    dprintf("package: initializing tmp file\n");
    filename = "[tmp]";

    char file[7] = "XXXXXX";
    fd = mkstemp(file);
    if (fd == -1)
        sysfail("can't create temporary save file");

    ::unlink(file); // FIXME: won't work on Windows

    if (!lock_file(fd, true))
    {
        close(fd);
        sysfail("failed to lock newly created save (%s)", file);
    }

    dirty = true;
    file_len = sizeof(file_header);
}

void package::load()
{
    file_header head;
    ssize_t res = ::read(fd, &head, sizeof(file_header));
    if (res < 0)
        sysfail("error reading the save file (%s)", filename.c_str());
    if (!res || !(head.magic || head.version || head.padding[0]
                  || head.padding[1] || head.padding[2] || head.start))
    {
        corrupted("The save file (%s) is empty!", filename.c_str());
    }
    if (res != sizeof(file_header))
        corrupted("save file (%s) corrupted -- header truncated", filename.c_str());

    if (htole(head.magic) != PACKAGE_MAGIC)
    {
        corrupted("save file (%s) corrupted -- not a DCSS save file",
             filename.c_str());
    }
    off_t len = lseek(fd, 0, SEEK_END);
    if (len == -1)
        sysfail("save file (%s) is not seekable", filename.c_str());
    file_len = len;
    read_directory(htole(head.start), head.version);

    if (rw)
        load_traces();
}

void package::load_traces()
{
    ASSERT(!dirty);
    ASSERT(!n_users);
    if (directory.empty() || !block_map.empty())
        return;

    free_blocks[sizeof(file_header)] = file_len - sizeof(file_header);

    for (directory_t::iterator ch = directory.begin();
        ch != directory.end(); ++ch)
    {
        trace_chunk(ch->second);
    }
#ifdef COSTLY_ASSERTS
    // any inconsitency in the save is guaranteed to be already found
    // by this time -- this checks only for internal bugs
    fsck();
#endif
}

package::~package()
{
    dprintf("package: finalizing\n");
    ASSERT(!n_users || CrawlIsCrashing); // not merely aborted, there are
        // live pointers to us.  With normal stack unwinding, destructors
        // will make sure this never happens and this assert is good for
        // catching missing manual deletes.  The C++ exit handler is the
        // only place that can be legitimately call things in wrong order.

    if (rw && !aborted)
    {
        commit();
        if (ftruncate(fd, file_len))
            sysfail("failed to update save file");
    }

    // all errors here should be cached write errors
    if (fd != -1)
        if (close(fd) && !aborted)
            sysfail(rw ? "write error while saving"
                       : "can't close the save I've just read???");
    dprintf("package: closed\n");
}

void package::commit()
{
    ASSERT(rw);
    if (!dirty)
        return;
    ASSERT(!aborted);

#ifdef COSTLY_ASSERTS
    fsck();
#endif

    file_header head;
    head.magic = htole(PACKAGE_MAGIC);
    head.version = PACKAGE_VERSION;
    memset(&head.padding, 0, sizeof(head.padding));
    head.start = htole(write_directory());
#ifdef DO_FSYNC
    // We need a barrier before updating the link to point at the new directory.
    if (!tmp && fdatasync(fd))
        sysfail("flush error while saving");
#endif
    seek(0);
    if (write(fd, &head, sizeof(head)) != sizeof(head))
        sysfail("write error while saving");
#ifdef DO_FSYNC
    if (!tmp && fdatasync(fd))
        sysfail("flush error while saving");
#endif

    new_chunks.clear();
    collect_blocks();
    dirty = false;

#ifdef COSTLY_ASSERTS
    fsck();
#endif
}

void package::seek(plen_t to)
{
    ASSERT(!aborted);

    if (to > file_len)
        corrupted("save file corrupted -- invalid offset");
    if (lseek(fd, to, SEEK_SET) != (off_t)to)
        sysfail("failed to seek inside the save file");
}

chunk_writer* package::writer(const string name)
{
    return new chunk_writer(this, name);
}

chunk_reader* package::reader(const string name)
{
    directory_t::iterator ch = directory.find(name);
    if (ch == directory.end())
        return 0;
    return new chunk_reader(this, ch->second);
}

plen_t package::extend_block(plen_t at, plen_t size, plen_t by)
{
    // the header is not counted into the block's size, yet takes space
    size += sizeof(block_header);
    if (at + size == file_len)
    {
        // at the end of file
        file_len += by;
        return by;
    }

    fb_t::iterator bl = free_blocks.find(at + size);
    if (bl == free_blocks.end())
        return 0;

    plen_t free = bl->second;
    dprintf("reusing %u of %u at %u\n", by, free, bl->first);
    if (free <= by)
    {
        // we consume the entire block
        by = free;
        free_blocks.erase(bl);
        return by;
    }

    free_blocks.erase(bl);
    free_blocks[at + size + by] = free - by;

    return by;
}

plen_t package::alloc_block(plen_t &size)
{
    fb_t::iterator bl, best_big, best_small;
    plen_t bb_size = (plen_t)-1, bs_size = 0;
    for (bl = free_blocks.begin(); bl!=free_blocks.end(); ++bl)
    {
        if (bl->second < bb_size && bl->second >= size + sizeof(block_header))
            best_big = bl, bb_size = bl->second;
        // don't reuse very small blocks unless they're big enough
        else if (bl->second >= 16 && bl->second > bs_size)
            best_small = bl, bs_size = bl->second;
    }
    if (bb_size != (plen_t)-1)
        bl = best_big;
    else if (bs_size != 0)
        bl = best_small;
    else
    {
        plen_t at = file_len;

        file_len += sizeof(block_header) + size;
        return at;
    }

    plen_t at = bl->first;
    plen_t free = bl->second;
    dprintf("found a block for reuse at %u size %u\n", at, free);
    free_blocks.erase(bl);
    free -= sizeof(block_header);
    if (size > free)
        size = free;
    if ((free -= size))
        free_blocks[at + sizeof(block_header) + size] = free;

    return at;
}

void package::finish_chunk(const string name, plen_t at)
{
    free_chunk(name);
    directory[name] = at;
    new_chunks.insert(at);
    dirty = true;
}

void package::free_chunk(const string name)
{
    directory_t::iterator ci = directory.find(name);
    if (ci == directory.end())
        return;

    dprintf("freeing chunk(%s)\n", name.c_str());
    if (new_chunks.count(ci->second))
        free_block_chain(ci->second);
    else // can't free committed blocks yet
        unlinked_blocks.push_back(ci->second);

    dirty = true;
}

void package::delete_chunk(const string name)
{
    free_chunk(name);
    directory.erase(name);
}

plen_t package::write_directory()
{
    delete_chunk("");

    stringstream dir;
    for (directory_t::iterator i = directory.begin();
         i != directory.end(); ++i)
    {
        uint8_t name_len = i->first.length();
        dir.write((const char*)&name_len, sizeof(name_len));
        dir.write(&i->first[0], i->first.length());
        plen_t start = htole(i->second);
        dir.write((const char*)&start, sizeof(plen_t));
    }

    ASSERT(dir.str().size());
    dprintf("writing directory (%u bytes)\n", (unsigned int)dir.str().size());
    {
        chunk_writer dch(this, "");
        dch.write(&dir.str()[0], dir.str().size());
    }

    return directory[""];
}

void package::collect_blocks()
{
    for (ssize_t i = unlinked_blocks.size() - 1; i >= 0; --i)
    {
        plen_t at = unlinked_blocks[i];
        // Blocks may be re-added onto the list if they're in use.
        if (i != (ssize_t)unlinked_blocks.size() - 1)
            unlinked_blocks[i] = unlinked_blocks[unlinked_blocks.size() - 1];
        unlinked_blocks.pop_back();
        free_block_chain(at);
    }
}

void package::free_block_chain(plen_t at)
{
    if (reader_count.count(at))
    {
        dprintf("deleting an in-use chain at %d\n", at);
        unlinked_blocks.push_back(at);
        return;
    }

    dprintf("freeing an unlinked chain at %d\n", at);
    while (at)
    {
        bm_t::iterator bl = block_map.find(at);
        ASSERT(bl != block_map.end());
        dprintf("+- at %d size=%d+header\n", at, bl->second.first);
        free_block(at, bl->second.first + sizeof(block_header));
        at = bl->second.second;
        block_map.erase(bl);
    }
}

void package::free_block(plen_t at, plen_t size)
{
    ASSERT(at >= sizeof(file_header));
    ASSERT(at + size <= file_len);

    fb_t::iterator neigh;

    neigh = free_blocks.lower_bound(at);
    if (neigh != free_blocks.begin())
    {
        --neigh;
        ASSERT(neigh->first + neigh->second <= at);
        if (neigh->first + neigh->second == at)
        {
            // combine with the left neighbour
            at = neigh->first;
            size += neigh->second;
            free_blocks.erase(neigh);
        }
    }

    neigh = free_blocks.lower_bound(at);
    if (neigh != free_blocks.end())
    {
        ASSERT(neigh->first >= at + size);
        if (neigh->first == at + size)
        {
            // combine with the right neighbour
            size += neigh->second;
            free_blocks.erase(neigh);
        }
    }

    if (at + size == file_len)
        file_len -= size;
    else
        free_blocks[at] = size;
}

void package::fsck()
{
    fb_t  save_free_blocks = free_blocks;
    plen_t save_file_len = file_len;

#ifdef FSCK_VERBOSE
    printf("Fsck starting.  %u chunks, %u used blocks, %u free ones, file size %u\n",
           (unsigned int)directory.size(), (unsigned int)block_map.size(),
           (unsigned int)free_blocks.size(), file_len);

    for (fb_t::const_iterator bl = free_blocks.begin(); bl != free_blocks.end();
        ++bl)
    {
        printf("<at %u size %u>\n", bl->first, bl->second);
    }
#endif
    for (bm_t::const_iterator bl = block_map.begin(); bl != block_map.end();
        ++bl)
    {
#ifdef FSCK_VERBOSE
        printf("[at %u size %u+header]\n", bl->first, bl->second.first);
#endif
        free_block(bl->first, bl->second.first + sizeof(block_header));
    }
    // after freeing everything, the file should be empty
    ASSERT(free_blocks.empty());
    ASSERT(file_len == sizeof(file_header));

    free_blocks = save_free_blocks;
    file_len = save_file_len;
}

struct dir_entry0
{
    char name[sizeof(plen_t)];
    plen_t start;
};

void package::read_directory(plen_t start, uint8_t version)
{
    ASSERT(directory.empty());
    directory[""] = start;

    dprintf("package: reading directory\n");
    chunk_reader rd(this, start);

    switch (version)
    {
    case 0:
        dir_entry0 ch0;
        while (plen_t res = rd.read(&ch0, sizeof(dir_entry0)))
        {
            if (res != sizeof(dir_entry0))
                corrupted("save file corrupted -- truncated directory");
            string chname(ch0.name, 4);
            chname.resize(strlen(chname.c_str()));
            directory[chname] = htole(ch0.start);
            dprintf("* %s\n", chname.c_str());
        }
        break;
    case 1:
        uint8_t name_len;
        plen_t bstart;
        while (plen_t res = rd.read(&name_len, sizeof(name_len)))
        {
            if (res != sizeof(name_len))
                corrupted("save file corrupted -- truncated directory");
            string chname;
            chname.resize(name_len);
            if (rd.read(&chname[0], name_len) != name_len)
                corrupted("save file corrupted -- truncated directory");
            if (rd.read(&bstart, sizeof(bstart)) != sizeof(bstart))
                corrupted("save file corrupted -- truncated directory");
            directory[chname] = htole(bstart);
            dprintf("* %s\n", chname.c_str());
        }
        break;
    default:
        corrupted("save file (%s) uses an unknown format %u", filename.c_str(),
             version);
    }
}

bool package::has_chunk(const string name)
{
    return !name.empty() && directory.find(name) != directory.end();
}

vector<string> package::list_chunks()
{
    vector<string> list;
    list.reserve(directory.size());
    for (directory_t::iterator i = directory.begin();
         i != directory.end(); ++i)
    {
        if (!i->first.empty())
            list.push_back(i->first);
    }
    return list;
}

void package::trace_chunk(plen_t start)
{
    while (start)
    {
        block_header bl;
        seek(start);
        ssize_t res = ::read(fd, &bl, sizeof(block_header));
        if (res < 0)
            sysfail("error reading the save file");
        if (res != sizeof(block_header))
            corrupted("save file corrupted -- block past eof");

        plen_t len  = htole(bl.len);
        plen_t next = htole(bl.next);
        plen_t end  = start + len + sizeof(block_header);
        dprintf("{at %u size %u+header}\n", start, len);

        fb_t::iterator sp = free_blocks.upper_bound(start);
        if (sp == free_blocks.begin())
            corrupted("save file corrupted -- overlapping blocks");
        --sp;
        plen_t sp_start = sp->first;
        plen_t sp_size  = sp->second;
        if (sp_start > start || sp_start + sp_size < end)
            corrupted("save file corrupted -- overlapping blocks");
        free_blocks.erase(sp);
        if (sp_start < start)
            free_blocks[sp_start] = start - sp_start;
        if (sp_start + sp_size > end)
            free_blocks[end] = sp_start + sp_size - end;

        block_map[start] = bm_p(len, next);
        start = next;
    }
}

void package::abort()
{
    // Disable any further operations, allow a shutdown.  All errors past
    // this point are ignored (assuming we already failed).  All writes since
    // the last commit() are lost.
    aborted = true;
}

void package::unlink()
{
    abort();
    close(fd);
    fd = -1;
    ::unlink_u(filename.c_str());
}

// the amount of free space not at the end of file
plen_t package::get_slack()
{
    load_traces();

    plen_t slack = 0;
    for (fb_t::iterator bl = free_blocks.begin(); bl!=free_blocks.end(); ++bl)
        slack += bl->second;
    return slack;
}

plen_t package::get_chunk_fragmentation(const string name)
{
    load_traces();
    ASSERT(directory.find(name) != directory.end()); // not has_chunk(), "" is valid
    plen_t frags = 0;
    plen_t at = directory[name];
    while (at)
    {
        bm_t::iterator bl = block_map.find(at);
        ASSERT(bl != block_map.end());
        frags ++;
        at = bl->second.second;
    }
    return frags;
}

plen_t package::get_chunk_compressed_length(const string name)
{
    load_traces();
    ASSERT(directory.find(name) != directory.end()); // not has_chunk(), "" is valid
    plen_t len = 0;
    plen_t at = directory[name];
    while (at)
    {
        bm_t::iterator bl = block_map.find(at);
        ASSERT(bl != block_map.end());
        len += bl->second.first;
        at = bl->second.second;
    }
    return len;
}

chunk_writer::chunk_writer(package *parent, const string _name)
    : first_block(0), cur_block(0), block_len(0)
{
    ASSERT(parent);
    ASSERT(!parent->aborted);

    // If you need more, please change {read,write}_directory().
    ASSERT(MAX_CHUNK_NAME_LENGTH < 256);
    ASSERT(_name.length() < MAX_CHUNK_NAME_LENGTH);

    dprintf("chunk_writer(%s): starting\n", _name.c_str());
    pkg = parent;
    pkg->n_users++;
    name = _name;

#ifdef USE_ZLIB
    zs.data_type = Z_BINARY;
    zs.zalloc    = 0;
    zs.zfree     = 0;
    zs.opaque    = Z_NULL;
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION))
        fail("save file compression failed during init: %s", zs.msg);
#define ZB_SIZE 32768
    zs.next_out  = z_buffer = (Bytef*)malloc(ZB_SIZE);
    zs.avail_out = ZB_SIZE;
#endif
}

chunk_writer::~chunk_writer()
{
    dprintf("chunk_writer(%s): closing\n", name.c_str());

    ASSERT(pkg->n_users > 0);
    pkg->n_users--;
    if (pkg->aborted)
    {
#ifdef USE_ZLIB
        // ignore errors, they're not relevant anymore
        deflateEnd(&zs);
        free(z_buffer);
#endif
        return;
    }

#ifdef USE_ZLIB
    zs.avail_in = 0;
    int res;
    do
    {
        res = deflate(&zs, Z_FINISH);
        if (res != Z_STREAM_END && res != Z_OK && res != Z_BUF_ERROR)
            fail("save file compression failed: %s", zs.msg);
        raw_write(z_buffer, zs.next_out - z_buffer);
        zs.next_out = z_buffer;
        zs.avail_out = ZB_SIZE;
    } while (res != Z_STREAM_END);
    if (deflateEnd(&zs) != Z_OK)
        fail("save file compression failed during clean-up: %s", zs.msg);
    free(z_buffer);
#endif
    if (cur_block)
        finish_block(0);
    pkg->finish_chunk(name, first_block);
}

void chunk_writer::raw_write(const void *data, plen_t len)
{
    while (len > 0)
    {
        plen_t space = pkg->extend_block(cur_block, block_len, len);
        if (!space)
        {
            plen_t next_block = pkg->alloc_block(space = len);
            ASSERT(space > 0);
            if (cur_block)
                finish_block(next_block);
            cur_block = next_block;
            if (!first_block)
                first_block = next_block;
            block_len = 0;
        }

        pkg->seek(cur_block + block_len + sizeof(block_header));
        if (::write(pkg->fd, data, space) != (ssize_t)space)
            sysfail("write error while saving");
        data = (char*)data + space;
        block_len += space;
        len -= space;
    }
}

void chunk_writer::finish_block(plen_t next)
{
    block_header head;
    head.len = htole(block_len);
    head.next = htole(next);

    pkg->seek(cur_block);
    if (::write(pkg->fd, &head, sizeof(head)) != sizeof(head))
        sysfail("write error while saving");

    pkg->block_map[cur_block] = bm_p(block_len, next);
}

void chunk_writer::write(const void *data, plen_t len)
{
    ASSERT(data);
    ASSERT(!pkg->aborted);

#ifdef USE_ZLIB
    zs.next_in  = (Bytef*)data;
    zs.avail_in = len;
    while (zs.avail_in)
    {
        if (!zs.avail_out)
        {
            raw_write(z_buffer, zs.next_out - z_buffer);
            zs.next_out  = z_buffer;
            zs.avail_out = ZB_SIZE;
        }
        // we don't allow Z_BUF_ERROR, so it's fatal for us
        if (deflate(&zs, Z_NO_FLUSH) != Z_OK)
            fail("save file compression failed: %s", zs.msg);
    }
#else
    raw_write(data, len);
#endif
}

void chunk_reader::init(plen_t start)
{
    ASSERT(!pkg->aborted);
    pkg->n_users++;
    pkg->reader_count[start]++;
    first_block = next_block = start;
    block_left = 0;

#ifdef USE_ZLIB
    if (!start)
        corrupted("save file corrupted -- zlib header missing");

    zs.zalloc    = 0;
    zs.zfree     = 0;
    zs.opaque    = Z_NULL;
    zs.next_in   = Z_NULL;
    zs.avail_in  = 0;
    if (inflateInit(&zs))
        fail("save file decompression failed during init: %s", zs.msg);
    eof = false;
#endif
}

chunk_reader::chunk_reader(package *parent, plen_t start)
{
    ASSERT(parent);
    dprintf("chunk_reader[%u]: starting\n", start);
    pkg = parent;
    init(start);
}

chunk_reader::chunk_reader(package *parent, const string _name)
{
    ASSERT(parent);
    if (!parent->has_chunk(_name))
        corrupted("save file corrupted -- chunk \"%s\" missing", _name.c_str());
    dprintf("chunk_reader(%s): starting\n", _name.c_str());
    pkg = parent;
    init(parent->directory[_name]);
}

chunk_reader::~chunk_reader()
{
    dprintf("chunk_reader: closing\n");

#ifdef USE_ZLIB
    if (inflateEnd(&zs) != Z_OK)
        fail("save file decompression failed during clean-up: %s", zs.msg);
#endif
    ASSERT(pkg->reader_count[first_block] > 0);
    if (!--pkg->reader_count[first_block])
        pkg->reader_count.erase(first_block);
    ASSERT(pkg->n_users > 0);
    pkg->n_users--;
}

plen_t chunk_reader::raw_read(void *data, plen_t len)
{
    void *buf = data;
    while (len)
    {
        if (!block_left)
        {
            if (!next_block)
                return (char*)buf - (char*)data;

            block_header bl;
            pkg->seek(next_block);
            ssize_t res = ::read(pkg->fd, &bl, sizeof(block_header));
            if (res < 0)
                sysfail("error reading the save file");
            if (res != sizeof(block_header))
                corrupted("save file corrupted -- block past eof");

            off = next_block + sizeof(block_header);
            block_left = htole(bl.len);
            next_block = htole(bl.next);
            // This reeks of on-disk corruption (zeroed data).
            if (!block_left)
                corrupted("save file corrupted -- empty block");
        }
        else
            pkg->seek(off);

        plen_t s = len;
        if (s > block_left)
            s = block_left;
        ssize_t res = ::read(pkg->fd, buf, s);
        if (res < 0)
            sysfail("error reading the save file");
        if ((plen_t)res != s)
            corrupted("save file corrupted -- block past eof");

        buf = (char*)buf + s;
        off += s;
        len -= s;
        block_left -= s;
    }

    return (char*)buf - (char*)data;
}

plen_t chunk_reader::read(void *data, plen_t len)
{
    ASSERT(data);
    if (pkg->aborted)
        return 0;

#ifdef USE_ZLIB
    if (!len)
        return 0;
    if (eof)
        return 0;

    zs.next_out  = (Bytef*)data;
    zs.avail_out = len;
    while (zs.avail_out)
    {
        if (!zs.avail_in)
        {
            zs.next_in  = z_buffer;
            zs.avail_in = raw_read(z_buffer, sizeof(z_buffer));
            if (!zs.avail_in)
                corrupted("save file corrupted -- block truncated");
        }
        int res = inflate(&zs, Z_NO_FLUSH);
        if (res == Z_STREAM_END)
        {
            eof = true;
            return zs.next_out - (Bytef*)data;
        }
        if (res != Z_OK)
            corrupted("save file decompression failed: %s", zs.msg);
    }
    return zs.next_out - (Bytef*)data;
#else
    return raw_read(data, len);
#endif
}

void chunk_reader::read_all(vector<char> &data)
{
#define SPACE 1024
    plen_t s, at;
    do
    {
        at = data.size();
        data.resize(at + SPACE);
        s = read(&data[at], SPACE);
    } while (s == SPACE);
    data.resize(at + s);
#undef SPACE
}
