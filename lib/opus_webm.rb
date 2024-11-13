require 'opus_webm/version'
require 'opus_webm/opus_webm'  # Load the compiled C extension
require 'tempfile'

module OpusWebm
  def self.convert(input, output = nil)
    if output
      _convert(input, output)
    else
      output = Tempfile.new(["opus_webm", ".webm"], binmode: true)
      _convert(input, output)
      output.rewind
      output
    end
  end
end
