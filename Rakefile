#!/usr/bin/env rake
require "bundler/gem_tasks"
require 'yard'
require 'rspec/core/rake_task'
require 'rake/extensiontask'
require 'travis/release/task'

Travis::Release::Task.new

CLOBBER.include("pkg","doc")

RSpec::Core::RakeTask.new(:spec)

YARD::Rake::YardocTask.new

Rake::ExtensionTask.new('rfuse') do |ext|
  ext.lib_dir = "lib/rfuse"
end

task :spec => :compile
task :default => :spec
