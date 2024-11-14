# include <unistd.h>
# include <fcntl.h>
# include <ruby/version.h>
# include <ruby.h>
# include <ruby/io.h>
# include <ogg/ogg.h>
# include <mkvmuxer/mkvmuxer.h>
# include <mkvmuxer/mkvwriter.h>
# include "ruby_writer.hpp"

#define BUFFER_SIZE 4096

// Function to parse the Opus packet duration
static int GetOpusPacketDuration(const uint8_t* data, size_t size) {
    if (size < 1) return -1;

    const uint8_t toc = data[0];
    const uint8_t config = toc >> 3;
    const uint8_t frame_count_code = toc & 0x03;

    int frame_size = 0;
    int num_frames = 0;

    if (config < 12) {
        static const int frame_sizes[12] = {
            480, 960, 1920, 2880, 480, 960, 1920, 2880,
            480, 960, 1920, 2880
        };
        frame_size = frame_sizes[config % 12];
    } else if (config < 16) {
        frame_size = 120 << (config & 0x01);
    } else if (config < 20) {
        frame_size = 240 << (config & 0x03);
    } else {
        return -1;
    }

    if (frame_count_code == 0) {
        num_frames = 1;
    } else if (frame_count_code == 1 || frame_count_code == 2) {
        num_frames = 2;
    } else {
        if (size < 2) return -1;
        num_frames = data[1] & 0x3F;
    }

    return frame_size * num_frames;
}

static VALUE opus_webm_convert(VALUE self, VALUE input, VALUE output) {
    // Setup input
    int input_fd = -1;
    rb_io_t* input_fptr = NULL;
    bool input_is_io = false;

    if (RB_TYPE_P(input, T_STRING)) {
        input_fd = open(StringValueCStr(input), O_RDONLY);
        if (input_fd < 0) {
            rb_sys_fail("open");
        }
    } else {
        VALUE io = rb_io_check_io(input);
        if (NIL_P(io)) {
            rb_raise(rb_eTypeError, "expected IO object");
        }
        input_fd = NUM2INT(rb_funcall(io, rb_intern("fileno"), 0));
        input_is_io = true;
    }

    // Initialize Ogg structures
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;

    ogg_sync_init(&oy);

    // Setup output
    mkvmuxer::MkvWriter std_writer;
    std::unique_ptr<RubyWriter> ruby_writer;
    mkvmuxer::IMkvWriter* writer = nullptr;

    if (RB_TYPE_P(output, T_STRING)) {
        if (!std_writer.Open(StringValueCStr(output))) {
            rb_raise(rb_eIOError, "Could not open output file");
        }
        writer = &std_writer;
    } else {
        ruby_writer.reset(new RubyWriter(output));
        writer = ruby_writer.get();
    }

    // Initialize WebM segment
    mkvmuxer::Segment segment;
    if (!segment.Init(writer)) {
        rb_raise(rb_eRuntimeError, "Could not initialize WebM segment");
    }

    // Set up segment info
    mkvmuxer::SegmentInfo* info = segment.GetSegmentInfo();
    info->set_timecode_scale(1000000); // 1ms
    info->set_writing_app("opus_webm-ruby");
    info->set_muxing_app("libwebm");

    // Variables for Opus headers and timing
    int packet_count = 0;
    unsigned char *id_header = nullptr;
    size_t id_header_size = 0;
    unsigned char *comment_header = nullptr;
    size_t comment_header_size = 0;
    uint8_t channels = 2;
    uint16_t pre_skip = 0;

    int32_t sample_rate = 48000; // Opus always uses 48kHz
    int64_t pts_samples = 0;
    int64_t pts_ns = 0;
    int64_t last_pts_ns = 0;
    int64_t total_samples = 0;

    uint64_t audio_track_number = 0;
    mkvmuxer::AudioTrack* audio_track = nullptr;

    bool eos = false;
    char* buffer;
    long bytes;

    // Main processing loop
    while (!eos) {
        buffer = ogg_sync_buffer(&oy, BUFFER_SIZE);
        bytes = read(input_fd, buffer, BUFFER_SIZE);
        if (bytes < 0) {
            rb_sys_fail("read");
        }
        ogg_sync_wrote(&oy, bytes);

        while (ogg_sync_pageout(&oy, &og) == 1) {
            if (ogg_page_bos(&og)) {
                ogg_stream_init(&os, ogg_page_serialno(&og));
            }

            ogg_stream_pagein(&os, &og);

            while (ogg_stream_packetout(&os, &op) == 1) {
                if (packet_count == 0) {
                    // Process ID Header
                    id_header_size = op.bytes;
                    id_header = new unsigned char[id_header_size];
                    memcpy(id_header, op.packet, id_header_size);

                    if (id_header_size >= 19 && memcmp(id_header, "OpusHead", 8) == 0) {
                        channels = id_header[9];
                        pre_skip = id_header[10] | (id_header[11] << 8);
                    } else {
                        rb_raise(rb_eRuntimeError, "Invalid Opus ID header");
                    }

                    // Add audio track
                    audio_track_number = segment.AddAudioTrack(sample_rate, channels, 1);
                    if (!audio_track_number) {
                        rb_raise(rb_eRuntimeError, "Could not add audio track");
                    }

                    audio_track = static_cast<mkvmuxer::AudioTrack*>(
                        segment.GetTrackByNumber(audio_track_number));

                    audio_track->set_codec_id("A_OPUS");
                    audio_track->set_codec_delay(
                        static_cast<uint64_t>(pre_skip) * 1000000000ULL / sample_rate);
                    audio_track->set_seek_pre_roll(80000000ULL);

                } else if (packet_count == 1) {
                    // Process Comment Header
                    comment_header_size = op.bytes;
                    comment_header = new unsigned char[comment_header_size];
                    memcpy(comment_header, op.packet, comment_header_size);

                    // Combine headers for CodecPrivate
                    size_t codec_private_size = id_header_size + comment_header_size;
                    unsigned char *codec_private = new unsigned char[codec_private_size];
                    memcpy(codec_private, id_header, id_header_size);
                    memcpy(codec_private + id_header_size, comment_header, comment_header_size);

                    audio_track->SetCodecPrivate(codec_private, codec_private_size);

                    delete[] codec_private;
                    delete[] id_header;
                    delete[] comment_header;
                    id_header = nullptr;
                    comment_header = nullptr;

                } else {
                    // Process audio packets
                    int packet_duration_samples = GetOpusPacketDuration(op.packet, op.bytes);
                    if (packet_duration_samples < 0) {
                        rb_raise(rb_eRuntimeError, "Invalid Opus packet");
                    }

                    total_samples += packet_duration_samples;
                    pts_samples = total_samples - pre_skip;
                    pts_ns = pts_samples * 1000000000LL / sample_rate;

                    if (pts_ns < 0) pts_ns = 0;
                    if (pts_ns < last_pts_ns) pts_ns = last_pts_ns;
                    last_pts_ns = pts_ns;

                    if (!segment.AddFrame(
                            static_cast<const uint8_t*>(op.packet),
                            op.bytes,
                            audio_track_number,
                            pts_ns,
                            true)) {
                        rb_raise(rb_eRuntimeError, "Could not add frame to segment");
                    }
                }

                packet_count++;
            }

            if (ogg_page_eos(&og)) {
                eos = true;
                break;
            }
        }

        if (bytes == 0) {
            eos = true;
            break;
        }
    }

    // Cleanup
    if (!segment.Finalize()) {
        rb_raise(rb_eRuntimeError, "Could not finalize segment");
    }

    // Clean up writers
    if (RB_TYPE_P(output, T_STRING)) {
        std_writer.Close();
    } else {
        ruby_writer.reset();
    }

    // Clean up Ogg structures
    ogg_stream_clear(&os);
    ogg_sync_clear(&oy);

    if (!input_is_io) {
        close(input_fd);
    }

    return Qnil;
}

extern "C" void Init_opus_webm(void) {
    VALUE mOpusWebm = rb_define_module("OpusWebm");
    rb_define_private_method(rb_singleton_class(mOpusWebm), "_convert", opus_webm_convert, 2);
}
