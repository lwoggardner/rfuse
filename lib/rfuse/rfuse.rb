# frozen_string_literal: true

require 'ffi-libfuse'

module FFI
  module Libfuse
    module Adapter
      # Map callback methods to be compatible for legacy RFuse
      #
      # All native {FuseOperations} callback signatures are amended to take {FuseContext} as the first argument.
      #
      # A subset of callbacks are documented here where their signatures are further altered to match RFuse
      #
      # RFuse did not support Fuse3
      module RFuse
        # Filler object
        # @see readdir
        class Filler
          def initialize(buf, &fill_dir_func)
            @buf = buf
            @fill_dir_func = fill_dir_func
          end

          def push(name, stat, offset = 0)
            @fill_dir_func.call(@buf, name, stat, offset)
          end
        end

        # adapter module
        # @!visibility private
        module Shim
          module_function

          def write(path, buf, size, offset, info)
            yield(path, buf[0..size], offset, info)
            size
          end

          def read(path, buf, size, offset, info)
            res = yield(path, size, offset, info)

            return -Errno::ERANGE::Errno unless res.size <= size

            buf.write_bytes(res)
            res.size
          end

          def getattr(path, stat)
            res = yield path
            stat.fill(res)
            0
          end

          def readlink(path, buf, size)
            link = yield(path)
            buf.write_bytes(link, [0..size])
            0
          end

          def readdir(path, buf, fill_dir_func, offset, fuse_file_info, &block)
            filler = Filler.new(buf, &fill_dir_func)
            block.call(path, filler, offset, fuse_file_info)
          end

          def getxattr(path, name, buf, size, fuse_file_info = nil)
            res = yield(path, name, fuse_file_info)

            return -Errno::ENODATA::Errno unless res

            res = res.to_s
            return -Errno::ERANGE::Errno if res.size > size

            buf.write_bytes(res)
            res.size
          end

          def listxattr(path, buf, size)
            res = yield(path)
            res.reduce(0) do |offset, name|
              name = name.to_s
              return -Errno::ERANGE::Errno if offset + name.size >= size

              buf.put_string(offset, name) # put string includes the NUL terminator
              offset + name.size + 1
            end
          end

          def utimens(path, times, _ffi = nil)
            # Empty times means set both to current time
            times = [Stat::TimeSpec.now, Stat::TimeSpec.now] unless times&.size == 2

            # If both times are set to UTIME_NOW, make sure they get the same value!
            now = times.any?(&:now?) && Time.now
            atime, mtime = times.map { |t| t.nanos(now) }

            yield path, atime, mtime
            0
          end

          def mknod(path, mode, dev)
            minormask = (1 << 20) -1``
            #define MINORBITS       20
            #define MINORMASK       ((1U << MINORBITS) - 1)

            #define MAJOR(dev)      ((unsigned int) ((dev) >> MINORBITS))
            #define MINOR(dev)      ((unsigned int) ((dev) & MINORMASK))
            #define MKDEV(ma,mi)    (((ma) << MINORBITS) | (mi))
            yield path, mode
          end
        end

        # @method utimens(ctx,path,atime,mtime,info = nil)
        #  @abstract
        #  @param [Context] context
        #  @param [String] path
        #  @param [Integer] atime access time in nanoseconds or nil if only setting mtime
        #  @param [Integer] mtime modification time in nanoseconds or nil if only setting atime
        #
        #  @return [void]
        #  @raise [Errno]

        # @method write(ctx,path,data,offset,info)
        #  @abstract
        #  @param [FuseContext ctx]
        #  @param [String] path
        #  @param [String] data
        #  @param [Integer] offset
        #  @param [FuseFileInfo] info
        #  @return [void]

        # @method read(ctx,path,size,offset,info)
        #  @abstract
        #  @param [FuseContext ctx]
        #  @param [String] path
        #  @param [Integer] size
        #  @param [Integer] offset
        #  @param [FuseFileInfo] info
        #
        #  @return [String] the data, expected to be exactly size bytes

        # @method getattr(ctx,path)
        #  @param [FuseContext ctx]
        #  @param [String] path
        #  @return [Stat] the stat information (or something that can fill a stat)

        # @method readlink(path)
        #  @abstract
        #  @param [String] path
        #  @return [String] the link target

        # @method readdir(ctx,path,filler,offset,ffi)
        #  @abstract
        #
        #  List contents of a directory
        #  @param [FuseContext ctx]
        #  @param [String] path
        #  @param [Filler] filler
        #  @param [Fixnum] offset
        #  @param [FileInfo] ffi
        #
        #  @return [void]
        #  @raise [Errno]

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
          wrappers << Shim
          return wrappers unless defined?(super)

          super(*wrappers)
        end

        def self.included(mod)
          mod.include(Fuse3Support)
          mod.include(Debug)
          mod.include(Safe)
          mod.include(Context)
        end
      end
    end
  end
end
