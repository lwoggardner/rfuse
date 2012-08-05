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
                system("fusermount -u #{mnt}")
            end
        }

        fuse = RFuse::FuseDelegator.new(mockfs,mnt,*options)
        fuse.loop
        pid,result = Process.waitpid2(fpid) 
        result.should be_success
        fuse.open_files.should be_empty()
        fuse.mounted?.should be_false
    end

    def file_mode(mode)
        FileModeMatcher.new(mode)
    end

    def tempmount()
        Dir.mktmpdir("rfuse-spec")
    end
end
include RFuseHelper
