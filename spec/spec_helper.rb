require 'rfuse'

module RFuseHelper
    # Runs the single threaded fuse loop
    # on a pre configured mock fuse filesystem
    # Executes fork block in a separate process
    # that is expected to return success
    def with_fuse(mnt,mockfs,&fork_block)

        fpid = Kernel.fork() {
            sleep 1
            begin
                fork_block.call()
            ensure
                system("fusermount -u #{mnt}")
            end
        }

        fuse = RFuse::FuseDelegator.new(mockfs,mnt,"-odebug")
        fuse.loop
        pid,result = Process.waitpid2(fpid)
        result.should be_success
        fuse.open_files.should be_empty()
    end
end
#TODO: mkdir /tmp/rfuse-spec, or provide an optional parameter to with_fuse for
# a tempdir
include RFuseHelper
