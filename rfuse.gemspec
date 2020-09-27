# -*- encoding: utf-8 -*-
require_relative "lib/rfuse/version"

Gem::Specification.new do |s|
  s.name        = "rfuse"
  s.version     = "#{RFuse::VERSION}"
  # Only use the release version for actual deployment
  if ENV['TRAVIS_BUILD_STAGE_NAME']&.downcase == 'prerelease'
    s.version = "#{s.version}.#{ENV['TRAVIS_BRANCH']}.#{ENV['TRAVIS_BUILD_NUMBER']}"
  elsif ENV['TRAVIS'] != 'true' || ENV['TRAVIS_BUILD_STAGE_NAME'].downcase != 'deploy'
    s.version= "#{s.version}.pre"
  end
  s.platform    = Gem::Platform::RUBY
  s.license     = 'GPL-2.0'
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
