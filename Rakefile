#!/usr/bin/env rake
require 'yard'
require 'rspec/core/rake_task'
require 'rake/clean'

CLOBBER.include("pkg","doc")

RSpec::Core::RakeTask.new(:spec)

YARD::Rake::YardocTask.new

task :default => :spec

RELEASE_BRANCH = 'master'
desc 'Release RFuse Gem'
task :release,[:options] => %i(clean default) do |_t,args|
  args.with_defaults(options: '--pretend')
  branch = `git rev-parse --abbrev-ref HEAD`.strip
  raise "Cannot release from #{branch}, only #{RELEASE_BRANCH}" unless branch == RELEASE_BRANCH
  Bundler.with_unbundled_env do
    raise "Tag failed" unless system({'RFUSE_RELEASE' => 'Y'},"gem tag -p #{args[:options]}".strip)
    raise "Bump failed" unless system("gem bump -v patch -p #{args[:options]}".strip)
  end
end
