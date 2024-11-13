#include "ruby_writer.hpp"

RubyWriter::RubyWriter(VALUE io) : io_(io), position_(0) {
}

RubyWriter::~RubyWriter() {
    rb_funcall(io_, rb_intern("flush"), 0);
}

int32_t RubyWriter::Write(const void* buffer, uint32_t length) {
    VALUE str = rb_str_new(static_cast<const char*>(buffer), length);
    rb_funcall(io_, rb_intern("write"), 1, str);
    position_ += length;
    return 0;
}

int64_t RubyWriter::Position() const {
    VALUE pos = rb_funcall(io_, rb_intern("tell"), 0);
    return NUM2LL(pos);
}

int32_t RubyWriter::Position(int64_t position) {
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

void RubyWriter::ElementStartNotify(uint64_t element_id, int64_t position) {
}
