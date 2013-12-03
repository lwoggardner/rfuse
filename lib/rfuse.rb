require 'fcntl'
require 'rfuse/version'
require 'rfuse/rfuse'
require 'rfuse/compat'

# Ruby FUSE (Filesystem in USErspace) binding
module RFuse

    # Used by listxattr
    def self.packxattr(xattrs)
        case xattrs
        when Array
            xattrs.join("\000").concat("\000")
        when String
            #assume already \0 separated list of keys
            xattrs
        else
            raise RFuse::Error, ":listxattr must return Array or String, got #{xattrs.inspect}"
        end
    end

    # Parse mount arguments and options
    #
    # @param [Array<String>] argv
    #        normalised fuse options
    #
    # @param [Array<Symbol>] local_opts local options
    #        if these are found in the argv entry following "-o" they are removed
    #        from argv, ie so argv is a clean set of options that can be passed
    #        to {RFuse::Fuse} or {RFuse::FuseDelegator}
    #
    # 
    # @return [Hash{Symbol => String,Boolean}]
    #         the extracted options
    #
    # @since 1.0.3
    # 
    # Fuse itself will normalise arguments
    #
    #    mount -t fuse </path/to/fs>#<device> mountpoint [options...]
    #    mount.fuse </path/to/fs>#<device> mountpoint [options...]
    #
    # both result in a call to /path/to/fs with arguments...
    #
    #    [device] mountpoint [-h] [-o [opt,optkey=value,...]]
    #
    # which this method will parse into a Hash with the following special keys
    #
    #  :device - the optional mount device, removed from argv if present
    #  :mountpoint - required mountpoint
    #  :help - true if -h was supplied
    #
    # and any other supplied options.
    #
    # @example
    #   ARGV = [ "some/device", "/mnt/point", "-h", "-o", "debug,myfs=aValue" ]
    #   options = RFuse.parse_options(ARGV,:myfs)
    # 
    #   # options == 
    #   { :device => "some/device",
    #     :mountpoint => "/mnt/point",
    #     :help => true,
    #     :debug => true,
    #     :myfs => "aValue"
    #   }
    #   # and ARGV == 
    #   [ "/mnt/point","-h","-o","debug" ]
    #
    #   fs = create_filesystem(options)
    #   fuse = RFuse::FuseDelegator.new(fs,*ARGV)
    #  
    def self.parse_options(argv,*local_opts)
        result = Hash.new(nil)
        
        first_opt_index = (argv.find_index() { |opt| opt =~ /-.*/ } || argv.length ) 

        result[:device] = argv.shift if first_opt_index > 1
        result[:mountpoint] = argv[0] if argv.length > 0
        
        if argv.include?("-h")
            result[:help]  =  true
        end

        if argv.include?("-d")
            result[:debug] = true
        end

        opt_index = ( argv.find_index("-o") || -1 ) + 1

        if opt_index > 1 && opt_index < argv.length
            options = argv[opt_index].split(",")

            options.delete_if() do |opt| 
                opt.strip!

                opt,value = opt.split("=",2)
                value = true unless value
                opt_sym = opt.to_sym
                result[opt_sym] = value

                #result of delete if
                local_opts.include?(opt_sym)
            end

            if options.empty?
                argv.slice!(opt_index - 1,2)
            else
                argv[opt_index] = options.join(",")
            end
        end

        result
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
        def loop()
            raise RFuse::Error, "Already running!" if @running
            raise RFuse::Error, "FUSE not mounted" unless mounted?
            @running = true
            while @running do
                begin
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
                rescue Interrupt
                    #oh well...
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

        # Called by C unmount before doing all the FUSE stuff
        def ruby_unmount
            @pr.close if @pr && !@pr.closed?
            @pw.close if @pw && !@pw.closed?

            # Ideally we want this IO to avoid autoclosing at GC, but
            # in Ruby 1.8 we have no way to do that. A work around is to close
            # the IO here. FUSE won't necessarily like that but it is the best
            # we can do
            @fuse_io.close() if @fuse_io && !@fuse_io.closed? && @fuse_io.autoclose?
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

        # @return [Integer]
        attr_accessor :f_bsize,:f_frsize,:f_blocks,:f_bfree,:f_bavail

        # @return [Integer]
        attr_accessor :f_files,:f_ffree,:f_favail,:f_fsid,:f_flag,:f_namemax

        # values can be symbols or strings but drop the pointless f_ prefix
        def initialize(values={ })
            @f_bsize, @f_frsize, @f_blocks, @f_bfree, @f_bavail, @f_files, @f_ffree, @f_favail,@f_fsid, @f_flag,@f_namemax = Array.new(13,0)
            values.each_pair do |k,v|
                prefix = k.startswith?("f_") ? "" : "f_"
                instance_variable_set("@#{prefix}#{k}",v)
            end
        end
    end
end #Module RFuse
