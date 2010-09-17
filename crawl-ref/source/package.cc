/*
 *  File:       package.cc
 *  Summary:    A container for chunks that make up the save file.
 *  Written by: Adam Borowski
 */

/*
Guarantees:
* A crash at any moment may not cause corruption -- the save will return to
  the exact state it had at the last commit().

Caveats/issues:
* Before the first commit, the save is "not a valid DCSS save file",
  how should we handle this?
* Benchmarked on random chunks of size 2^random2(X) * frandom(1),
  naive reusing of the first slab of free space produces less waste than
  best fit.  I don't fully understand why, but for now, let's use that.
* Unless DO_FSYNC is defined, crashes that put down the operating system
  may break the consistency guarantee.
* A commit() will break readers who read a chunk that was deleted or
  overwritten.  Not that it's a sane thing to do...  Writers don't have
  any such limitations, an uncompleted write will be not committed yet
  but won't be corrupted.
* Readers ignore uncompleted writes; completed but not committed ones will
  be available immediately -- yet a crash will lose them.
*/

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "AppHdr.h"
#include "package.h"
#include "endianness.h"
#include "errors.h"
#include "syscalls.h"

#undef  FSCK_VERBOSE
#undef  COSTLY_ASSERTS
#undef  DO_FSYNC
#undef  DEBUG_PACKAGE

static len_t htole(len_t x)
{
    if (sizeof(len_t) == 4)
        return htole32(x);
    if (sizeof(len_t) == 8)
        return htole64(x);
    ASSERT(!"unsupported len_t size");
}

#ifdef DEBUG_PACKAGE
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) do {} while(0)
#endif

#define PACKAGE_VERSION 0
#define PACKAGE_MAGIC   0x53534344 /* "DCSS" */

struct file_header
{
    int32_t magic;
    int8_t version;
    char padding[3];
    len_t start;
};

struct block_header
{
    len_t len;
    len_t next;
};

struct dir_entry
{
    char name[sizeof(len_t)];
    len_t start;
};

typedef std::map<std::string, len_t> directory_t;
typedef std::pair<len_t, len_t> bm_p;
typedef std::map<len_t, bm_p> bm_t;
typedef std::map<len_t, len_t> fb_t;

package::package(const char* file, bool writeable, bool empty)
  : n_users(0), dirty(false), aborted(false)
{
    dprintf("package: initializing file=\"%s\" rw=%d\n", file, writeable);
    ASSERT(writeable || !empty);
    rw = writeable;

    if (empty)
    {
        fd = open(file, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
        if (fd == -1)
            sysfail("can't create save file (%s)", file);

        if (!lock_file(fd, true))
            sysfail("failed to lock newly created save (%s)", file);

        dirty = true;
        file_len = sizeof(file_header);
    }
    else
    {
        fd = open(file, (writeable? O_RDWR : O_RDONLY) | O_BINARY, 0666);
        if (fd == -1)
            sysfail("can't open save file (%s)", file);

        if (!lock_file(fd, writeable))
            fail("Another game is already in progress using this save!");

        file_header head;
        ssize_t res = ::read(fd, &head, sizeof(file_header));
        if (res < 0)
            sysfail("error reading the save file (%s)", file);
        if (!res || !(head.magic || head.version || head.padding[0]
                      || head.padding[1] || head.padding[2] || head.start))
        {
            fail("The save file (%s) is empty!", file);
        }
        if (res != sizeof(file_header))
            fail("save file (%s) corrupted -- header truncated", file);

        if (htole(head.magic) != PACKAGE_MAGIC)
            fail("save file (%s) corrupted -- not a DCSS save file", file);
        if (head.version != PACKAGE_VERSION)
            fail("save file (%s) uses an unknown format %u", file, head.version);
        off_t len = lseek(fd, 0, SEEK_END);
        if (len == -1)
            sysfail("save file (%s) is not seekable", file);
        file_len = len;
        read_directory(htole(head.start));

        if (writeable)
        {
            free_blocks[sizeof(file_header)] = file_len - sizeof(file_header);

            for(directory_t::iterator ch = directory.begin();
                ch != directory.end(); ch++)
            {
                trace_chunk(ch->second);
            }
#ifdef COSTLY_ASSERTS
            // any inconsitency in the save is guaranteed to be already found
            // by this time -- this checks only for internal bugs
            fsck();
#endif
        }
    }
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
    head.start = write_directory();
#ifdef DO_FSYNC
    // We need a barrier before updating the link to point at the new directory.
    if (fdatasync(fd))
        sysfail("flush error while saving");
#endif
    seek(0);
    if (write(fd, &head, sizeof(head)) != sizeof(head))
        sysfail("write error while saving");
#ifdef DO_FSYNC
    if (fdatasync(fd))
        sysfail("flush error while saving");
#endif

    collect_blocks();
    dirty = false;

#ifdef COSTLY_ASSERTS
    fsck();
#endif
}

void package::seek(len_t to)
{
    ASSERT(!aborted);

    if (to > file_len)
        fail("save file corrupted -- invalid offset");
    if (lseek(fd, to, SEEK_SET) != (off_t)to)
        sysfail("failed to seek inside the save file");
}

chunk_writer* package::writer(const std::string name)
{
    return new chunk_writer(this, name);
}

chunk_reader* package::reader(const std::string name)
{
    directory_t::iterator ch = directory.find(name);
    if (ch == directory.end())
        return 0;
    return new chunk_reader(this, ch->second);
}

len_t package::extend_block(len_t at, len_t size, len_t by)
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

    len_t free = bl->second;
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

len_t package::alloc_block()
{
    for(fb_t::iterator bl = free_blocks.begin(); bl!=free_blocks.end(); bl++)
    {
        // don't reuse very small blocks
        if (bl->second < 16)
            continue;

          len_t at = bl->first;
          len_t free = bl->second;
          dprintf("found a block for reuse at %u size %u\n", at, free);
          free_blocks.erase(bl);
          free_blocks[at + sizeof(block_header)] = free - sizeof(block_header);

          return at;
    }
    len_t at = file_len;

    file_len += sizeof(block_header);
    return at;
}

void package::finish_chunk(const std::string name, len_t at)
{
    free_chunk(name);
    directory[name] = at;
    dirty = true;
}

void package::free_chunk(const std::string name)
{
    directory_t::iterator ci = directory.find(name);
    if (ci == directory.end())
        return;

    dprintf("freeing chunk(%s)\n", name.c_str());
    unlinked_blocks.push(ci->second);

    dirty = true;
}

void package::delete_chunk(const std::string name)
{
    free_chunk(name);
    directory.erase(name);
}

len_t package::write_directory()
{
    delete_chunk("");

    std::vector<dir_entry> dir;
    dir.reserve(directory.size());
    for (directory_t::iterator i = directory.begin();
         i != directory.end(); i++)
    {
        dir_entry ch;
        // probably the only case ever when strncpy() does exactly the
        // right thing
        strncpy(ch.name, i->first.c_str(), sizeof(ch.name));
        ch.start = htole(i->second);
        dir.push_back(ch);
    }

    ASSERT(dir.size());
    dprintf("writing directory of size %u*%u\n", (unsigned int)dir.size(),
            (unsigned int)sizeof(dir_entry));
    {
        chunk_writer dch(this, "");
        dch.write(&dir.front(), dir.size() * sizeof(dir_entry));
    }

    return directory[""];
}

void package::collect_blocks()
{
    while (!unlinked_blocks.empty())
    {
        len_t at = unlinked_blocks.top();
        unlinked_blocks.pop();
        dprintf("freeing an unlinked chain at %d\n", at);
        while(at)
        {
            bm_t::iterator bl = block_map.find(at);
            ASSERT(bl != block_map.end());
            dprintf("+- at %d size=%d+header\n", at, bl->second.first);
            free_block(at, bl->second.first + sizeof(block_header));
            at = bl->second.second;
            block_map.erase(bl);
        }
    }
}

void package::free_block(len_t at, len_t size)
{
    ASSERT(at >= sizeof(file_header));
    ASSERT(at + size <= file_len);

    fb_t::iterator neigh;

    neigh = free_blocks.lower_bound(at);
    if (neigh != free_blocks.begin())
    {
        neigh--;
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
    len_t save_file_len = file_len;

#ifdef FSCK_VERBOSE
    printf("Fsck starting.  %u chunks, %u used blocks, %u free ones, file size %u\n",
           (unsigned int)directory.size(), (unsigned int)block_map.size(),
           (unsigned int)free_blocks.size(), file_len);

    for(fb_t::const_iterator bl = free_blocks.begin(); bl != free_blocks.end();
        bl++)
    {
        printf("<at %u size %u>\n", bl->first, bl->second);
    }
#endif
    for(bm_t::const_iterator bl = block_map.begin(); bl != block_map.end();
        bl++)
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

void package::read_directory(len_t start)
{
    ASSERT(directory.empty());
    directory[""] = start;

    dprintf("package: reading directory\n");
    chunk_reader *rd = new chunk_reader(this, start);

    dir_entry ch;
    while(len_t res = rd->read(&ch, sizeof(dir_entry)))
    {
        if (res != sizeof(dir_entry))
            fail("save file corrupted -- truncated directory");
        std::string chname(ch.name, 4);
        chname.resize(strlen(chname.c_str()));
        directory[chname] = htole(ch.start);
        dprintf("* %s\n", chname.c_str());
    }

    delete rd;
}

bool package::has_chunk(const std::string name)
{
    return !name.empty() && directory.find(name) != directory.end();
}

std::vector<std::string> package::list_chunks()
{
    std::vector<std::string> list;
    list.reserve(directory.size());
    for (directory_t::iterator i = directory.begin();
         i != directory.end(); i++)
    {
        if (!i->first.empty())
            list.push_back(i->first);
    }
    return list;
}

void package::trace_chunk(len_t start)
{
    while(start)
    {
        block_header bl;
        seek(start);
        ssize_t res = ::read(fd, &bl, sizeof(block_header));
        if (res < 0)
            sysfail("error reading the save file");
        if (res != sizeof(block_header))
            fail("save file corrupted -- block past eof");

        len_t len  = htole(bl.len);
        len_t next = htole(bl.next);
        len_t end  = start + len + sizeof(block_header);
        dprintf("{at %u size %u+header}\n", start, len);

        fb_t::iterator sp = free_blocks.upper_bound(start);
        if (sp == free_blocks.begin())
            fail("save file corrupted -- overlapping blocks");
        sp--;
        len_t sp_start = sp->first;
        len_t sp_size  = sp->second;
        if (sp_start > start || sp_start + sp_size < end)
            fail("save file corrupted -- overlapping blocks");
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


chunk_writer::chunk_writer(package *parent, const std::string _name)
    : first_block(0), cur_block(0), block_len(0)
{
    ASSERT(parent);
    ASSERT(!parent->aborted);

    /*
    The save format is currently limited to 4 character names for simplicity
    (we use 3 max).  If you want to extend this, please implement marshalling
    of arbitrary strings in {read,write}_directory().
    */
    ASSERT(_name.size() <= 4);

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
        return;

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
        zs.avail_out = sizeof(ZB_SIZE);
    } while (res != Z_STREAM_END);
    if (deflateEnd(&zs) != Z_OK)
        fail("save file compression failed during clean-up: %s", zs.msg);
    free(z_buffer);
#endif
    if (cur_block)
        finish_block(0);
    pkg->finish_chunk(name, first_block);
}

void chunk_writer::raw_write(const void *data, len_t len)
{
    while (len > 0)
    {
        int space = pkg->extend_block(cur_block, block_len, len);
        if (!space)
        {
            len_t next_block = pkg->alloc_block();
            space = pkg->extend_block(next_block, 0, len);
            ASSERT(space > 0);
            if (cur_block)
                finish_block(next_block);
            cur_block = next_block;
            if (!first_block)
                first_block = next_block;
            block_len = 0;
        }

        pkg->seek(cur_block + block_len + sizeof(block_header));
        if (::write(pkg->fd, data, space) != space)
            sysfail("write error while saving");
        data = (char*)data + space;
        block_len += space;
        len -= space;
    }
}

void chunk_writer::finish_block(len_t next)
{
    block_header head;
    head.len = htole(block_len);
    head.next = htole(next);

    pkg->seek(cur_block);
    if (::write(pkg->fd, &head, sizeof(head)) != sizeof(head))
        sysfail("write error while saving");

    pkg->block_map[cur_block] = bm_p(block_len, next);
}

void chunk_writer::write(const void *data, len_t len)
{
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

void chunk_reader::init(len_t start)
{
    ASSERT(!pkg->aborted);
    pkg->n_users++;
    next_block = start;
    block_left = 0;

#ifdef USE_ZLIB
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

chunk_reader::chunk_reader(package *parent, len_t start)
{
    ASSERT(parent);
    dprintf("chunk_reader[%u]: starting\n", start);
    pkg = parent;
    init(start);
}

chunk_reader::chunk_reader(package *parent, const std::string _name)
{
    ASSERT(parent);
    if (!parent->has_chunk(_name))
        fail("save file corrupted -- chunk \"%s\" missing", _name.c_str());
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
    ASSERT(pkg->n_users > 0);
    pkg->n_users--;
}

len_t chunk_reader::raw_read(void *data, len_t len)
{
    void *buf = data;
    while(len)
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
                fail("save file corrupted -- block past eof");

            off = next_block + sizeof(block_header);
            block_left = htole(bl.len);
            next_block = htole(bl.next);
        }
        else
            pkg->seek(off);

        len_t s = len;
        if (s > block_left)
            s = block_left;
        ssize_t res = ::read(pkg->fd, buf, s);
        if (res < 0)
            sysfail("error reading the save file");
        if ((len_t)res != s)
            fail("save file corrupted -- block past eof");

        buf = (char*)buf + s;
        off += s;
        len -= s;
        block_left -= s;
    }

    return (char*)buf - (char*)data;
}

len_t chunk_reader::read(void *data, len_t len)
{
    if (pkg->aborted)
        return 0;

#ifdef USE_ZLIB
    if (!len)
        return 0;
    if (eof)
        return 0;

    zs.next_out  = (Bytef*)data;
    zs.avail_out = len;
    while(zs.avail_out)
    {
        if (!zs.avail_in)
        {
            zs.next_in  = z_buffer;
            zs.avail_in = raw_read(z_buffer, sizeof(z_buffer));
            if (!zs.avail_in)
                fail("save file corrupted -- block truncated");
        }
        int res = inflate(&zs, Z_NO_FLUSH);
        if (res == Z_STREAM_END)
        {
            eof = true;
            return zs.next_out - (Bytef*)data;
        }
        if (res != Z_OK)
            fail("save file decompression failed: %s", zs.msg);
    }
    return zs.next_out - (Bytef*)data;
#else
    return raw_read(data, len);
#endif
}

void chunk_reader::read_all(std::vector<char> &data)
{
#define SPACE 1024
    len_t s, at;
    do
    {
        at = data.size();
        data.resize(at + SPACE);
        s = read(&data[at], SPACE);
    } while (s == SPACE);
    data.resize(at + s);
#undef SPACE
}
