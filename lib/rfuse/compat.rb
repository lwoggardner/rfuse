
unless IO.instance_methods.include?(:autoclose?)
    class IO
        def self.for_fd(fd, mode_string, options = {})
            IO.new(fd,mode_string)
        end

        def autoclose?
            true
        end
    end
end

unless Time.instance_methods.include?(:nsec)
    class Time
        def nsec
            usec * 1000
        end
    end
end
