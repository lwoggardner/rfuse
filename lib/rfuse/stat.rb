module RFuse
  # Helper class to return from :getattr method
  class Stat

    # See FFI::Stat constants
    def self.const_missing(const)
      return super unless FFI::Stat.const_defined?(const)
      FFI::Stat.const_get(const)
    end

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
end