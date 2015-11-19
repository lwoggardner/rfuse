#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif
//FOR LINUX ONLY
#include <linux/stat.h> 
#include <linux/kdev_t.h>

#include <ruby.h>
#include "ruby-compat.h"
#include <fuse.h>
#include <errno.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "helper.h"
#include "intern_rfuse.h"
#include "filler.h"
#include "context.h"

#include "file_info.h"
#include "pollhandle.h"
#include "bufferwrapper.h"


static VALUE mRFuse;
static VALUE eRFuse_Error;

static int unsafe_return_error(VALUE *args)
{

  if (rb_respond_to(rb_errinfo(),rb_intern("errno"))) {
    VALUE res;
    //We expect these and they get passed on the fuse so be quiet...
    res = rb_funcall(rb_errinfo(),rb_intern("errno"),0);
    rb_set_errinfo(Qnil);
    return res;
  } else {
    VALUE info;
    VALUE bt_ary;
    int c;

    info = rb_inspect(rb_errinfo());
    printf ("ERROR: Exception %s not an Errno:: !respond_to?(:errno) \n",StringValueCStr(info)); 
    //We need the ruby_errinfo backtrace not fuse.loop ... rb_backtrace();
    bt_ary = rb_funcall(rb_errinfo(), rb_intern("backtrace"),0);

    for (c=0;c<RARRAY_LEN(bt_ary);c++) {
      printf("%s\n",StringValueCStr(RARRAY_PTR(bt_ary)[c]));
    }
    return Qnil;
  }
}

static int return_error(int def_error)
{
  /*if the raised error has a method errno the return that value else
    return def(ault)_error */
  VALUE res;
  int error = 0;
  res=rb_protect((VALUE (*)())unsafe_return_error,Qnil,&error);
  if (error)
  { 
    //something went wrong resolving the exception
    printf ("ERROR: Exception in exception!\n");
    return def_error;
  }
  else 
  {
    return NIL_P(res) ? def_error : FIX2INT(res);
  }
}

//Every call needs this stuff
static void init_context_path_args(VALUE *args,struct fuse_context *ctx,const char *path)
{
  args[0] = (VALUE) ctx->private_data;
  args[1] = wrap_context(ctx);
  args[2] = rb_str_new2(path);
  rb_filesystem_encode(args[2]);
}
/*
 @overload readdir(context,path,filler,offset,ffi)
 @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#0f634deda31d1e1c42664585ae820076 readdir}
 
 List contents of a directory

 @param [Context] context
 @param [String] path
 @param [Filler] filler
 @param [Fixnum] offset
 @param [FileInfo] ffi 

 @return [void]
 @raise [Errno] 
 @todo the API for the filler could be more ruby like - eg yield
*/
static VALUE unsafe_readdir(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("readdir"),5,&args[1]);
}

static int rf_readdir(const char *path, void *buf,
  fuse_fill_dir_t filler, off_t offset,struct fuse_file_info *ffi)
{
  VALUE fuse_module;
  VALUE rfiller_class;
  VALUE rfiller_instance;
  struct filler_t *fillerc;
  VALUE args[6];
  int error = 0;
  
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  //create a filler object
  fuse_module = rb_const_get(rb_cObject, rb_intern("RFuse"));
  rfiller_class=rb_const_get(fuse_module,rb_intern("Filler"));
  rfiller_instance=rb_funcall(rfiller_class,rb_intern("new"),0);
  Data_Get_Struct(rfiller_instance,struct filler_t,fillerc);

  fillerc->filler=filler;
  fillerc->buffer=buf;
  args[3]=rfiller_instance;
  args[4]=OFFT2NUM(offset);
  args[5]=get_file_info(ffi);

  rb_protect((VALUE (*)())unsafe_readdir,(VALUE)args,&error);
  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
   Resolve target of symbolic link 
   @overload readlink(context,path,size)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#b4ce6e6d69dfde3ec550f22d932c5633 readlink}
   @param [Context] context
   @param [String] path
   @param [Integer] size if the resolved path is greater than this size it should be truncated

   @return [String] the resolved link path
   @raise [Errno]
*/
static VALUE unsafe_readlink(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("readlink"),3,&args[1]);
}

static int rf_readlink(const char *path, char *buf, size_t size)
{
  VALUE args[4];
  VALUE res;
  int error = 0;
  char *rbuf;
  
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=SIZET2NUM(size);
  res=rb_protect((VALUE (*)())unsafe_readlink,(VALUE)args,&error);  
  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    rbuf=StringValueCStr(res);
    strncpy(buf,rbuf,size);
    return 0;
  }
}

/*
  @deprecated see {#readdir}
  @abstract Fuse operation getdir
*/
static VALUE unsafe_getdir(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("getdir"),3,&args[1]);
}

//call getdir with an Filler object
static int rf_getdir(const char *path, fuse_dirh_t dh, fuse_dirfil_t df)
{
  VALUE fuse_module;
  VALUE rfiller_class;
  VALUE rfiller_instance;
  VALUE args[4];
  struct filler_t *fillerc;
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  //create a filler object
  fuse_module = rb_const_get(rb_cObject, rb_intern("RFuse"));
  rfiller_class    = rb_const_get(fuse_module,rb_intern("Filler"));
  rfiller_instance = rb_funcall(rfiller_class,rb_intern("new"),0);

  Data_Get_Struct(rfiller_instance,struct filler_t,fillerc);

  fillerc->df = df;
  fillerc->dh = dh;

  args[3]=rfiller_instance;

  rb_protect((VALUE (*)())unsafe_getdir, (VALUE)args, &error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
   Create a file node
   @overload mknod(context,path,mode,major,minor)
   @abstract Fuse Operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#1465eb2268cec2bb5ed11cb09bbda42f mknod}

   @param [Context] context
   @param [String] path
   @param [Integer] mode  type & permissions
   @param [Integer] major
   @param [Integer] minor

   @return[void]

   This is called for creation of all non-directory, non-symlink nodes. If the filesystem defines {#create}, then for regular files that will be called instead.

*/
static VALUE unsafe_mknod(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("mknod"),5,&args[1]);
}

static int rf_mknod(const char *path, mode_t mode,dev_t dev)
{
  VALUE args[6];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  int major;
  int minor;

  init_context_path_args(args,ctx,path);
 
  major = MAJOR(dev);
  minor = MINOR(dev);

  args[3]=INT2FIX(mode);
  args[4]=INT2FIX(major);
  args[5]=INT2FIX(minor);
  rb_protect((VALUE (*)())unsafe_mknod,(VALUE) args,&error);
  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
   Get file attributes.
   @overload getattr(context,path)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#7a4c5d8eaf7179d819618c0cf3f73724 getattr}
   @param [Context] context
   @param [String] path

   @return [Stat] or something that quacks like a stat, or nil if the path does not exist
   @raise [Errno]

   Similar to stat(). The 'st_dev' and 'st_blksize' fields are ignored. The 'st_ino' field is ignored except if the 'use_ino' mount option is given.
*/
static VALUE unsafe_getattr(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("getattr"),2,&args[1]);
}

//calls getattr with path and expects something like FuseStat back
static int rf_getattr(const char *path, struct stat *stbuf)
{
  VALUE args[3];
  VALUE res;
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  res=rb_protect((VALUE (*)())unsafe_getattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  
  if (res == Qnil) {
      return -ENOENT;
  }
  else
  {
    rstat2stat(res,stbuf);
    return 0;
  }
}

/*
   Create a directory

   @overload mkdir(context,path,mode)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#0a38aa6ca60e945772d5d21b0c1c8916 mkdir}
   
   @param [Context] context
   @param [String] path
   @param [Integer] mode to obtain correct directory permissions use mode | {Stat.S_IFDIR}

   @return [void]
   @raise [Errno]
   
*/
static VALUE unsafe_mkdir(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("mkdir"),3,&args[1]);
}

static int rf_mkdir(const char *path, mode_t mode)
{
  VALUE args[4];
  int error = 0;
  
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=INT2FIX(mode);
  rb_protect((VALUE (*)())unsafe_mkdir,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
  File open operation

  @overload open(context,path,ffi)
  @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#14b98c3f7ab97cc2ef8f9b1d9dc0709d open}
  @param [Context] context
  @param [String] path
  @param [FileInfo] ffi
     file open flags etc.
     The fh attribute may be used to store an arbitrary filehandle object which will be passed to all subsequent operations on this file

  @raise [Errno::ENOPERM] if user is not permitted to open the file
  @raise [Errno] for other errors

  @return [void]
*/
static VALUE unsafe_open(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("open"),3,&args[1]);
}

static int rf_open(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[4];
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  args[3]=wrap_file_info(ctx,ffi);

  rb_protect((VALUE (*)())unsafe_open,(VALUE) args,&error);
  
  return error ? -(return_error(ENOENT)) : 0;
}

//This method is registered as a default for release in the case when
//open/create are defined, but a specific release method is not.
//similarly for opendir/releasedir
static int rf_release_ffi(const char *path, struct fuse_file_info *ffi)
{
  struct fuse_context *ctx=fuse_get_context();
  
  release_file_info(ctx,ffi);

  return 0;
}

/*
  Release an open file

  @overload release(context,path,ffi)
  @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#bac8718cdfc1ee273a44831a27393419 release}
  @param [Context] context
  @param [String] path
  @param [FileInfo] ffi

  @return [void] 

  Release is called when there are no more references to an open file: all file descriptors are closed and all memory mappings are unmapped.

  For every {#open} call there will be exactly one {#release} call with the same flags and file descriptor. It is possible to have a file opened more than once, in which case only the last release will mean, that no more reads/writes will happen on the file.
*/
static VALUE unsafe_release(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("release"),3,&args[1]);
}

static int rf_release(const char *path, struct fuse_file_info *ffi)
{
  VALUE args[4];
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  args[3]=release_file_info(ctx,ffi);
  
  rb_protect((VALUE (*)())unsafe_release,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Synchronize file contents

   @overload fsync(context,path,datasync,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#92bdd6f43ba390a54ac360541c56b528 fsync}
  
   @param [Context] context
   @param [String] path
   @param [Integer] datasync if non-zero, then only user data should be flushed, not the metadata
   @param [FileInfo] ffi

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_fsync(VALUE *args) {
  return rb_funcall3(args[0],rb_intern("fsync"),4,&args[1]);
}

static int rf_fsync(const char *path, int datasync, struct fuse_file_info *ffi)
{
  VALUE args[5];
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3] = INT2NUM(datasync);
  args[4] = get_file_info(ffi);

  rb_protect((VALUE (*)())unsafe_fsync,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}



/*
   Possibly flush cached data

   @overload flush(context,path,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#d4ec9c309072a92dd82ddb20efa4ab14 flush}
  
   @param [Context] context
   @param [String] path
   @param [FileInfo] ffi

   @return [void]
   @raise [Errno]

   BIG NOTE: This is not equivalent to fsync(). It's not a request to sync dirty data.

   Flush is called on each close() of a file descriptor. So if a filesystem wants to return write errors in close() and the file has cached dirty data, this is a good place to write back data and return any errors. Since many applications ignore close() errors this is not always useful.

NOTE: The flush() method may be called more than once for each open(). This happens if more than one file descriptor refers to an opened file due to dup(), dup2() or fork() calls. It is not possible to determine if a flush is final, so each flush should be treated equally. Multiple write-flush sequences are relatively rare, so this shouldn't be a problem.

Filesystems shouldn't assume that flush will always be called after some writes, or that if will be called at all.
*/
static VALUE unsafe_flush(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("flush"),3,&args[1]);
}

static int rf_flush(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=get_file_info(ffi);
  rb_protect((VALUE (*)())unsafe_flush,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Change the size of a file

   @overload truncate(context,path,offset)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#8efb50b9cd975ba8c4c450248caff6ed truncate}

   @param [Context] context
   @param [String] path
   @param [Integer] offset

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_truncate(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("truncate"),3,&args[1]);
}

static int rf_truncate(const char *path,off_t offset)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=INT2FIX(offset);
  rb_protect((VALUE (*)())unsafe_truncate,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}


/*
   Change access/modification times of a file

   @overload utime(context,path,actime,modtime)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#cb7452acad1002d418409892b6e54c2e utime}
   @deprecated See {#utimens}

   @param [Context] context
   @param [String] path
   @param [Integer] actime access time
   @param [Integer] modtime modification time

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_utime(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("utime"),4,&args[1]);
}

static int rf_utime(const char *path,struct utimbuf *utim)
{
  VALUE args[5];
  int error = 0;
  
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=INT2NUM(utim->actime);
  args[4]=INT2NUM(utim->modtime);
  rb_protect((VALUE (*)())unsafe_utime,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Change file ownership

   @overload chown(context,path,uid,gid)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#40421f8a43e903582c49897894f4692d chown}

   @param [Context] context
   @param [String] path
   @param [Integer] uid new user id
   @param [Integer] gid new group id

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_chown(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("chown"),4,&args[1]);
}

static int rf_chown(const char *path,uid_t uid,gid_t gid)
{
  VALUE args[5];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=INT2FIX(uid);
  args[4]=INT2FIX(gid);
  rb_protect((VALUE (*)())unsafe_chown,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Change file permissions

   @overload chmod(context,path,mode)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#7e75d299efe3a401e8473af7028e5cc5 chmod}

   @param [Context] context
   @param [String] path
   @param [Integer] mode

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_chmod(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("chmod"),3,&args[1]);
}

static int rf_chmod(const char *path,mode_t mode)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=INT2FIX(mode);
  rb_protect((VALUE (*)())unsafe_chmod,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Remove a file

   @overload unlink(context,path)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#8bf63301a9d6e94311fa10480993801e unlink}

   @param [Context] context
   @param [String] path

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_unlink(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("unlink"),2,&args[1]);
}

static int rf_unlink(const char *path)
{
  VALUE args[3];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  rb_protect((VALUE (*)())unsafe_unlink,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Remove a directory

   @overload rmdir(context,path)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#c59578d18db12f0142ae1ab6e8812d55 rmdir}

   @param [Context] context
   @param [String] path

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_rmdir(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("rmdir"),2,&args[1]);
}

static int rf_rmdir(const char *path)
{
  VALUE args[3];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  rb_protect((VALUE (*)())unsafe_rmdir, (VALUE) args ,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   Create a symbolic link

   @overload symlink(context,to,from)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#b86022391e56a8ad3211cf754b5b5ebe symlink}

   @param [Context] context
   @param [String] to
   @param [String] from

   @return [void]
   @raise [Errno]

   Create a symbolic link named "from" which, when evaluated, will lead to "to". 
*/
static VALUE unsafe_symlink(VALUE *args){
  return rb_funcall3(args[0],rb_intern("symlink"),3,&args[1]);
}

static int rf_symlink(const char *path,const char *as)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=rb_str_new2(as);
  rb_filesystem_encode(args[3]);
  
  rb_protect((VALUE (*)())unsafe_symlink,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}

/*
   @overload rename(context,from,to)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#a777cbddc91887b117ac414e9a2d3cb5 rename}

   @param [Context] context
   @param [String] from
   @param [String] to

   @return [void]
   @raise [Errno]

   Rename the file, directory, or other object "from" to the target "to". Note that the source and target don't have to be in the same directory, so it may be necessary to move the source to an entirely new directory. See rename(2) for full details.
*/
static VALUE unsafe_rename(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("rename"),3,&args[1]);
}

static int rf_rename(const char *path,const char *as)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  args[3]=rb_str_new2(as);
  rb_filesystem_encode(args[3]);
  
  rb_protect((VALUE (*)())unsafe_rename,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}


/*
   Create a hard link to file
   @overload link(context,from,to)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#1b234c43e826c6a690d80ea895a17f61 link}

   @param [Context] context
   @param [String] from
   @param [String] to

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_link(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("link"),3,&args[1]);
}

static int rf_link(const char *path,const char * as)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=rb_str_new2(as);
  rb_filesystem_encode(args[3]);
  rb_protect((VALUE (*)())unsafe_link,(VALUE) args,&error);

  return error ? -(return_error(ENOENT)) : 0;
}


/*
   Read data from an open file

   @overload read(context,path,size,offset,ffi)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#2a1c6b4ce1845de56863f8b7939501b5 read}

   @param [Context] context
   @param [String] path
   @param [Integer] size
   @param [Integer] offset
   @param [FileInfo] ffi

   @return [String] should be exactly the number of bytes requested, or empty string on EOF
   @raise [Errno]
*/
static VALUE unsafe_read(VALUE *args)
{
  VALUE res;
  
  res = rb_funcall3(args[0],rb_intern("read"),5,&args[1]);
  //TODO If res does not implement to_str then you'll get an exception here that
  //is hard to debug
  return StringValue(res);
}


long rb_strcpy(VALUE str, char *buf, size_t size)
{
    long length;

    length = RSTRING_LEN(str);
    if (length <= (long) size)
    { 
        memcpy(buf,RSTRING_PTR(str),length);
    }

    return length;
}

static int rf_read(const char *path,char * buf, size_t size,off_t offset,struct fuse_file_info *ffi)
{
  VALUE args[6];
  VALUE res;
  int error = 0;
  long length=0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=SIZET2NUM(size);
  args[4]=OFFT2NUM(offset);
  args[5]=get_file_info(ffi);

  res=rb_protect((VALUE (*)())unsafe_read,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
   
   length = rb_strcpy(res,buf,size);

   if (length <= (long) size) {
       return length;
   } else {
      //This cannot happen => IO error.
      return -(return_error(ENOENT));
   }    

  }
}


/*
   Write data to an open file

   @overload write(context,path,data,offset,ffi)

   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#897d1ece4b8b04c92d97b97b2dbf9768 write}

   @param [Context] context
   @param [String] path
   @param [String] data
   @param [Integer] offset
   @param [FileInfo] ffi

   @return [Integer] exactly the number of bytes requested except on error
   @raise [Errno]
*/
static VALUE unsafe_write(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("write"),5,&args[1]);
}

static int rf_write(const char *path,const char *buf,size_t size,
  off_t offset,struct fuse_file_info *ffi)
{
  VALUE args[6];
  VALUE res;
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  args[3]=rb_str_new(buf, size);
  args[4]=OFFT2NUM(offset);
  args[5]=get_file_info(ffi);

  res = rb_protect((VALUE (*)())unsafe_write,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return NUM2INT(res);
  }
}

/*
   Get file system statistics

   @overload statfs(context,path)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#4e765e29122e7b6b533dc99849a52655 statfs}
   @param [Context] context
   @param [String] path

   @return [StatVfs] or something that quacks like a statVfs
   @raise [Errno]

   The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored

*/
static VALUE unsafe_statfs(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("statfs"),2,&args[1]);
}

static int rf_statfs(const char * path, struct statvfs * vfsinfo)
{
  VALUE args[3];
  VALUE res;
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  res = rb_protect((VALUE (*)())unsafe_statfs,(VALUE) args,&error);

  if (error || (res == Qnil))
  {
    return -(return_error(ENOENT));
  }
  else
  {
    rstatvfs2statvfs(res,vfsinfo);
    return 0;
  }
}


/*
   Set extended attributes 

   @overload setxattr(context,path,name,data,flags)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#988ced7091c2821daa208e6c96d8b598 setxattr}
   @param [Context] context
   @param [String] path
   @param [String] name
   @param [String] data
   @param [Integer] flags

   @return [void]
   @raise [Errno]

*/
static VALUE unsafe_setxattr(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("setxattr"),5,&args[1]);
}

static int rf_setxattr(const char *path,const char *name,
           const char *value, size_t size, int flags)
{
  VALUE args[6];
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=rb_str_new2(name);
  args[4]=rb_str_new(value,size);
  args[5]=INT2NUM(flags);

  rb_protect((VALUE (*)())unsafe_setxattr,(VALUE) args,&error);
  
  return error ? -(return_error(ENOENT)) : 0;
}


/*
   Get extended attribute 

   @overload getxattr(context,path,name)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#e21503c64fe2990c8a599f5ba339a8f2 getxattr}
   @param [Context] context
   @param [String] path
   @param [String] name

   @return [String] attribute value
   @raise [Errno] Errno::ENOATTR if attribute does not exist

*/
static VALUE unsafe_getxattr(VALUE *args)
{
  VALUE res;
  res = rb_funcall3(args[0],rb_intern("getxattr"),3,&args[1]);
  //TODO - exception won't should that we're calling getxattr
  return StringValue(res);
}

static int rf_getxattr(const char *path,const char *name,char *buf,
           size_t size)
{
  VALUE args[4];
  VALUE res;
  int error = 0;
  long length;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  args[3]=rb_str_new2(name);
  res=rb_protect((VALUE (*)())unsafe_getxattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    length = rb_strcpy(res,buf,size);
    if (buf != NULL && length > ((long) size))
    {
       return -ERANGE;
    }
    return length;
  }
}

/*
   List extended attributes

   @overload listxattr(context,path)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#b4a9c361ce48406f07d5a08ab03f5de8 listxattr}
   @param [Context] context
   @param [String] path

   @return [Array<String>] list of attribute names
   @raise [Errno]
*/
static VALUE unsafe_listxattr(VALUE *args)
{
  VALUE res;
  res = rb_funcall3(args[0],rb_intern("listxattr"),2,&args[1]);

  //We'll let Ruby do the hard work of creating a String
  //separated by NULLs
  return rb_funcall(mRFuse,rb_intern("packxattr"),1,res);
}

static int rf_listxattr(const char *path,char *buf, size_t size)
{
  VALUE args[3];
  VALUE res;
  int error = 0;
  long length;
  
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  
  res=rb_protect((VALUE (*)())unsafe_listxattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    length = rb_strcpy(res,buf,size);
    if (buf != NULL && length > (long) size)
    {
       return -ERANGE;
    }
    return length;
  }
}


/*
   Remove extended attribute

   @overload removexattr(context,path,name)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#5e54de801a0e0d7019e4579112ecc477 removexattr}
   @param [Context] context
   @param [String] path
   @param [String] name attribute to remove

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_removexattr(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("removexattr"),3,&args[1]);
}

static int rf_removexattr(const char *path,const char *name)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=rb_str_new2(name);
  rb_protect((VALUE (*)())unsafe_removexattr,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}


/*
   Open directory

   @overload opendir(context,path,name)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#1813889bc5e6e0087a936b7abe8b923f opendir}
   @param [Context] context
   @param [String] path
   @param [FileInfo] ffi

   @return [void]
   @raise [Errno]


   Unless the 'default_permissions' mount option is given, this method should check if opendir is permitted for this directory. Optionally opendir may also return an arbitrary filehandle in the fuse_file_info structure, which will be available to {#readdir},  {#fsyncdir}, {#releasedir}.

*/
static VALUE unsafe_opendir(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("opendir"),3,&args[1]);
}

static int rf_opendir(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);

  args[3]=wrap_file_info(ctx,ffi);
  
  rb_protect((VALUE (*)())unsafe_opendir,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
   Release directory

   @overload releasedir(context,path,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#729e53d36acc05a7a8985a1a3bbfac1e releasedir}
   @param [Context] context
   @param [String] path
   @param [FileInfo] ffi

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_releasedir(VALUE *args)
{
 return rb_funcall3(args[0],rb_intern("releasedir"),3,&args[1]);
}

static int rf_releasedir(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[4];
  int error = 0;

  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=release_file_info(ctx,ffi);
  
  rb_protect((VALUE (*)())unsafe_releasedir,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}


/*
   Sync directory

   @overload fsyncdir(context,path,datasync,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#ba5cc1fe9a63ec152ceb19656f243256 fsyncdir}
   @param [Context] context
   @param [String] path
   @param [Integer] datasync  if nonzero sync only data, not metadata
   @param [FileInfo] ffi

   @return [void]
   @raise [Errno]

*/
static VALUE unsafe_fsyncdir(VALUE *args)
{
  return rb_funcall(args[0],rb_intern("fsyncdir"),4,&args[1]);
}

static int rf_fsyncdir(const char *path,int meta,struct fuse_file_info *ffi)
{
  VALUE args[5];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3]=INT2NUM(meta);
  args[4]=get_file_info(ffi);
  rb_protect((VALUE (*)())unsafe_fsyncdir,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
   Called when filesystem is initialised

   @overload init(info)
   @abstract Fuse Operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#dc6dc71274f185de72217e38d62142c4 init}

   @param [Context] context
   @param [Struct] info connection information

   @return [void]
*/
static VALUE unsafe_init(VALUE* args)
{
  return rb_funcall3(args[0],rb_intern("init"),2,&args[1]);
}

static void *rf_init(struct fuse_conn_info *conn)
{
  VALUE args[3];
  int error = 0;
  struct fuse_context *ctx;
 
  VALUE self, s, fci, fcio;

  ctx = fuse_get_context();

  self = (VALUE) ctx->private_data;

  args[0] = self;
  args[1] = wrap_context(ctx);

  //Create a struct for the conn_info
  //TODO - some of these are writable!
  s  = rb_const_get(rb_cObject,rb_intern("Struct"));
  fci = rb_funcall(s,rb_intern("new"),7,
    ID2SYM(rb_intern("proto_major")),
    ID2SYM(rb_intern("proto_minor")),
    ID2SYM(rb_intern("async_read")),
    ID2SYM(rb_intern("max_write")),
    ID2SYM(rb_intern("max_readahead")),
    ID2SYM(rb_intern("capable")),
    ID2SYM(rb_intern("want"))
  );

  fcio = rb_funcall(fci,rb_intern("new"),7,
    UINT2NUM(conn->proto_major),
    UINT2NUM(conn->proto_minor),
    UINT2NUM(conn->async_read),
    UINT2NUM(conn->max_write),
    UINT2NUM(conn->max_readahead),
    UINT2NUM(conn->capable),
    UINT2NUM(conn->want)
  );

  args[2] = fcio;

  rb_protect((VALUE (*)())unsafe_init,(VALUE) args,&error);

  //This previously was the result of the init call (res)
  //but it was never made available to any of the file operations
  //and nothing was done to prevent it from being GC'd so
  //filesystems would need to have stored it separately anyway
  return error ? NULL : (void *) self;
}

/*
   Check access permissions

   @overload access(context,path,mode)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#2248db35e200265f7fb9a18348229858 access}
   @param [Context] context
   @param [String] path
   @param [Integer] mode the permissions to check

   @return [void]
   @raise [Errno::EACCESS] if the requested permission isn't available

*/
static VALUE unsafe_access(VALUE* args)
{
  return rb_funcall3(args[0],rb_intern("access"),3,&args[1]);
}

static int rf_access(const char *path, int mask)
{
  VALUE args[4];
  int error = 0;
  struct fuse_context *ctx=fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3] = INT2NUM(mask);
  rb_protect((VALUE (*)())unsafe_access,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}


/*
   Create and open a file

   @overload create(context,path,mode,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#97243e0f9268a96236bc3b6f2bacee17 create}
   @param [Context] context
   @param [String] path
   @param [Integer] mode the file permissions to create
   @param [Fileinfo] ffi - use the {FileInfo#fh} attribute to store a filehandle

   @return [void]
   @raise [Errno]

   If the file does not exist, first create it with the specified mode, and then open it.

*/
static VALUE unsafe_create(VALUE* args)
{
  return rb_funcall3(args[0],rb_intern("create"),4,&args[1]);
}

static int rf_create(const char *path, mode_t mode, struct fuse_file_info *ffi)
{
  VALUE args[5];
  int error = 0;

  struct fuse_context *ctx = fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3] = INT2NUM(mode);
  args[4] = wrap_file_info(ctx,ffi);

  rb_protect((VALUE (*)())unsafe_create,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;

}


/*
   Change the size of an open file

   @overload ftruncate(context,path,size,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#1e492882859740f13cbf3344cf963c70 ftruncate}
   @param [Context] context
   @param [String] path
   @param [Integer] size
   @param [Fileinfo] ffi 

   @return [void]
   @raise [Errno] 

*/
static VALUE unsafe_ftruncate(VALUE* args)
{
  return rb_funcall3(args[0],rb_intern("ftruncate"),4,&args[1]);
}

static int rf_ftruncate(const char *path, off_t size,
  struct fuse_file_info *ffi)
{
  VALUE args[5];
  int error = 0;

  struct fuse_context *ctx = fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3] = OFFT2NUM(size);
  args[4] = get_file_info(ffi);

  rb_protect((VALUE (*)())unsafe_ftruncate,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}


/*
   Get attributes of an open file

   @overload fgetattr(context,path,ffi)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#573d79862df591c98e1685225a4cd3a5 fgetattr}
   @param [Context] context
   @param [String] path
   @param [Fileinfo] ffi 

   @return [Stat] file attributes
   @raise [Errno] 

*/
static VALUE unsafe_fgetattr(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("fgetattr"),3,&args[1]);
}

static int rf_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *ffi)
{
  VALUE args[4];
  VALUE res;
  int error = 0;

  struct fuse_context *ctx = fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3] = get_file_info(ffi);

  res=rb_protect((VALUE (*)())unsafe_fgetattr,(VALUE) args,&error);

  if (error || (res == Qnil))
  {
    return -(return_error(ENOENT));
  }
  else
  {
    rstat2stat(res,stbuf);
    return 0;
  }
}

/*
   Perform POSIX file locking operation

   @overload lock(context,path,ffi,cmd,flock)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#1c3fff5cf0c1c2003d117e764b9a76fd lock}
   @param [Context] context
   @param [String] path
   @param [Fileinfo] ffi 
   @param [Integer] cmd
   @param [Struct] flock

   @return [void]
   @raise [Errno]
   @todo Some of the attributes of the flock struct should be writable

   The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.

   For the meaning of fields in 'struct flock' see the man page for fcntl(2). The l_whence field will always be set to SEEK_SET.

   For checking lock ownership, the FileInfo#owner argument must be used. (NOT YET IMPLEMENTED)

   For F_GETLK operation, the library will first check currently held locks, and if a conflicting lock is found it will return information without calling this method. This ensures, that for local locks the l_pid field is correctly filled in. The results may not be accurate in case of race conditions and in the presence of hard links, but it's unlikly that an application would rely on accurate GETLK results in these cases. If a conflicting lock is not found, this method will be called, and the filesystem may fill out l_pid by a meaningful value, or it may leave this field zero.

   For F_SETLK and F_SETLKW the l_pid field will be set to the pid of the process performing the locking operation.

Note: if this method is not implemented, the kernel will still allow file locking to work locally. Hence it is only interesting for network filesystems and similar.
*/
static VALUE unsafe_lock(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("lock"),5,&args[1]);
}

static int rf_lock(const char *path, struct fuse_file_info *ffi,
  int cmd, struct flock *lock)
{
  VALUE args[6];
  int error = 0;

  VALUE cStruct;
  VALUE lockc;
  VALUE locko;

  struct fuse_context *ctx = fuse_get_context();
  init_context_path_args(args,ctx,path);
  

  //Create a struct for the lock structure
  cStruct  = rb_const_get(rb_cObject,rb_intern("Struct"));
  lockc    = rb_funcall(cStruct,rb_intern("new"),5,
    ID2SYM(rb_intern("l_type")),
    ID2SYM(rb_intern("l_whence")),
    ID2SYM(rb_intern("l_start")),
    ID2SYM(rb_intern("l_len")),
    ID2SYM(rb_intern("l_pid"))
  );

  locko = rb_funcall(lockc,rb_intern("new"),5,
    UINT2NUM(lock->l_type),
    UINT2NUM(lock->l_whence),
    UINT2NUM(lock->l_start),
    UINT2NUM(lock->l_len),
    UINT2NUM(lock->l_pid)
  );

  args[3] = get_file_info(ffi);
  args[4] = INT2NUM(cmd);
  args[5] = locko;

  rb_protect((VALUE (*)())unsafe_lock,(VALUE) args,&error);

  return error ?  -(return_error(ENOENT)) : 0 ;
}


/*
   Change access/modification times of a file

   @overload utimens(context,path,actime,modtime)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#79955861cc5eb006954476607ef28944 utimens}

   @param [Context] context
   @param [String] path
   @param [Integer] actime access time in nanoseconds
   @param [Integer] modtime modification time in nanoseconds

   @return [void]
   @raise [Errno]
*/
static VALUE unsafe_utimens(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("utimens"),4,&args[1]);
}

static int rf_utimens(const char * path, const struct timespec tv[2])
{
  VALUE args[5];
  int   error = 0;

  struct fuse_context *ctx = fuse_get_context();
  init_context_path_args(args,ctx,path);

  // tv_sec * 1000000000 + tv_nsec
  args[3] = rb_funcall(
    rb_funcall(
      INT2NUM(tv[0].tv_sec), rb_intern("*"), 1, INT2NUM(1000000000)
    ),
    rb_intern("+"), 1, INT2NUM(tv[0].tv_nsec)
  );

  args[4] = rb_funcall(
    rb_funcall(
      INT2NUM(tv[1].tv_sec), rb_intern("*"), 1, INT2NUM(1000000000)
    ),
    rb_intern("+"), 1, INT2NUM(tv[1].tv_nsec)
  );
  
  rb_protect((VALUE (*)())unsafe_utimens,(VALUE) args, &error);
  return error ?  -(return_error(ENOENT)) : 0 ;
}

/*
   Map block index within file to block index within device

   @overload bmap(context,path,blocksize,index)
   @abstract Fuse operation {http://fuse.sourceforge.net/doxygen/structfuse__operations.html#e3f3482e33a0eada0292350d76b82901 bmap}

   @param [Context] context
   @param [String] path
   @param [Integer] blocksize
   @param [Integer] index

   @return [Integer] device relative block index
   @raise [Errno]


   Note: This makes sense only for block device backed filesystems mounted with the 'blkdev' option
*/
static VALUE unsafe_bmap(VALUE *args)
{
  return rb_funcall3(args[0],rb_intern("bmap"),4,&args[1]);
}

static int rf_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
  VALUE args[5];
  VALUE res;
  int   error = 0;

  struct fuse_context *ctx = fuse_get_context();
  init_context_path_args(args,ctx,path);
  args[3] = SIZET2NUM(blocksize);
  args[4] = LL2NUM(*idx);

  res = rb_protect((VALUE (*)())unsafe_bmap,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    *idx = NUM2LL(res);
    return 0;
  }
}

//----------------------IOCTL
#ifdef RFUSE_BROKEN
static VALUE unsafe_ioctl(VALUE *args)
{
  VALUE path  = args[0];
  VALUE cmd   = args[1];
  VALUE arg   = args[2];
  VALUE ffi   = args[3];
  VALUE flags = args[4];
  VALUE data  = args[5];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall((VALUE) ctx->private_data, rb_intern("ioctl"), 7, wrap_context(ctx),
    path, cmd, arg, ffi, flags, data);
}

static int rf_ioctl(const char *path, int cmd, void *arg,
  struct fuse_file_info *ffi, unsigned int flags, void *data)
{
  VALUE args[6];
  VALUE res;
  int   error = 0;

  args[0] = rb_str_new2(path);
  rb_filesystem_encode(args[0]);
  args[1] = INT2NUM(cmd);
  args[2] = wrap_buffer(arg);
  args[3] = get_file_info(ffi);
  args[4] = INT2NUM(flags);
  args[5] = wrap_buffer(data);

  res = rb_protect((VALUE (*)())unsafe_ioctl,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }

  return 0;
}
#endif

//----------------------POLL
#ifdef RFUSE_BROKEN
static VALUE unsafe_poll(VALUE *args)
{
  VALUE path     = args[0];
  VALUE ffi      = args[1];
  VALUE ph       = args[2];
  VALUE reventsp = args[3];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall( ctx->private_data, rb_intern("poll"), 5, wrap_context(ctx),
    path, ffi, ph, reventsp);
}

static int rf_poll(const char *path, struct fuse_file_info *ffi,
  struct fuse_pollhandle *ph, unsigned *reventsp)
{
  VALUE args[4];
  VALUE res;
  int   error = 0;

  args[0] = rb_str_new2(path);
  rb_filesystem_encode(args[0]);
  args[1] = get_file_info(ffi);
  args[2] = wrap_pollhandle(ph);
  args[3] = INT2NUM(*reventsp);

  res = rb_protect((VALUE (*)())unsafe_poll,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    *reventsp = NUM2INT(args[3]);
  }
  return 0;
}
#endif

/* 
   Is the filesystem successfully mounted

   @return [Boolean] true if mounted, false otherwise

*/
static VALUE rf_mounted(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
 
  // Never mounted, unmounted via fusermount, or via rf_unmount
  return (inf->fuse == NULL || fuse_exited(inf->fuse) ) ? Qfalse : Qtrue; 
}

/*
   Unmount filesystem
*/
VALUE rf_unmount(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);

  rb_funcall(self,rb_intern("ruby_unmount"),0);

  if (inf->fuse != NULL)  {
      fuse_exit(inf->fuse);
  }

  if (inf->fc != NULL) {
      fuse_unmount(inf->mountpoint, inf->fc);
      inf->fc = NULL;
  }
  return Qnil;
}

/* 
   @return [String] directory where this filesystem is mounted
*/
VALUE rf_mountname(VALUE self)
{
  struct intern_fuse *inf;
  VALUE result;
  
  Data_Get_Struct(self,struct intern_fuse,inf);

  result = rb_str_new2(inf->mountpoint);
  rb_filesystem_encode(result);

  return result;
}

/* 
  @deprecated obsolete in FUSE itself
*/
VALUE rf_invalidate(VALUE self,VALUE path)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  return fuse_invalidate(inf->fuse,StringValueCStr(path)); 
}

/*
   @return [Integer] /dev/fuse file descriptor for use with IO.select and {#process}
   @raise [RFuse::Error] if fuse not mounted   
*/
VALUE rf_fd(VALUE self)
{
 struct intern_fuse *inf;
 Data_Get_Struct(self,struct intern_fuse,inf);
 if (inf->fuse == NULL) {
   rb_raise(eRFuse_Error,"FUSE not mounted");
   return Qnil;
 } else {
   return INT2NUM(intern_fuse_fd(inf));
 }
}

/*
 Process one fuse command from the kernel

 @return [Integer] 0 if successful
 @raise [RFuse::Error] if fuse not mounted   
*/
VALUE rf_process(VALUE self)
{
 struct intern_fuse *inf;
 Data_Get_Struct(self,struct intern_fuse,inf);
 if (inf->fuse == NULL) {
   rb_raise(eRFuse_Error,"FUSE not mounted");
   return Qnil;
 } else {
    return INT2NUM(intern_fuse_process(inf));
 }
}

#define RESPOND_TO(obj,methodname) \
  rb_funcall( \
    obj,rb_intern("respond_to?"), \
    1, rb_str_new2(methodname) \
  ) == Qtrue

/*
* initialize and mount the filesystem
* @overload initialize(mountpoint,*options)
* @param [String] mountpoint The mountpoint
* @param [Array<String>] options fuse arguments (-h to see a list)
*/
static VALUE rf_initialize(
  int argc,
  VALUE* argv,
  VALUE self)
{

  VALUE opts;
  VALUE first_opt;
  VALUE mountpoint_arg;
  VALUE mountpoint;
  struct intern_fuse *inf;
  int init_result;
  struct fuse_args *args;

  rb_scan_args(argc, argv, "1*",&mountpoint_arg, &opts);

  // Allow pre 1.0.6 style Fuse which forced options to be passed as an array
  if (RARRAY_LEN(opts) == 1) {
      first_opt = rb_ary_entry(opts,0);
      if (TYPE(first_opt) == T_ARRAY) {
         opts = first_opt;
      }
  }
  //Allow things like Pathname to be sent as a mountpoint
  mountpoint = rb_obj_as_string(mountpoint_arg);

  //Is this redundant if scan_args has populted opts?
  Check_Type(opts, T_ARRAY);

  Data_Get_Struct(self,struct intern_fuse,inf);

  inf->mountpoint = strdup(StringValueCStr(mountpoint));
  
  args = rarray2fuseargs(opts);

  if (RESPOND_TO(self,"getattr"))
    inf->fuse_op.getattr     = rf_getattr;
  if (RESPOND_TO(self,"readlink"))
    inf->fuse_op.readlink    = rf_readlink;
  if (RESPOND_TO(self,"getdir"))
    inf->fuse_op.getdir      = rf_getdir;  // Deprecated
  if (RESPOND_TO(self,"mknod"))
    inf->fuse_op.mknod       = rf_mknod;
  if (RESPOND_TO(self,"mkdir"))
    inf->fuse_op.mkdir       = rf_mkdir;
  if (RESPOND_TO(self,"unlink"))
    inf->fuse_op.unlink      = rf_unlink;
  if (RESPOND_TO(self,"rmdir"))
    inf->fuse_op.rmdir       = rf_rmdir;
  if (RESPOND_TO(self,"symlink"))
    inf->fuse_op.symlink     = rf_symlink;
  if (RESPOND_TO(self,"rename"))
    inf->fuse_op.rename      = rf_rename;
  if (RESPOND_TO(self,"link"))
    inf->fuse_op.link        = rf_link;
  if (RESPOND_TO(self,"chmod"))
    inf->fuse_op.chmod       = rf_chmod;
  if (RESPOND_TO(self,"chown"))
    inf->fuse_op.chown       = rf_chown;
  if (RESPOND_TO(self,"truncate"))
    inf->fuse_op.truncate    = rf_truncate;
  if (RESPOND_TO(self,"utime"))
    inf->fuse_op.utime       = rf_utime;    // Deprecated
  if (RESPOND_TO(self,"open")) {
    inf->fuse_op.open        = rf_open;
    inf->fuse_op.release     = rf_release_ffi; // remove open file reference
  }
  if (RESPOND_TO(self,"create")) {
    inf->fuse_op.create      = rf_create;
    inf->fuse_op.release     = rf_release_ffi; // remove open file reference
  }
  if (RESPOND_TO(self,"read"))
    inf->fuse_op.read        = rf_read;
  if (RESPOND_TO(self,"write"))
    inf->fuse_op.write       = rf_write;
  if (RESPOND_TO(self,"statfs"))
    inf->fuse_op.statfs      = rf_statfs;
  if (RESPOND_TO(self,"flush"))
    inf->fuse_op.flush       = rf_flush;
  if (RESPOND_TO(self,"release"))
    inf->fuse_op.release     = rf_release;
  if (RESPOND_TO(self,"fsync"))
    inf->fuse_op.fsync       = rf_fsync;
  if (RESPOND_TO(self,"setxattr"))
    inf->fuse_op.setxattr    = rf_setxattr;
  if (RESPOND_TO(self,"getxattr"))
    inf->fuse_op.getxattr    = rf_getxattr;
  if (RESPOND_TO(self,"listxattr"))
    inf->fuse_op.listxattr   = rf_listxattr;
  if (RESPOND_TO(self,"removexattr"))
    inf->fuse_op.removexattr = rf_removexattr;
  if (RESPOND_TO(self,"opendir")) {
    inf->fuse_op.opendir     = rf_opendir;
    inf->fuse_op.release     = rf_release_ffi; // remove open file reference
  }
  if (RESPOND_TO(self,"readdir"))
    inf->fuse_op.readdir     = rf_readdir;
  if (RESPOND_TO(self,"releasedir"))
    inf->fuse_op.releasedir  = rf_releasedir;
  if (RESPOND_TO(self,"fsyncdir"))
    inf->fuse_op.fsyncdir    = rf_fsyncdir;
  if (RESPOND_TO(self,"init"))
    inf->fuse_op.init        = rf_init;
  if (RESPOND_TO(self,"access"))
    inf->fuse_op.access      = rf_access;
  if (RESPOND_TO(self,"ftruncate"))
    inf->fuse_op.ftruncate   = rf_ftruncate;
  if (RESPOND_TO(self,"fgetattr"))
    inf->fuse_op.fgetattr    = rf_fgetattr;
  if (RESPOND_TO(self,"lock"))
    inf->fuse_op.lock        = rf_lock;
  if (RESPOND_TO(self,"utimens"))
    inf->fuse_op.utimens     = rf_utimens;
  if (RESPOND_TO(self,"bmap"))
    inf->fuse_op.bmap        = rf_bmap;
#ifdef RFUSE_BROKEN
  if (RESPOND_TO(self,"ioctl"))
    inf->fuse_op.ioctl       = rf_ioctl;
  if (RESPOND_TO(self,"poll"))
    inf->fuse_op.poll        = rf_poll;
#endif


  // init_result indicates not mounted, but so does inf->fuse == NULL
  // raise exceptions only if we try to use the mount
  // can test with mounted?
  // Note we are storing this Ruby object as the FUSE user_data
  init_result = intern_fuse_init(inf, args, (void *) self);

  //Create the open files hash where we cache FileInfo objects
  if (init_result == 0) {
    VALUE open_files_hash;
    open_files_hash=rb_hash_new();
    rb_iv_set(self,"@open_files",open_files_hash);
    rb_funcall(self,rb_intern("ruby_initialize"),0);
  }
  return self;
}

static VALUE rf_new(VALUE class)
{
  struct intern_fuse *inf;
  VALUE self;
  inf = intern_fuse_new();
  self=Data_Wrap_Struct(class, 0, intern_fuse_destroy, inf);
  return self;
}

/*
* Document-class: RFuse::Fuse
* 
* A FUSE filesystem - extend this class implementing
* the various abstract methods to provide your filesystem.
*
* All file operations take a {Context} and a path as well as any other necessary parameters
* 
* Mount your filesystem by creating an instance of your subclass and call #loop to begin processing
*/
void rfuse_init(VALUE module)
{
#if 0
    //Trick Yardoc
    mRFuse = rb_define_mRFuse("RFuse");
#endif
  VALUE cFuse;

  mRFuse = module;
 
  // The underlying FUSE library major version
  rb_define_const(mRFuse,"FUSE_MAJOR_VERSION",INT2FIX(FUSE_MAJOR_VERSION));
  // The underlyfing FUSE library minor versoin
  rb_define_const(mRFuse,"FUSE_MINOR_VERSION",INT2FIX(FUSE_MINOR_VERSION));
      
  cFuse = rb_define_class_under(mRFuse,"Fuse",rb_cObject);

  rb_define_alloc_func(cFuse,rf_new);

  rb_define_method(cFuse,"initialize",rf_initialize,-1);
  rb_define_method(cFuse,"mounted?",rf_mounted,0);
  rb_define_method(cFuse,"invalidate",rf_invalidate,1);
  rb_define_method(cFuse,"unmount",rf_unmount,0);
  rb_define_method(cFuse,"mountname",rf_mountname,0);
  rb_define_alias(cFuse,"mountpoint","mountname");
  rb_define_method(cFuse,"fd",rf_fd,0);
  rb_define_method(cFuse,"process",rf_process,0);
  rb_attr(cFuse,rb_intern("open_files"),1,0,Qfalse);

  eRFuse_Error = rb_define_class_under(mRFuse,"Error",rb_eStandardError);

#if 0
  //Trick Yarddoc into documenting abstract fuseoperations
  rb_define_method(cFuse,"readdir",unsafe_readdir,0);
  rb_define_method(cFuse,"readlink",unsafe_readlink,0);
  rb_define_method(cFuse,"getdir",unsafe_getdir,0);
  rb_define_method(cFuse,"mknod",unsafe_mknod,0);
  rb_define_method(cFuse,"getattr",unsafe_getattr,0);
  rb_define_method(cFuse,"mkdir",unsafe_mkdir,0);
  rb_define_method(cFuse,"open",unsafe_open,0);
  rb_define_method(cFuse,"release",unsafe_release,0);
  rb_define_method(cFuse,"fsync",unsafe_fsync,0);
  rb_define_method(cFuse,"flush",unsafe_flush,0);
  rb_define_method(cFuse,"truncate",unsafe_truncate,0);
  rb_define_method(cFuse,"utime",unsafe_utime,0);
  rb_define_method(cFuse,"chown",unsafe_chown,0);
  rb_define_method(cFuse,"chmod",unsafe_chmod,0);
  rb_define_method(cFuse,"unlink",unsafe_unlink,0);
  rb_define_method(cFuse,"symlink",unsafe_symlink,0);
  rb_define_method(cFuse,"rename",unsafe_rename,0);
  rb_define_method(cFuse,"link",unsafe_link,0);
  rb_define_method(cFuse,"read",unsafe_read,0);
  rb_define_method(cFuse,"write",unsafe_write,0);
  rb_define_method(cFuse,"statfs",unsafe_statfs,0);
  rb_define_method(cFuse,"setxattr",unsafe_setxattr,0);
  rb_define_method(cFuse,"getxattr",unsafe_getxattr,0);
  rb_define_method(cFuse,"listxattr",unsafe_listxattr,0);
  rb_define_method(cFuse,"removexattr",unsafe_removexattr,0);
  rb_define_method(cFuse,"opendir",unsafe_opendir,0);
  rb_define_method(cFuse,"releasedir",unsafe_releasedir,0);
  rb_define_method(cFuse,"fsyncdir",unsafe_fsyncdir,0);
  rb_define_method(cFuse,"init",unsafe_init,0);
  rb_define_method(cFuse,"access",unsafe_access,0);
  rb_define_method(cFuse,"create",unsafe_create,0);
  rb_define_method(cFuse,"ftruncate",unsafe_ftruncate,0);
  rb_define_method(cFuse,"fgetattr",unsafe_fgetattr,0);
  rb_define_method(cFuse,"lock",unsafe_lock,0);
  rb_define_method(cFuse,"utimens",unsafe_utimens,0);
  rb_define_method(cFuse,"bmap",unsafe_bmap,0);
#endif
}
