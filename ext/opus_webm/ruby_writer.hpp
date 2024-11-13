#ifndef RUBY_WRITER_HPP
#define RUBY_WRITER_HPP

#include <ruby/version.h>
#include <ruby.h>
#include "mkvmuxer/mkvwriter.h"

class RubyWriter : public mkvmuxer::IMkvWriter {
public:
    explicit RubyWriter(VALUE io);
    ~RubyWriter();

    virtual int32_t Write(const void* buffer, uint32_t length);
    virtual int64_t Position() const;
    virtual int32_t Position(int64_t position);
    virtual bool Seekable() const;
    virtual void ElementStartNotify(uint64_t element_id, int64_t position);

private:
    VALUE io_;
    int64_t position_;
};

#endif // RUBY_WRITER_HPP
