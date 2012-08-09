#!/usr/bin/env rake
require "bundler/gem_tasks"
require 'yard'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:spec)

YARD::Rake::YardocTask.new 

task :default => :spec
