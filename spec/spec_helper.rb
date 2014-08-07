require 'rfuse'
require 'tmpdir'

class FileModeMatcher
    def initialize(expected)
        @expected = expected
    end

    def description
        "file mode #{@expected}"
    end

    def ==(actual)
        (actual | RFuse::Stat::S_IFMT) ==  (@expected | RFuse::Stat::S_IFMT)
    end
end

module DoubleAliases
  def mock(*args, &block)
    double(*args, &block)
  end
  alias stub mock
end


module RFuseHelper
    # Runs the single threaded fuse loop
    # on a pre configured mock fuse filesystem
    # Executes fork block in a separate process
    # that is expected to return success
    def with_fuse(mnt,mockfs,*options,&fork_block)

        fuse = RFuse::FuseDelegator.new(mockfs,mnt,*options)
        fuse.mounted?.should be(true)
        fork_fuse(fuse) do
            begin
                fork_block.call() if fork_block
            ensure
                fusermount(mnt)
            end
        end
        fuse.open_files.should be_empty()
        fuse.mounted?.should be(false)
    end

    def fork_fuse(fuse,&fork_block)

        fpid = Kernel.fork() { fork_block.call() }

        fuse.loop

        pid,result = Process.waitpid2(fpid)
        result.should be_success
    end

    def fusermount(mnt)
        unless system("fusermount -u #{mnt} >/dev/null 2>&1")
            sleep(0.5)
            system("fusermount -u #{mnt}")
        end
    end

    def file_mode(mode)
        FileModeMatcher.new(mode)
    end

    def tempmount()
        Dir.mktmpdir("rfuse-spec")
    end

    # Generate a set of times with non-zero usec values
    def usec_times(*increments)
        increments.collect { |inc|
            begin
                time = Time.now() + inc
                sleep(0.001)
            end until time.usec != 0
            time
        }
    end

end

RSpec.configure do |config|
  config.expect_with :rspec do |c|
    c.syntax = [:should, :expect]
  end
  config.mock_with :rspec do |c|
    c.syntax = [:should, :expect]
  end

  config.after(:suite) do
      Dir.glob(Dir.tmpdir + "/rfuse-spec*") do |dir|
          count = 0
          begin
            system("fusermount -u #{dir} >/dev/null 2>&1")
            FileUtils.rmdir(dir)
          rescue Errno::EBUSY
            #puts "Cleaning up #{dir} is busy, retrying..."
            sleep(0.5)
            count = count + 1
            retry unless count > 3
          end
      end
  end

  config.include RFuseHelper
  config.include DoubleAliases
end
