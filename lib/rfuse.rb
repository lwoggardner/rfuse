require 'fcntl'
require 'rfuse/version'
require 'rfuse/rfuse'

# Ruby FUSE (Filesystem in USErspace) binding
module RFuse

    # Used by listxattr
    def self.packxattr(xattrs) 
        xattrs.join("\000")
    end

    class Fuse

        # Main processing loop
        #
        # Use {#exit} to stop processing (or externally call fusermount -u)
        #
        #
        # Other ruby threads can continue while loop is running, however
        # no thread can operate on the filesystem itself (ie with File or Dir methods)
        #
        # @return [void]
        # @raise [RFuse::Error] if already running or not mounted
        #
        def loop()
            raise RFuse::Error, "Already running!" if @running
            raise RFuse::Error, "FUSE not mounted" unless mounted?
            @running = true
            while @running do
                    ready, ignore, errors  = IO.select([@fuse_io,@pr],[],[@fuse_io])

                    if ready.include?(@pr)
                    
                        @pr.getc
                        @running = false

                    elsif errors.include?(@fuse_io)

                        @running = false
                        raise RFuse::Error, "FUSE error"

                    elsif ready.include?(@fuse_io)
                        if process() < 0
                            # Fuse has been unmounted externally
                            # TODO: mounted? should now return false
                            # fuse_exited? is not true...
                            @running = false
                        end
                    end
            end
        end

        # Stop processing {#loop}
        # eg called from Signal handlers, or some other monitoring thread
        def exit
            @pw.putc(0) 
        end

        private

        # Called by the C iniitialize
        # afer the filesystem has been mounted successfully
        def ruby_initialize
            @pr,@pw = IO.pipe()
            
            # The FD was created by FUSE so we don't want
            # ruby to do anything with it during GC
            @fuse_io = IO.for_fd(fd(),"r",:autoclose => false) 
        end
    end

    #This class is useful to make your filesystem implementation
    #debuggable and testable without needing to mount an actual filesystem
    #or inherit from {Fuse}
    class FuseDelegator < Fuse

        # Available fuse methods -see http://fuse.sourceforge.net/doxygen/structfuse__operations.html
        #  Note :getdir and :utime are deprecated
        #  :ioctl, :poll are not implemented in the C extension 
        FUSE_METHODS = [ :getattr, :readlink, :getdir, :mknod, :mkdir,
            :unlink, :rmdir, :symlink, :rename, :link,
            :chmod, :chown, :truncate, :utime, :open,
            :create, :read, :write, :statfs, :flush,
            :release, :fsync, :setxattr, :getxattr, :listxattr,:removexattr,
            :opendir, :readdir, :releasedir, :fsycndir,
            :init, :destroy, :access, :ftruncate, :fgetattr, :lock,
            :utimens, :bmap, :ioctl, :poll ]

        # @param [Object] fuse_object your filesystem object that responds to fuse methods
        # @param [String] mountpoint existing directory where the filesystem will be mounted
        # @param [String...] options fuse mount options (use "-h" to see a list)
        # 
        # Create and mount a filesystem
        #
        # If ruby debug is enabled then each call to fuse_object will be represented on $stderr
        def initialize(fuse_object,mountpoint,*options)
            @fuse_delegate = fuse_object
            define_fuse_methods(fuse_object)
            super(mountpoint,options)
        end

        private
        def define_fuse_methods(fuse_object)
            #Wrap all the rfuse methods in a context block
            FUSE_METHODS.each do |method|
                if fuse_object.respond_to?(method)
                    method_name = method.id2name
                    instance_eval(<<-EOM, "(__FUSE_DELEGATE__)",1)
                    def #{method_name} (*args,&block)
                        begin
                            $stderr.puts "==> #{ self }.#{ method_name }(\#{args.inspect })" if $DEBUG
                            result = @fuse_delegate.send(:#{method_name},*args,&block)
                                                         $stderr.puts "<== #{ self }.#{ method_name }()"  if $DEBUG
                            result
                        rescue Exception => ex
                            $@.delete_if{|s| /^\\(__FUSE_DELEGATE__\\):/ =~ s}
                            $stderr.puts(ex.message) unless ex.respond_to?(:errno) || $DEBUG
                            Kernel::raise
                        end
                    end
                    EOM
                end
            end
        end

    end #class FuseDelegator

    # Helper class to return from :getattr method
    class Stat
        # Format mask
        S_IFMT   = 0170000 
        # Directory  
        S_IFDIR  = 0040000
        # Character device
        S_IFCHR  = 0020000
        # Block device
        S_IFBLK  = 0060000
        # Regular file 
        S_IFREG  = 0100000
        # FIFO. 
        S_IFIFO  = 0010000
        # Symbolic link 
        S_IFLNK  = 0120000
        # Socket 
        S_IFSOCK = 0140000

        # @param [Fixnum] mode file permissions
        # @param [Hash<Symbol,Fixnum>] values initial values for other attributes
        #
        # @return [Stat] representing a directory
        def self.directory(mode=0,values = { })
            return self.new(S_IFDIR,mode,values)
        end

        # @param [Fixnum] mode file permissions
        # @param [Hash<Symbol,Fixnum>] values initial values for other attributes
        #
        # @return [Stat] representing a regular file
        def self.file(mode=0,values = { })
            return self.new(S_IFREG,mode,values)
        end

        # @return [Integer] see stat(2)
        attr_accessor :uid,:gid,:mode,:size, :dev,:ino,:nlink,:rdev,:blksize,:blocks

        # @return [Integer, Time] see stat(2)
        attr_accessor :atime,:mtime,:ctime

        def initialize(type,permissions,values = { })
            values[:mode] = ((type & S_IFMT) | (permissions & 07777))
            @uid,@gid,@size,@mode,@atime,@mtime,@ctime,@dev,@ino,@nlink,@rdev,@blksize,@blocks = Array.new(13,0)
            values.each_pair do |k,v|
                instance_variable_set("@#{ k }",v)
            end
        end
    end

    # Helper class to return from :statfs (eg for df output)
    # All attributes are Integers and default to 0
    class StatVfs
        attr_accessor :f_bsize,:f_frsize,:f_blocks,:f_bfree,:f_bavail
        attr_accessor :f_files,:f_ffree,:f_favail,:f_fsid,:f_flag,:f_namemax

        # values can be symbols or strings but drop the pointless f_ prefix
        def initialize(values={ })
            @f_bsize, @f_frsize, @f_blocks, @f_bfree, @f_bavail, @f_files, @f_ffree, @f_favail,@f_fsid, @f_flag,@f_namemax = Array.new(13,0)
            values.each_pair do |k,v|
                instance_variable_set("@f_#{ k }",v)
            end
        end
    end

end #Module RFuse
