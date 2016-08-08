#!/usr/bin/env rake
require "bundler/gem_tasks"
require 'yard'
require 'rspec/core/rake_task'
require 'rake/extensiontask'

CLOBBER.include("pkg","doc")
RSpec::Core::RakeTask.new(:spec)

YARD::Rake::YardocTask.new

Rake::ExtensionTask.new('rfuse') do |ext|
  ext.lib_dir = "lib/rfuse"

  if RUBY_PLATFORM.include?('darwin')
    ext.config_options << '--with-cflags="-D_FILE_OFFSET_BITS=64 -D_DARWIN_USE_64_BIT_INODE -I/usr/local/include/osxfuse"'
  end
end

task :spec => :compile
task :default => :spec
