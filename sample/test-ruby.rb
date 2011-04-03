#!/usr/bin/ruby

# TestFS for RFuse-ng

require "rfuse_ng"

class MyDir < Hash
  attr_accessor :name, :mode , :actime, :modtime, :uid, :gid
  def initialize(name,mode)
    @uid=0
    @gid=0
    @actime=0     #of couse you should use now() here!
    @modtime=0    # -''-
    @xattr=Hash.new
    @name=name
    @mode=mode | (4 << 12) #yes! we have to do this by hand
  end
  def listxattr()
    @xattr.each {|key,value| list=list+key+"\0"}
  end
  def setxattr(name,value,flag)
    @xattr[name]=value #TODO:don't ignore flag
  end
  def getxattr(name)
    return @xattr[name]
  end
  def removexattr(name)
    @xattr.delete(name)
  end
  def dir_mode
    return (@mode & 170000)>> 12 #see dirent.h
  end
  def size
    return 48 #for testing only
  end
  def isdir
    true
  end
  def insert_obj(obj,path)
    d=self.search(File.dirname(path))
    if d.isdir then
      d[obj.name]=obj
    else
      raise Errno::ENOTDIR.new(d.name)
    end
    return d
  end
  def remove_obj(path)
    d=self.search(File.dirname(path))
    d.delete(File.basename(path))
  end
  def search(path)
    p=path.split('/').delete_if {|x| x==''}
    if p.length==0 then
      return self
    else
      return self.follow(p)
    end
  end
  def follow (path_array)
    if path_array.length==0 then
      return self
    else
      d=self[path_array.shift]
      if d then
        return d.follow(path_array)
      else
        raise Errno::ENOENT.new
      end
    end
  end
  def to_s
    return "Dir: " + @name + "(" + @mode.to_s + ")"
  end
end

class MyFile
  attr_accessor :name, :mode, :actime, :modtime, :uid, :gid, :content
  def initialize(name,mode,uid,gid)
    @actime=0
    @modtime=0
    @xattr=Hash.new
    @content=""
    @uid=uid
    @gid=gid
    @name=name
    @mode=mode
  end
  def listxattr() #hey this is a raw interface you have to care about the \0
    list=""
    @xattr.each {|key,value| 
      list=list+key+"\0"}
    return list
  end
  def setxattr(name,value,flag)
    @xattr[name]=value #TODO:don't ignore flag
  end
  def getxattr(name)
    return @xattr[name]
  end
  def removexattr(name)
    @xattr.delete(name)
  end
  def size
    return content.size
  end 
  def dir_mode
    return (@mode & 170000) >> 12
  end
  def isdir
    false
  end
  def follow(path_array)
    if path_array.length != 0 then
      raise Errno::ENOTDIR.new
    else
      return self
    end
  end
  def to_s
    return "File: " + @name + "(" + @mode.to_s + ")"
  end
end

class Stat
  attr_accessor :uid,:gid,:mode,:size,:atime,:mtime,:ctime 
  attr_accessor :dev,:ino,:nlink,:rdev,:blksize,:blocks
  def initialize
    @uid     = 0
    @gid     = 0
    @mode    = 0
    @size    = 0
    @atime   = 0
    @mtime   = 0
    @ctime   = 0
    @dev     = 0
    @ino     = 0
    @nlink   = 0
    @rdev    = 0
    @blksize = 0
    @blocks  = 0
  end
end

class StatVfs
  attr_accessor :f_bsize,:f_frsize,:f_blocks,:f_bfree,:f_bavail
  attr_accessor :f_files,:f_ffree,:f_favail,:f_fsid,:f_flag,:f_namemax
  def initialize
    @f_bsize    = 0
    @f_frsize   = 0
    @f_blocks   = 0
    @f_bfree    = 0
    @f_bavail   = 0
    @f_files    = 0
    @f_ffree    = 0
    @f_favail   = 0
    @f_fsid     = 0
    @f_flag     = 0
    @f_namemax  = 0
  end
end

class MyFuse < RFuse::Fuse

  def initialize(mnt,kernelopt,libopt,root)
    super(mnt,kernelopt,libopt)
    @root=root
  end

  # The old, deprecated way: getdir
  #def getdir(ctx,path,filler)
  #  d=@root.search(path)
  #  if d.isdir then
  #    d.each {|name,obj| 
  #      # Use push_old to add this entry, no need for Stat here
  #      filler.push_old(name, obj.mode, 0)
  #    }
  #  else
  #    raise Errno::ENOTDIR.new(path)
  #  end
  #end

  # The new readdir way, c+p-ed from getdir
  def readdir(ctx,path,filler,offset,ffi)
    d=@root.search(path)
    if d.isdir then
      d.each {|name,obj| 
        stat = Stat.new
        stat.uid   = obj.uid
        stat.gid   = obj.gid
        stat.mode  = obj.mode
        stat.size  = obj.size
        stat.atime = obj.actime
        stat.mtime = obj.modtime
        filler.push(name,stat,0)
      }
    else
      raise Errno::ENOTDIR.new(path)
    end
  end

  def getattr(ctx,path)
    d = @root.search(path)
    stat = Stat.new
    stat.uid   = d.uid
    stat.gid   = d.gid
    stat.mode  = d.mode
    stat.size  = d.size
    stat.atime = d.actime
    stat.mtime = d.modtime
    return stat
  end #getattr

  def mkdir(ctx,path,mode)
    @root.insert_obj(MyDir.new(File.basename(path),mode),path)
  end #mkdir

  def mknod(ctx,path,mode,dev)
    @root.insert_obj(MyFile.new(File.basename(path),mode,ctx.uid,ctx.gid),path)
  end #mknod

  def open(ctx,path,ffi)
  end

  #def release(ctx,path,fi)
  #end

  #def flush(ctx,path,fi)
  #end

  def chmod(ctx,path,mode)
    d=@root.search(path)
    d.mode=mode
  end

  def chown(ctx,path,uid,gid)
    d=@root.search(path)
    d.uid=uid
    d.gid=gid
  end

  def truncate(ctx,path,offset)
    d=@root.search(path)
    d.content = d.content[0..offset]
  end

  def utime(ctx,path,actime,modtime)
    d=@root.search(path)
    d.actime=actime
    d.modtime=modtime
  end

  def unlink(ctx,path)
    @root.remove_obj(path)
  end

  def rmdir(ctx,path)
    @root.remove_obj(path)
  end

  #def symlink(ctx,path,as)
  #end

  def rename(ctx,path,as)
    d = @root.search(path)
    @root.remove_obj(path)
    @root.insert_obj(d,path)
  end

  #def link(ctx,path,as)
  #end

  def read(ctx,path,size,offset,fi)
    d = @root.search(path)
    if (d.isdir) 
      raise Errno::EISDIR.new(path)
      return nil
    else
      return d.content[offset..offset + size - 1]
    end
  end

  def write(ctx,path,buf,offset,fi)
    d=@root.search(path)
    if (d.isdir) 
      raise Errno::EISDIR.new(path)
    else
      d.content[offset..offset+buf.length - 1] = buf
    end
    return buf.length
  end

  def setxattr(ctx,path,name,value,size,flags)
    d=@root.search(path)
    d.setxattr(name,value,flags)
  end

  def getxattr(ctx,path,name,size)
    d=@root.search(path)
    if (d) 
      value=d.getxattr(name)
      if (!value)
        value=""
        #raise Errno::ENOENT.new #TODO raise the correct error :
        #NOATTR which is not implemented in Linux/glibc
      end
    else
      raise Errno::ENOENT.new
    end
    return value
  end

  def listxattr(ctx,path,size)
    d=@root.search(path)
    value= d.listxattr()
    return value
  end

  def removexattr(ctx,path,name)
    d=@root.search(path)
    d.removexattr(name)
  end

  #def opendir(ctx,path,ffi)
  #end

  #def releasedir(ctx,path,ffi)
  #end

  #def fsyncdir(ctx,path,meta,ffi)
  #end

  # Some random numbers to show with df command
  def statfs(ctx,path)
    s = StatVfs.new
    s.f_bsize    = 1024
    s.f_frsize   = 1024
    s.f_blocks   = 1000000
    s.f_bfree    = 500000
    s.f_bavail   = 990000
    s.f_files    = 10000
    s.f_ffree    = 9900
    s.f_favail   = 9900
    s.f_fsid     = 23423
    s.f_flag     = 0
    s.f_namemax  = 10000
    return s
  end

  def ioctl(ctx, path, cmd, arg, ffi, flags, data)
    # FT: I was not been able to test it.
    print "*** IOCTL: command: ", cmd, "\n"
  end

  def poll(ctx, path, ffi, ph, reventsp)
    print "*** POLL: ", path, "\n"
    # This is how we notify the caller if something happens:
    ph.notifyPoll();
    # when the GC harvests the object it calls fuse_pollhandle_destroy
    # by itself.
  end

  def init(ctx,rfuseconninfo)
    print "RFuse-ng TestFS started\n"
    print "init called\n"
    print "proto_major: "
    print rfuseconninfo.proto_major
    print "\n"
    return nil
  end

end #class Fuse

# Parameters are intended to passed as array elements. This clearly don't
# work with kernel options. The first element is discarded and the first
# element after the -o is also discarded. Somebody please figure this out,
# I already wasted half my weekend trying to solve this.

fo = MyFuse.new("/tmp/fuse",["notparsed","-o notparsed,allow_other"],["debug"], MyDir.new("",0777));

#kernel:  default_permissions,allow_other,kernel_cache,large_read,direct_io
#         max_read=N,fsname=NAME
#library: debug,hard_remove

Signal.trap("TERM") do
  fo.exit
  fo.unmount
end

begin
  fo.loop
rescue
  print "Error:" + $!
end
