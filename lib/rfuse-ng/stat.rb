require 'fcntl'

module RFuse
  
  # File/Directory attributes
  class Stat
    S_IFMT   = 0170000 # Format mask
    S_IFDIR  = 0040000 # Directory.  
    S_IFCHR  = 0020000 # Character device.
    S_IFBLK  = 0060000 # Block device.
    S_IFREG  = 0100000 # Regular file. 
    S_IFIFO  = 0010000 # FIFO. 
    S_IFLNK  = 0120000 # Symbolic link. 
    S_IFSOCK = 0140000 # Socket. 
    
    def self.directory(mode=0,values = { })
      return self.new(S_IFDIR,mode,values)
    end
    
    def self.file(mode=0,values = { })
      return self.new(S_IFREG,mode,values)
    end
    
    attr_accessor :uid,:gid,:mode,:size,:atime,:mtime,:ctime
    attr_accessor :dev,:ino,:nlink,:rdev,:blksize,:blocks
    
    def initialize(type,permissions,values = { })
      values[:mode] = ((type & S_IFMT) | (permissions & 07777))
      @uid,@gid,@size,@mode,@atime,@mtime,@ctime,@dev,@ino,@nlink,@rdev,@blksize,@blocks = Array.new(13,0)
      values.each_pair do |k,v|
        instance_variable_set("@#{ k }",v)
      end
    end
  end
  
  # Filesystem attributes (eg for df output)
  class StatVfs
    attr_accessor :f_bsize,:f_frsize,:f_blocks,:f_bfree,:f_bavail
    attr_accessor :f_files,:f_ffree,:f_favail,:f_fsid,:f_flag,:f_namemax
    #values can be symbols or strings but drop the pointless f_ prefix
    def initialize(values={ })
      @f_bsize, @f_frsize, @f_blocks, @f_bfree, @f_bavail, @f_files, @f_ffree, @f_favail,@f_fsid, @f_flag,@f_namemax = Array.new(13,0)
      values.each_pair do |k,v|
        instance_variable_set("@f_#{ k }",v)
      end
    end
  end

end #Module RFuse
