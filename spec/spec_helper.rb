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

module RFuseHelper
    def mac?
        RUBY_PLATFORM.include?("darwin")
    end

    # Runs the single threaded fuse loop
    # on a pre configured mock fuse filesystem
    # Executes fork block in a separate process
    # that is expected to return success
    def with_fuse(mnt,mockfs,*options,&fork_block)

        fpid = Kernel.fork() {
            sleep 0.5
            begin
                fork_block.call() if fork_block
            ensure
                sleep 0.3
                if mac?
                    system("umount #{mnt}")
                else
                    system("fusermount -u #{mnt}")
                end
            end
        }

        fuse = RFuse::FuseDelegator.new(mockfs,mnt,*options)
        if mac?
            fuse.loop rescue nil
        else
            fuse.loop
        end

        pid,result = Process.waitpid2(fpid) 
        result.should be_success
        fuse.open_files.should be_empty()
        fuse.mounted?.should be_false unless mac?
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
include RFuseHelper
