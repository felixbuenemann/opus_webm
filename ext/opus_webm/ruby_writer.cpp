#include "ruby_writer.hpp"

RubyWriter::RubyWriter(VALUE io) : io_(io), position_(0) {
}

RubyWriter::~RubyWriter() {
    rb_funcall(io_, rb_intern("flush"), 0);
}

mkvmuxer::int32 RubyWriter::Write(const void* buffer, mkvmuxer::uint32 length) {
    VALUE str = rb_str_new(static_cast<const char*>(buffer), length);
    VALUE result = rb_funcall(io_, rb_intern("write"), 1, str);
    if (NIL_P(result)) {
        return -1;
    }
    position_ += length;
    return 0;
}

mkvmuxer::int64 RubyWriter::Position() const {
    VALUE pos = rb_funcall(io_, rb_intern("tell"), 0);
    return NUM2LL(pos);
}

mkvmuxer::int32 RubyWriter::Position(mkvmuxer::int64 position) {
    VALUE result = rb_funcall(io_, rb_intern("seek"), 2, 
                            LL2NUM(position), INT2FIX(SEEK_SET));
    if (NIL_P(result)) {
        return -1;
    }
    position_ = position;
    return 0;
}

bool RubyWriter::Seekable() const {
    return rb_respond_to(io_, rb_intern("seek")) && rb_respond_to(io_, rb_intern("tell"));
}

void RubyWriter::ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position) {
}
