#include "AppHdr.h"

#include "message.h"

#include <streambuf>
#include <iostream>

namespace msg
{
    mpr_stream_buf msbuf(MSGCH_PLAIN);
    std::ostream stream(&msbuf);
    std::vector<std::ostream*> stream_ptrs;
    std::vector<mpr_stream_buf*> stream_buffers;

    std::ostream& streams(msg_channel_type chan)
    {
        ASSERT(chan >= 0
               && static_cast<unsigned int>(chan) < stream_ptrs.size());
        return *stream_ptrs[chan];
    }

    void initialise_mpr_streams()
    {
        for (int i = 0; i < NUM_MESSAGE_CHANNELS; ++i)
        {
            mpr_stream_buf* pmsb =
                new mpr_stream_buf(static_cast<msg_channel_type>(i));
            std::ostream* pos = new std::ostream(pmsb);
            (*pos) << std::nounitbuf;
            stream_ptrs.push_back(pos);
            stream_buffers.push_back(pmsb);
        }
        stream << std::nounitbuf;
    }

    void deinitialise_mpr_streams()
    {
        for (unsigned int i = 0; i < stream_ptrs.size(); ++i)
            delete stream_ptrs[i];
        stream_ptrs.clear();
        for (unsigned int i = 0; i < stream_buffers.size(); ++i)
            delete stream_buffers[i];
        stream_buffers.clear();
    }


    setparam::setparam(int param)
    {
        m_param = param;
    }

    mpr_stream_buf::mpr_stream_buf(msg_channel_type chan) :
        internal_count(0), param(0), muted(false), channel(chan)
    {}

    void mpr_stream_buf::set_param(int p)
    {
        this->param = p;
    }

    void mpr_stream_buf::set_muted(bool m)
    {
        this->muted = m;
    }

    // again, can be improved
    int mpr_stream_buf::overflow(int c)
    {
        if (muted)
            return 0;

        if (c == '\n')
        {
            // null-terminate and print the string
            internal_buf[internal_count] = 0;
            mpr(internal_buf, channel, param);

            internal_count = 0;

            // reset to defaults (param changing isn't sticky)
            set_param(0);
        }
        else
            internal_buf[internal_count++] = c;

        if (internal_count + 3 > INTERNAL_LENGTH)
        {
            mpr("oops, hit overflow", MSGCH_ERROR);
            internal_count = 0;
            return std::streambuf::traits_type::eof();
        }
        return 0;
    }
}

std::ostream& operator<<(std::ostream& os, const msg::setparam& sp)
{
    msg::mpr_stream_buf* ps = dynamic_cast<msg::mpr_stream_buf*>(os.rdbuf());
    ps->set_param(sp.m_param);
    return os;
}
