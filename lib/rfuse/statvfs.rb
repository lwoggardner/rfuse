module RFuse

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

    def respond_to_missing?(method, private=false)
      !method.start_with?('f_') && respond_to?("f_#{method}", private)
    end

    def method_missing(method, *args)
      return super if method.start_with?('f_')

      send("f_#{method}", *args)
    end
  end
end