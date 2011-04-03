# encoding: UTF-8

require 'rubygems'
require 'rake'

begin
  require 'jeweler'
  Jeweler::Tasks.new do |gem|
    gem.name = "rfuse-ng"
    gem.version = "0.5.3"
    gem.summary = 'Ruby language binding for FUSE'
    gem.description = 'Ruby language binding for FUSE. It was forked from rfuse'
    gem.rubyforge_project = 'rfuse-ng'
    gem.authors = ["Tamás László Fábián"]
    gem.email = "giganetom@gmail.com"
    gem.homepage = "http://rubyforge.org/projects/rfuse-ng"
    gem.licenses = ["LGPL"]
    gem.extensions = ["ext/extconf.rb"]
  end
  Jeweler::GemcutterTasks.new
rescue LoadError
  puts "Jeweler (or a dependency) not available. Install it with: sudo gem install jeweler"
end

task :default => :build
