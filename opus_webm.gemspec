require_relative 'lib/opus_webm/version'

Gem::Specification.new do |s|
  s.name        = 'opus_webm'
  s.version     = OpusWebm::VERSION
  s.summary     = 'Opus to WebM converter'
  s.description = 'Remux Opus audio from OGG into WebM container'
  s.authors     = ['Felix Buenemann']
  s.email       = 'felix.buenemann@gmail.com'
  s.files       = Dir.glob('ext/**/*.{rb,cpp,hpp}') + Dir.glob('lib/**/*.rb')
  s.extensions  = ['ext/opus_webm/extconf.rb']
  s.homepage    = 'https://rubygems.org/gems/opus_webm'
  s.license     = 'MIT'
end
