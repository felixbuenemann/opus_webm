#ifndef RUBY_WRITER_HPP
#define RUBY_WRITER_HPP

#include <ruby/version.h>
#include <ruby.h>
#include "mkvmuxer/mkvwriter.h"

class RubyWriter : public mkvmuxer::IMkvWriter {
public:
    explicit RubyWriter(VALUE io);
    ~RubyWriter();

    virtual mkvmuxer::int32 Write(const void* buffer, mkvmuxer::uint32 length);
    virtual mkvmuxer::int64 Position() const;
    virtual mkvmuxer::int32 Position(mkvmuxer::int64 position);
    virtual bool Seekable() const;
    virtual void ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position);

private:
    VALUE io_;
    mkvmuxer::int64 position_;
};

#endif // RUBY_WRITER_HPP
