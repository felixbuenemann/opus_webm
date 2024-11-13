require 'mkmf'

# Configure compiler flags
if find_executable('pkg-config')
  append_cflags `pkg-config ogg --cflags-only-I`.strip
  append_ldflags `pkg-config ogg --libs-only-L`.strip
end

# Check for required libraries
have_library('ogg')
have_library('webm')

# Enable C++11 support
$CXXFLAGS = [$CXXFLAGS, '-std=c++11', $CFLAGS].compact.join(' ')

# Check for required headers
find_header('ogg/ogg.h')

with_cflags("-x c++ -std=c++11") do
  find_header('mkvmuxer/mkvmuxer.h')
  find_header('mkvmuxer/mkvwriter.h')
end

# Create Makefile
create_makefile('opus_webm/opus_webm')
