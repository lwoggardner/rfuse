# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "rfuse-ng/version"

Gem::Specification.new do |s|
  s.name        = "rfuse-ng"
  s.version     = RFuseNG::VERSION
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Tamás László Fábián"]
  s.email       = ["giganetom@gmail.com"]
  s.homepage    = "http://github.com/tddium/rfuse-ng"
  s.summary     = %q{Ruby language binding for FUSE}
  s.description = %q{Ruby language binding for FUSE. It was forked from rfuse and modified for Ruby 1.9.2.}

  s.rubyforge_project = "rfuse-ng"

  s.files         = `git ls-files`.split("\n")
  s.extensions    = 'ext/extconf.rb'
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]

  s.add_development_dependency("rake")
end
