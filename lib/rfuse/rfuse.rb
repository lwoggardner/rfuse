# frozen_string_literal: true

require 'ffi/libfuse'

module RFuse
  # Filler object
  # @see readdir
  class Filler < FFI::Libfuse::Adapter::Ruby::ReaddirFiller

    def push(name, stat, offset = 0)
      fill(name, stat: stat, offset: offset)
    end
  end

  # Map callback methods to be compatible for legacy RFuse
  #
  # All native {FuseOperations} callback signatures are amended to take {FuseContext} as the first argument.
  #
  # A subset of callbacks are documented here where their signatures are further altered to match RFuse
  #
  # RFuse did not support Fuse3
  # adapter module
  module Adapter

    # Converts Fuse2 calls into RFuse calls
    # @!visibility private
    module Prepend

      include FFI::Libfuse::Adapter

      def readdir(ctx, path, buf, filler, offset, ffi)

        rd_filler = Filler.new(buf, filler, fuse3: false)
        super(ctx, path, rd_filler, offset, ffi)
        0
      end

      def readlink(ctx, path, buf, size)
        link = super(ctx, path)

        return -Errno::ERANGE::Errno unless link.size <= size

        buf.write_bytes(link)
        0
      end

      def mknod(ctx, path, mode, dev)
        super(ctx, path, mode, FFI::Device.major(dev), FFI::Device.minor(dev))
        0
      end

      def getattr(ctx, path, stat_buf)
        result = super(ctx, path)
        raise Errno::ENOENT unless result

        stat_buf.fill(result)
        0
      end

      def fgetattr(ctx, path, stat_buf, ffi)
        stat_buf.fill(super(ctx, path, ffi))
        0
      end

      # Set file atime, mtime via super as per {Ruby#utimens}
      def utimens(ctx, path, times)
        atime, mtime = Stat::TimeSpec.fill_times(times[0, 2], 2)

        begin
          super(ctx, path, atime.nanos, mtime.nanos)
        rescue NoMethodError
          utime(ctx, path, atime.sec, mtime.sec)
        end
        0
      end

      def read(ctx, path, buf, size, offset, info)
        res = super(ctx, path, size, offset, info)

        return -Errno::ERANGE::Errno unless res.size <= size

        buf.write_bytes(res)
        res.size
      end

      def write(ctx, path, buf, size, offset, info)
        super(ctx, path, buf.read_bytes(size), offset, info)
      end

      def statfs(ctx, path, statfs_buf)
        statfs_buf.fill(super(ctx, path))
        0
      end

      def setxattr(ctx, path, name, value, _size, flags)
        super(ctx, path, name, value, FFI::Libfuse::XAttr.to_h[flags] || 0)
        0
      end

      def getxattr(ctx, path, name, buf, size)
        FFI::Libfuse::Adapter::Ruby.getxattr(buf, size) do
          super(ctx, path, name)
        end
      end

      def listxattr(ctx, path, buf, size)
        FFI::Libfuse::Adapter::Ruby.listxattr(buf, size) do
          super(ctx, path)
        end
      end

      %i[create open opendir].each do |fuse_method|
        define_method(fuse_method) do |*args|
          super(*args)
          store_handle(args.last&.fh)
          0
        end
      end

      %i[release releasedir].each do |fuse_method|
        define_method(fuse_method) do |*args|
          super(*args)
          0
        rescue NoMethodError
          # OK, just release the file handle
          0
        ensure
          release_handle(args.last&.fh)
        end
      end

      def init(ctx, fuse_conn_info)
        super
        self
      end

      def lock(ctx, path, ffi, cmd, lock)
        super(ctx, path, ffi, FFI::Flock::Enums::LockCmd.to_h[cmd], Flock.new(lock))
        0
      end

      def fuse_respond_to?(fuse_callback)
        return true if super

        case fuse_callback
        when :release
          # Ensure file handles to be cleaned up at release
          %i[open create].any? { |m| super(m) }
        when :release_dir
          super(:open_dir)
        when :utimens
          super(:utime)
        else
          false
        end
      end

      def open_files
        handles
      end

      private

      # Prevent filehandles from being GCd
      def handles
        @handles ||= Set.new.compare_by_identity
      end

      def store_handle(file_handle)
        handles << file_handle if file_handle
      end

      def release_handle(file_handle)
        handles.delete(file_handle)
      end
    end


    # @!method readdir(context,path,filler,offset,ffi)
    #
    #  List contents of a directory
    #
    #  @param [Context] context
    #  @param [String] path
    #  @param [Filler] filler
    #  @param [Fixnum] offset
    #  @param [FileInfo] ffi
    #
    #  @return [void]
    #  @raise [Errno]

    # @!method readlink(path)
    #  @abstract
    #  @param [String] path
    #  @return [String] the link target

    # @!method mknod(context,path,mode,major,minor)
    # Create a file node
    #    @abstract Fuse Operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#1465eb2268cec2bb5ed11cb09bbda42f mknod}
    #
    #    @param [Context] context
    #    @param [String] path
    #    @param [Integer] mode  type & permissions
    #    @param [Integer] major
    #    @param [Integer] minor
    #
    #    @return[void]
    #
    #    This is called for creation of all non-directory, non-symlink nodes. If the filesystem defines {#create}, then for regular files that will be called instead.

    # @!method getattr(ctx,path)
    #  @param [FuseContext ctx]
    #  @param [String] path
    #  @return [Stat] the stat information (or something that can fill a stat)

    # @!method fgetattr(ctx,path,ffi)
    #  @param [FuseContext ctx]
    #  @param [String] path
    #  @return [Stat] the stat information (or something that can fill a stat)

    # @!method utime(ctx,path,atime,mtime)
    #  @abstract
    #  @deprecated prefer {utimens}
    #
    #  @param [Context] context
    #  @param [String] path
    #  @param [Integer] atime access time in seconds or nil if only setting mtime
    #  @param [Integer] mtime modification time in seconds or nil if only setting atime
    #
    #  @return [void]
    #  @raise [Errno]

    # @!method utimens(ctx,path,atime,mtime)
    #  @abstract
    #  @param [Context] context
    #  @param [String] path
    #  @param [Integer] atime access time in nanoseconds or nil if only setting mtime
    #  @param [Integer] mtime modification time in nanoseconds or nil if only setting atime
    #
    #  @return [void]
    #  @raise [Errno]

    # @!method read(ctx,path,size,offset,info)
    #  @abstract
    #  @param [FuseContext ctx]
    #  @param [String] path
    #  @param [Integer] size
    #  @param [Integer] offset
    #  @param [FuseFileInfo] info
    #
    #  @return [String] the data, expected to be exactly size bytes

    # @!method write(ctx,path,data,offset,info)
    #    @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#897d1ece4b8b04c92d97b97b2dbf9768 write}
    #    Write data to an open file
    #
    #    @param [Context] context
    #    @param [String] path
    #    @param [String] data
    #    @param [Integer] offset
    #    @param [FileInfo] ffi
    #
    #    @return [Integer] exactly the number of bytes requested except on error
    #    @raise [Errno]

    #    Set extended attributes
    #
    #    @!method setxattr(context,path,name,data,flags)
    #    @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#988ced7091c2821daa208e6c96d8b598 setxattr}
    #    @param [Context] context
    #    @param [String] path
    #    @param [String] name
    #    @param [String] data
    #    @param [Integer] flags
    #
    #    @return [void]
    #    @raise [Errno]

    # @method getxattr(ctx,path,name)
    #  @abstract
    #  @param [FuseContext ctx]
    #  @param [String] path
    #  @param [String] name the attribute name
    #  @return [String|nil] the attribute value or nil if it does not exist

    # @method listxattr(ctx,path)
    #  @abstract
    #  @param [FuseContext ctx]
    #  @param [String] path
    #  @return [Array<String>] list of attribute names for path

    # @!visibility private

    def fuse_wrappers(*wrappers)
      # Change signatures to match legacy RFuse
      wrappers.unshift({
        wrapper: proc { |_fm, *args, &b|  b.call(FFI::Libfuse::FuseContext.get, *args) },
        excludes: %i[destroy]
      })

      return wrappers unless defined?(super)

      super(*wrappers)
    end

    def default_errno
      Errno::ENOENT::Errno
    end

    def self.included(mod)
      mod.prepend(Prepend)
      mod.include(FFI::Libfuse::Adapter::Fuse3Support)
      mod.include(FFI::Libfuse::Adapter::Safe)
    end

  end
end

