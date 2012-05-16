# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "rfuse-ng/version"

Gem::Specification.new do |s|
  s.name        = "rfuse-ng"
  s.version     = RFuseNG::VERSION
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["Tamás László Fábián"]
  s.email       = ["info@tddium.com"]
  s.homepage    = "http://www.tddium.com/"
  s.summary     = %q{rfuse-ng gem for Ruby 1.9.2}
  s.description = %q{Ruby FUSE interface gem for Ruby 1.9.2}

  s.rubyforge_project = "rfuse-ng"

  s.files         = `git ls-files`.split("\n")
  s.extensions    = 'ext/extconf.rb'
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]

  s.add_development_dependency("rake")
end
