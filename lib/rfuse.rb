# frozen_string_literal: true

require 'fcntl'
require 'rfuse/version'
require 'rfuse/rfuse'
require 'rfuse/compat'

# Ruby FUSE (Filesystem in USErspace) binding
module RFuse
  # @private
  # Used by listxattr
  def self.packxattr(xattrs)
    case xattrs
    when Array
      xattrs.join("\000").concat("\000")
    when String
      # assume already \0 separated list of keys
      xattrs
    else
      raise Error, ":listxattr must return Array or String, got #{xattrs.inspect}"
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
  # @return [Hash<Symbol,String|Boolean>]
  #         the extracted options
  #
  # @since 1.0.3
  #
  # Fuse itself will normalise arguments
  #
  #     mount -t fuse </path/to/fs.rb>#<device> mountpoint [options...]
  #     mount.fuse </path/to/fs.rb>#<device> mountpoint [options...]
  #
  # both result in the following command execution
  #
  #     /path/to/fs.rb [device] mountpoint [-h] [-d] [-o [opt,optkey=value,...]]
  #
  # which this method will parse into a Hash with the following special keys
  #
  #  * `:device` - the optional mount device, removed from argv if present
  #  * `:mountpoint` - required mountpoint
  #  * `:help` - if -h was supplied - will print help text (and not mount the filesystem!)
  #  * `:debug` - if -d (or -o debug) was supplied - will print debug output from the underlying FUSE library
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
  def self.parse_options(argv, *local_opts, desc: nil)

    def desc.fuse_help
      self
    end if desc && !desc.empty?

    run_args = FFI::Libfuse::Main.parse_cmdline($0, *argv, handler: desc)

    run_args[:help] = true if run_args[:show_help] # compatibility
    run_args[:device] = run_args[:fsname] if run_args.key?(:fsname)

    local_opt_conf = local_opts.each.with_object({}) { |o,conf| conf["#{o}="] = o.to_sym }

    run_args[:args].parse!(local_opt_conf, run_args) do |data, arg, key, out|
      FFI::Libfuse::Main.hash_opt_proc(data, arg, key, out, discard: local_opts)
    end

    result
  end

  # Generate a usage string
  #
  # @param [String] device a description of how the device field should be used
  # @param [String] exec the executable
  # @return [String] the usage string
  def self.usage(device = nil, exec = File.basename($PROGRAM_NAME))
    "Usage:\n     #{exec} #{device} mountpoint [-h] [-d] [-o [opt,optkey=value,...]]\n"
  end

  # Convenience method to launch a fuse filesystem, with nice usage messages and default signal traps
  #
  # @param [Array<String>] argv command line arguments
  # @param [Array<Symbol>] extra_options list of additional options
  # @param [String] extra_options_usage describing additional option usage
  # @param [String] device a description of the device field
  # @param [String] exec the executable file
  #
  # @yieldparam [Hash<Symbol,String>] options See {.parse_options}
  # @yieldparam [Array<String>] argv Cleaned argv See {.parse_options}
  #
  # @yieldreturn [Class<Fuse>]
  #   a subclass of {Fuse} that implements your filesystem. Will receive `.new(*extra_options,*argv)`
  #
  # @yieldreturn [Class]
  #   a class that implements {FuseDelegator::FUSE_METHODS}. Will receive `.new(*extra_options)`
  #   and the resulting instance sent with `*argv` to {FuseDelegator}
  #
  # @yieldreturn [Fuse]
  #   Your initialised (and therefore already mounted) filesystem
  #
  # @yieldreturn [Object]
  #   An object that implements the {Fuse} methods, to be passed with `*argv` to {FuseDelegator}
  #
  # @yieldreturn [Error]
  #   raise {Error} with appropriate message for invalid options
  #
  # @since 1.1.0
  #
  # @example
  #
  #   class MyFuse < Fuse
  #      def initialize(myfs,*argv)
  #         @myfs = myfs # my filesystem local option value
  #         super(*argv)
  #      end
  #      # ... implementations for filesystem methods ...
  #   end
  #
  #   class MyFSClass  # not a subclass of Fuse, FuseDelegator used
  #      def initialize(myfs)
  #       @myfs = myfs # my filesystem local option value
  #      end
  #      # ...
  #   end
  #
  #   MY_OPTIONS = [ :myfs ]
  #   OPTION_USAGE = "  -o myfs=VAL how to use the myfs option"
  #   DEVICE_USAGE = "how to use device arg"
  #
  #   # Normally from the command line...
  #   ARGV = [ "some/device", "/mnt/point", "-h", "-o", "debug,myfs=aValue" ]
  #
  #   RFuse.main(ARGV, MY_OPTIONS, OPTION_USAGE, DEVICE_USAGE, $0) do |options, argv|
  #
  #       # options ==
  #          { :device => "some/device",
  #            :mountpoint => "/mnt/point",
  #            :help => true,
  #            :debug => true,
  #            :myfs => "aValue"
  #          }
  #
  #       # ... validate options...
  #       raise RFuse::Error, "Bad option" unless options[:myfs]
  #
  #       # return the filesystem class to be initialised by RFuse
  #       MyFuse
  #       # or
  #       MyFSClass
  #
  #       # OR take full control over initialisation yourself and return the object
  #       MyFuse.new(options[:myfs],*argv)
  #       # or
  #       MyFSClass.new(options[:myfs])
  #
  #   end
  #
  def self.main(argv = ARGV, extra_options = [], extra_options_usage = nil, device = nil, exec = File.basename($PROGRAM_NAME))
    begin
      fuse_help = [extra_options_usage,device].compact.join("\n")

      options = parse_options(argv, *extra_options, desc: fuse_help)

      fs = yield options, options[:args].argv[1..]

      raise Error, 'no filesystem provided' unless fs

      fuse = create(fs, argv, options, extra_options)

      raise Error, "filesystem #{fs} not mounted" unless fuse&.mounted?

      fuse.run
    rescue Error => e
      # These errors are produced generally because the user passed bad arguments etc..
      puts usage(device, exec) unless options[:help]
      warn "rfuse failed: #{e.message}"
      nil
    rescue StandardError => e
      # These are other errors related the yield block
      warn e.message
      warn e.backtrace.join("\n")
    end
  end

  # @private
  # Helper to create a {Fuse} from variety of #{.main} yield results
  def self.create(fs, argv = [], options = {}, extra_options = [])
    if fs.is_a?(Fuse)
      fs
    elsif fs.is_a?(Class)
      extra_option_values = extra_options.map { |o| options[o] }
      if Fuse > fs
        fs.new(*extra_option_values, *argv)
      else
        delegate = fs.new(*extra_option_values)
        FuseDelegator.new(delegate, *argv)
      end
    elsif fs
      FuseDelegator.new(fs, *argv)
    end
  end

  class Fuse

    def initialize(*argv)
      @fuse = FFI::Libfuse::Main.fuse_create(*argv, operations: self, private_data: self)
    end

    def mounted?
      @fuse
    end

    # Convenience method to run a mounted filesystem to completion.
    #
    # @param [Array<String|Integer>] signals list of signals to handle.
    #   Default is all available signals. See {#trap_signals}
    #
    # @return [void]
    # @since 1.1.0
    # @see RFuse.main
    def run(signals = Signal.list.keys)
      if mounted?
        begin
          traps = trap_signals(*signals)
          self.loop
        ensure
          traps.each { |t| Signal.trap(t, 'DEFAULT') }
          unmount
        end
      end
    end

    # Main processing loop
    #
    # Use {#exit} to stop processing (or externally call fusermount -u)
    #
    # Other ruby threads can continue while loop is running, however
    # no thread can operate on the filesystem itself (ie with File or Dir methods)
    #
    # @return [void]
    # @raise [RFuse::Error] if already running or not mounted
    def loop
      raise RFuse::Error, 'Already running!' if @running
      raise RFuse::Error, 'FUSE not mounted' unless mounted?

      @running = true
      while @running
        begin
          ready, ignore, errors = IO.select([@fuse_io, @pr], [], [@fuse_io])

          if ready.include?(@pr)

            signo = @pr.read_nonblock(1).unpack1('c')

            # Signal.signame exist in Ruby 2, but returns horrible errors for non-signals in 2.1.0
            if (signame = Signal.list.invert[signo])
              call_sigmethod(sigmethod(signame))
            end

          elsif errors.include?(@fuse_io)

            @running = false
            raise RFuse::Error, 'FUSE error'

          elsif ready.include?(@fuse_io)
            if process.negative?
              # Fuse has been unmounted externally
              # TODO: mounted? should now return false
              # fuse_exited? is not true...
              @running = false
            end
          end
        rescue Errno::EWOULDBLOCK, Errno::EAGAIN
          # oh well...
        end
      end
    end

    # Stop processing {#loop}
    # eg called from signal handlers, or some other monitoring thread
    def exit
      @running = false
      send_signal(-1)
    end

    # Set traps
    #
    # The filesystem supports a signal by providing a `sig<name>` method. eg {#sigint}
    #
    # The fuse {#loop} is notified of the signal via the self-pipe trick, and calls the corresponding
    #   `sig<name>` method, after any current filesystem operation completes.
    #
    # This method will not override traps that have previously been set to something other than "DEFAULT"
    #
    # Note: {Fuse} itself provides {#sigterm} and {#sigint}.
    #
    # @param [Array<String>] signames
    #   List of signal names to set traps for, if the filesystem has methods to handle them.
    #   Use `Signal.list.keys` to try all available signals.
    #
    # @return [Array<String>] List of signal names that traps have been set for.
    #
    # @since 1.1.0
    #
    # @example
    #   class MyFS < Fuse
    #      def sighup()
    #          # do something on HUP signal
    #      end
    #   end
    #
    #   fuse = MyFS.new(*args)
    #
    #   if fuse.mounted?
    #       # Below will result in (effectively) Signal.trap("HUP") { fuse.sighup() }
    #       fuse.trap_signals("HUP","USR1") # ==> ["HUP"]
    #       fuse.loop()
    #   end
    #
    def trap_signals(*signames)
      signames.map { |n| n.to_s.upcase }.map { |n| n.start_with?('SIG') ? n[3..-1] : n }.select do |signame|
        next false unless respond_sigmethod?(sigmethod(signame)) && signo = Signal.list[signame]

        next true if (prev = Signal.trap(signo) { |signo| send_signal(signo) }) == 'DEFAULT'

        Signal.trap(signo, prev)
        false
      end
    end

    # Default signal handler to exit on TERM/INT
    # @return [void]
    # @see #trap_signals
    def sigterm
      @running = false
    end
    alias sigint sigterm

    private



    def call_sigmethod(sigmethod)
      send(sigmethod)
    end

    def respond_sigmethod?(sigmethod)
      respond_to?(sigmethod)
    end

    def sigmethod(signame)
      "sig#{signame.downcase}".to_sym
    end

    def send_signal(signo)
      @pw.write([signo].pack('c')) unless !@pw || @pw.closed?
    end
  end

  # This class is useful to make your filesystem implementation
  # debuggable and testable without needing to mount an actual filesystem
  # or inherit from {Fuse}
  class FuseDelegator < Fuse
    # Available fuse methods -see http://fuse.sourceforge.net/doxygen/structfuse__operations.html
    #  Note :getdir and :utime are deprecated
    #  :ioctl, :poll are not implemented in the C extension
    FUSE_METHODS = %i[getattr readlink getdir mknod mkdir
                      unlink rmdir symlink rename link
                      chmod chown truncate utime open
                      create read write statfs flush
                      release fsync setxattr getxattr listxattr removexattr
                      opendir readdir releasedir fsycndir
                      init destroy access ftruncate fgetattr lock
                      utimens bmap ioctl poll].freeze

    # @param [Object] fuse_object your filesystem object that responds to fuse methods
    # @param [String] mountpoint existing directory where the filesystem will be mounted
    # @param [String...] options fuse mount options (use "-h" to see a list)
    #
    # Create and mount a filesystem
    #
    def initialize(fuse_object, mountpoint, *options)
      @fuse_delegate = fuse_object
      define_fuse_methods(fuse_object)
      @debug = false
      self.debug = $DEBUG
      super(mountpoint, options)
    end

    # USR1 sig handler - toggle debugging of fuse methods
    # @return [void]
    def sigusr1
      self.debug = !@debug
    end

    # RFuse Debugging status
    #
    # @note this is independent of the Fuse kernel module debugging enabled with the "-d" mount option
    #
    # @return [Boolean] whether debugging information should be printed to $stderr around each fuse method.
    #    Defaults to $DEBUG
    # @since 1.1.0
    # @see #sigusr1
    def debug?
      @debug
    end

    # Set debugging on or off
    # @param [Boolean] value enable or disable debugging
    # @return [Boolean] the new debug value
    # @since 1.1.0
    def debug=(value)
      value = value ? true : false
      if @debug && !value
        warn "=== #{self}.debug=false"
      elsif !@debug && value
        warn "=== #{self}.debug=true"
      end
      @debug = value
    end

    # @private
    def to_s
      "#{self.class.name}::#{@fuse_delegate}"
    end

    private

    def define_fuse_methods(fuse_object)
      FUSE_METHODS.each do |method|
        next unless fuse_object.respond_to?(method)

        method_name = method.id2name
        instance_eval(<<-EOM, __FILE__, __LINE__ + 1)
                    def #{method_name} (*args,&block)
                        begin
                            $stderr.puts "==> \#{ self }.#{method_name}(\#{args.inspect })" if debug?
                            result = @fuse_delegate.send(:#{method_name},*args,&block)
                            $stderr.puts "<== \#{ self }.#{method_name}()"  if debug?
                            result
                        rescue => ex
                            $@.delete_if{|s| /^\\(__FUSE_DELEGATE__\\):/ =~ s}
                            $stderr.puts(ex.message) unless ex.respond_to?(:errno) || debug?
                            Kernel::raise
                        end
                    end
        EOM
      end
    end

    def call_sigmethod(sigmethod)
      warn "==> #{self}.#{sigmethod}()" if debug?
      dlg = @fuse_delegate.respond_to?(sigmethod) ? @fuse_delegate : self
      dlg.send(sigmethod)
      warn "<== #{self}.#{sigmethod}()" if debug?
    end

    def respond_sigmethod?(sigmethod)
      @fuse_delegate.respond_to?(sigmethod) || respond_to?(sigmethod)
    end
  end

  class Context
    # @private
    def to_s
      "Context::u#{uid},g#{gid},p#{pid}"
    end
  end

  class Filler
    # @private
    def to_s
      'Filler::[]'
    end
  end

  class FileInfo
    # @private
    def to_s
      "FileInfo::fh->#{fh}"
    end
  end

  # Helper class to return from :getattr method
  class Stat
    # Format mask
    S_IFMT   = 0o170000
    # Directory
    S_IFDIR  = 0o040000
    # Character device
    S_IFCHR  = 0o020000
    # Block device
    S_IFBLK  = 0o060000
    # Regular file
    S_IFREG  = 0o100000
    # FIFO.
    S_IFIFO  = 0o010000
    # Symbolic link
    S_IFLNK  = 0o120000
    # Socket
    S_IFSOCK = 0o140000

    # @param [Fixnum] mode file permissions
    # @param [Hash<Symbol,Fixnum>] values initial values for other attributes
    #
    # @return [Stat] representing a directory
    def self.directory(mode = 0, values = {})
      new(S_IFDIR, mode, values)
    end

    # @param [Fixnum] mode file permissions
    # @param [Hash<Symbol,Fixnum>] values initial values for other attributes
    #
    # @return [Stat] representing a regular file
    def self.file(mode = 0, values = {})
      new(S_IFREG, mode, values)
    end

    # @return [Integer] see stat(2)
    attr_accessor :uid, :gid, :mode, :size, :dev, :ino, :nlink, :rdev, :blksize, :blocks

    # @return [Integer, Time] see stat(2)
    attr_accessor :atime, :mtime, :ctime

    def initialize(type, permissions, values = {})
      values[:mode] = ((type & S_IFMT) | (permissions & 0o7777))
      @uid, @gid, @size, @mode, @atime, @mtime, @ctime, @dev, @ino, @nlink, @rdev, @blksize, @blocks = Array.new(
        13, 0
      )
      values.each_pair do |k, v|
        instance_variable_set("@#{k}", v)
      end
    end
  end

  # Helper class to return from :statfs (eg for df output)
  # All attributes are Integers and default to 0
  class StatVfs
    # @return [Integer]
    attr_accessor :f_bsize, :f_frsize, :f_blocks, :f_bfree, :f_bavail

    # @return [Integer]
    attr_accessor :f_files, :f_ffree, :f_favail, :f_fsid, :f_flag, :f_namemax

    # values can be symbols or strings but drop the pointless f_ prefix
    def initialize(values = {})
      @f_bsize, @f_frsize, @f_blocks, @f_bfree, @f_bavail, @f_files, @f_ffree, @f_favail, @f_fsid, @f_flag, @f_namemax = Array.new(
        13, 0
      )
      values.each_pair do |k, v|
        prefix = k.to_s.start_with?('f_') ? '' : 'f_'
        instance_variable_set("@#{prefix}#{k}", v)
      end
    end
  end
end
