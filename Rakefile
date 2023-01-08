#!/usr/bin/env rake
require 'yard'
require 'rspec/core/rake_task'
require 'rake/clean'

CLOBBER.include("pkg","doc")

RSpec::Core::RakeTask.new(:spec)

YARD::Rake::YardocTask.new

task :default => %i[version spec]

require_relative 'lib/rfuse/gem_version'
FFI::Libfuse::GemHelper.install_tasks(main_branch: RFuse::MAIN_BRANCH, version: RFuse::VERSION)