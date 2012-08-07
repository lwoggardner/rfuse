#!/usr/bin/env rake
require "bundler/gem_tasks"
require 'yard'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:spec)

YARD::Rake::YardocTask.new do |t|
    # Need this because YardocTask does not read the gemspec
      t.files   = ['lib/**/*.rb', 'ext/**/*.c','-','CHANGES.md']   # optional
end

task :default => :spec
