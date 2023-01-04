module RFuse
  # Wrapper for FFI:Flock back to RFuse
  class Flock

    attr_reader :delegate

    def initialize(ffi_flock)
      @delegate = ffi_flock
    end

    def l_type
      FFI::Flock::Enums::LockType.to_h[delegate.type] || 0
    end

    def l_whence
      FFI::Flock::Enums::SeekWhenceShort.to_h[delegate.whence] || 0
    end

    def l_start
      delegate.start
    end

    def l_len
      delegate.len
    end

    def l_pid
      delegate.pid
    end
  end
end