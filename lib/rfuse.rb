# frozen_string_literal: true

require 'fcntl'
require_relative 'rfuse/version'
require_relative 'rfuse/rfuse'
require_relative 'rfuse/compat'
require_relative 'rfuse/stat'
require_relative 'rfuse/statvfs'
require_relative 'rfuse/flock'

# Ruby FUSE (Filesystem in USErspace) binding
module RFuse

  class Error < StandardError; end

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
  #   argv = [ "some/device", "/mnt/point", "-h", "-o", "debug,myfs=aValue" ]
  #   options = RFuse.parse_options(argv,:myfs)
  #
  #   # options ==
  #   { :device => "some/device",
  #     :mountpoint => "/mnt/point",
  #     :help => true,
  #     :debug => true,
  #     :myfs => "aValue"
  #   }
  #   # and argv ==
  #   [ "/mnt/point","-h","-o","debug" ]
  #
  #   fs = create_filesystem(options)
  #   fuse = RFuse::FuseDelegator.new(fs,*ARGV)
  #
  def self.parse_options(argv, *local_opts, desc: nil, exec: $0)

    def desc.fuse_help
      self
    end if desc && !desc.empty?

    argv.unshift(exec) unless argv.size >= 2 && argv[0..1].none? { |a| a.start_with?('-') }
    run_args = FFI::Libfuse::Main.fuse_parse_cmdline(*argv, handler: desc)

    run_args[:help] = true if run_args[:show_help] # compatibility

    device_opts = { 'subtype=' => :device, 'fsname=' => :device }
    local_opt_conf = local_opts.each.with_object(device_opts) do |o, conf|
      conf.merge!({ "#{o}=" => o.to_sym, "#{o}" => o.to_sym })
    end

    fuse_args = run_args.delete(:args)

    fuse_args.parse!(local_opt_conf, run_args, **{}) do |key:, value:, data:, **_|
      data[key] = value
      key == :device ? :keep : :discard
    end

    argv.replace(fuse_args.argv)
    # The first arg is actually the device and ignored in future calls to parse opts
    # (eg during fuse_new, but rfuse used to return the mountpoint here.
    argv[0] = run_args[:mountpoint]

    run_args
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
  def self.main(argv = ARGV.dup, extra_options = [], extra_options_usage = nil, device = nil, exec = File.basename($PROGRAM_NAME))
    begin
      fuse_help = ['Filesystem options:',extra_options_usage,device,''].compact.join("\n")

      options = parse_options(argv, *extra_options, desc: extra_options_usage && fuse_help, exec: exec)

      unless options[:mountpoint]
        warn "rfuse: failed, no mountpoint provided"
        puts usage(device,exec)
        return nil
      end

      fs = yield options, argv

      raise Error, 'no filesystem provided' unless fs

      # create can pass the extra options to a constructor so order and existence is important.
      fs_options = extra_options.each_with_object({}) { |k,h| h[k] = options.delete(k) }
      fuse = create(argv: argv, fs: fs, options: fs_options)

      return nil if options[:show_help] || options[:show_version]

      raise Error, "filesystem #{fs} not mounted" unless fuse&.mounted?

      fuse.run(**options)
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
  def self.create(fs:, argv: [], options: {})
    if fs.is_a?(Fuse)
      fs
    elsif fs.is_a?(Class)
      if Fuse > fs
        fs.new(*options.values, *argv)
      else
        delegate = fs.new(*options.values)
        FuseDelegator.new(delegate, *argv)
      end
    elsif fs
      FuseDelegator.new(fs, *argv)
    end
  end

  class Fuse

    attr_reader :mountpoint
    alias mountname mountpoint

    def initialize(mountpoint, *argv)
      @mountpoint = mountpoint
      @fuse = FFI::Libfuse::Main.fuse_create(mountpoint, *argv, operations: self, private_data: self)
    end

    #    Is the filesystem successfully mounted
    #
    #    @return [Boolean] true if mounted, false otherwise
    def mounted?
      @fuse && @fuse.mounted?
    end

    # Convenience method to run a mounted filesystem to completion.
    #
    # @param [Array<String|Integer>] signals list of signals to handle.
    #   Default is all available signals. See {#trap_signals}
    #
    # @return [void]
    # @since 1.1.0
    # @see RFuse.main
    def run(signals = Signal.list.keys, **run_args)
      raise Error, 'not mounted' unless @fuse
      trap_signals(*signals)
      @fuse.run(traps: @traps, **run_args)
    end

    def loop
      run([] ,single_thread: true, foreground: true)
    end

    # Stop processing
    def exit
      @fuse&.exit&.join
    end
    alias :unmount :exit

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
      signames = signames.map { |n| n.to_s.upcase }.map { |n| n.start_with?('SIG') ? n[3..-1] : n }
      signames.keep_if { |n| Signal.list[n] && respond_sigmethod?(sigmethod(n)) }

      @traps ||= {}
      @traps.merge!(signames.map { |n| [ n, ->() { call_sigmethod(sigmethod(n)) }]}.to_h)
      @traps.keys
    end

    private

    def respond_sigmethod?(sigmethod)
      respond_to?(sigmethod)
    end

    def sigmethod(signame)
      "sig#{signame.downcase}".to_sym
    end

    def call_sigmethod(sigmethod)
      send(sigmethod)
    end

    def self.inherited(klass)
      klass.include(Adapter) unless klass.ancestors.include?(Adapter)
    end
  end

  # This class is useful to make your filesystem implementation
  # debuggable and testable without needing to mount an actual filesystem
  # or inherit from {Fuse}
  class FuseDelegator < Fuse

    # @param [Object] fuse_object your filesystem object that responds to fuse methods
    # @param [String] mountpoint existing directory where the filesystem will be mounted
    # @param [String...] options fuse mount options (use "-h" to see a list)
    #
    # Create and mount a filesystem
    #
    def initialize(fuse_object, mountpoint, *options)
      @fuse_delegate = fuse_object
      @debug = $DEBUG
      super(mountpoint, *options)
    end

    # USR1 sig handler - toggle debugging of fuse methods
    # @return [void]
    def sigusr1
      @debug = !debug?
    end

    def debug?
      @debug
    end

    # @private
    def to_s
      "#{self.class.name}::#{@fuse_delegate}"
    end

    def fuse_respond_to?(fuse_method)
      return false unless @fuse_delegate.respond_to?(fuse_method)

      m = @fuse_delegate.method(fuse_method)
      m = m.super_method while m && FFI::Libfuse::Adapter.include?(m.owner)

      m && true
    end

    def respond_to_missing?(method, private=false)
      FFI::Libfuse::FuseOperations.fuse_callbacks.include?(method) ? @fuse_delegate.respond_to?(method, private) : super
    end

    def method_missing(method_name, *args, &block)
      return super unless FFI::Libfuse::FuseOperations.fuse_callbacks.include?(method_name) && @fuse_delegate.respond_to?(method_name)
      begin
        $stderr.puts "==> \#{ self }.#{method_name}(\#{args.inspect })" if debug?
        result = @fuse_delegate.send(method_name,*args,&block)
          $stderr.puts "<== \#{ self }.#{method_name}()"  if debug?
        result
      rescue => ex
        $@.delete_if{|s| /^\\(__FUSE_DELEGATE__\\):/ =~ s}
        $stderr.puts(ex.message) unless ex.respond_to?(:errno) || debug?
        Kernel::raise
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

    # Remove Kernel:open etc..
    FFI::Libfuse::FuseOperations.fuse_callbacks.each { |c| undef_method(c) rescue nil }
  end

end
