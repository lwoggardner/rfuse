# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "rfuse/version"

Gem::Specification.new do |s|
  s.name        = "rfuse"
  s.version     = RFuse::VERSION
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Grant Gardner"]
  s.email       = ["grant@lastweekend.com.au"]
  s.homepage    = "http://rubygems.org/gems/rfuse"
  s.summary     = %q{Ruby language binding for FUSE}
  s.description = %q{Write userspace filesystems in Ruby}

  s.files         = `git ls-files`.split("\n")
  s.extensions    = 'ext/rfuse/extconf.rb'
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]

  s.has_rdoc = 'yard'
  s.extra_rdoc_files = 'CHANGES.md'

  s.add_development_dependency("rake")
  s.add_development_dependency("rake-compiler")
  s.add_development_dependency("rspec","~> 3.0.0")
  s.add_development_dependency("yard")
  s.add_development_dependency("redcarpet")
  s.add_development_dependency("ffi-xattr")
  s.add_development_dependency("sys-filesystem")
end
