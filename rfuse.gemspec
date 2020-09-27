# -*- encoding: utf-8 -*-
require_relative "lib/rfuse/version"

Gem::Specification.new do |s|
  s.name        = "rfuse"
  s.version     = "#{RFuse::VERSION}"
  # Pre-release deployment on matching branches
  s.version     = "#{s.version}.#{ENV['TRAVIS_BRANCH']}.#{ENV['TRAVIS_BUILD_NUMBER']}" if (ENV['TRAVIS_TAG'] || '') == '' && ENV['TRAVIS_BUILD_STAGE_NAME'].downcase == 'deploy'
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Grant Gardner"]
  s.email       = ["grant@lastweekend.com.au"]
  s.homepage    = "http://rubygems.org/gems/rfuse"
  s.summary     = %q{Ruby language binding for FUSE}
  s.description = %q{Write userspace filesystems in Ruby}
  s.files       = Dir['lib/**/*.rb','ext/**/*.{c,h,rb}','*.md','LICENSE','.yardopts']
  s.extensions    = 'ext/rfuse/extconf.rb'
  s.require_paths = ["lib"]

  s.required_ruby_version = '>= 2.5'

  s.extra_rdoc_files = 'CHANGES.md'

  s.add_development_dependency("rake")
  s.add_development_dependency("rake-compiler")
  s.add_development_dependency("rspec")
  s.add_development_dependency("yard")
  s.add_development_dependency("redcarpet")
  s.add_development_dependency("ffi-xattr")
  s.add_development_dependency("sys-filesystem")
end
