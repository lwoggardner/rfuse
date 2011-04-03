#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif
//FOR LINUX ONLY
#include <linux/stat.h> 

#include <ruby.h>
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

//this is a global variable where we store the fuse object
static VALUE fuse_object;

#if !defined(STR2CSTR)
  #define STR2CSTR(X) StringValuePtr(X) 
#endif

// The ugliest hack I've seen today
#if HAVE_RUBY_RUBY_H
  static VALUE ruby_errinfo()
  {
    return rb_errinfo();
  }
#endif

static int unsafe_return_error(VALUE *args)
{
 
  if (rb_respond_to(ruby_errinfo,rb_intern("errno"))) {
    //We expect these and they get passed on the fuse so be quiet...
    return rb_funcall(ruby_errinfo,rb_intern("errno"),0);
  } else {
    VALUE info;
    info = rb_inspect(ruby_errinfo);
    printf ("ERROR: Exception %s not an Errno:: !respond_to?(:errno) \n",STR2CSTR(info)); 
    //We need the ruby_errinfo backtrace not fuse.loop ... rb_backtrace();
    VALUE bt_ary = rb_funcall(ruby_errinfo, rb_intern("backtrace"),0);
    int c;
    for (c=0;c<RARRAY(bt_ary)->len;c++) {
      printf("%s\n",RSTRING(RARRAY(bt_ary)->ptr[c])->ptr);
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

//----------------------READDIR

static VALUE unsafe_readdir(VALUE *args)
{
  VALUE path   = args[0];
  VALUE filler = args[1];
  VALUE offset = args[2];
  VALUE ffi    = args[3];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("readdir"),5,wrap_context(ctx),path,filler,
        offset,ffi);
}

//call readdir with an Filler object
static int rf_readdir(const char *path, void *buf,
  fuse_fill_dir_t filler, off_t offset,struct fuse_file_info *ffi)
{
  VALUE fuse_module;
  VALUE rfiller_class;
  VALUE rfiller_instance;
  VALUE args[4];
  VALUE res;
  struct filler_t *fillerc;
  int error = 0;

  //create a filler object
  args[0]=rb_str_new2(path);

  fuse_module = rb_const_get(rb_cObject, rb_intern("RFuse"));

  rfiller_class=rb_const_get(fuse_module,rb_intern("Filler"));
  rfiller_instance=rb_funcall(rfiller_class,rb_intern("new"),0);
  Data_Get_Struct(rfiller_instance,struct filler_t,fillerc);

  fillerc->filler=filler;//Init the filler by hand.... TODO: cleaner
  fillerc->buffer=buf;
  args[1]=rfiller_instance;
  args[2]=INT2NUM(offset);
  args[3]=wrap_file_info(ffi);

  res=rb_protect((VALUE (*)())unsafe_readdir,(VALUE)args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------READLINK

static VALUE unsafe_readlink(VALUE *args)
{
  VALUE path = args[0];
  VALUE size = args[1];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("readlink"),3,wrap_context(ctx),path,size);
}

static int rf_readlink(const char *path, char *buf, size_t size)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2NUM(size);
  char *rbuf;
  res=rb_protect((VALUE (*)())unsafe_readlink,(VALUE)args,&error);  
  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    rbuf=STR2CSTR(res);
    strncpy(buf,rbuf,size);
    return 0;
  }
}

//----------------------GETDIR

static VALUE unsafe_getdir(VALUE *args)
{
  VALUE path   = args[0];
  VALUE filler = args[1];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(
    fuse_object,rb_intern("getdir"),3,
    wrap_context(ctx),path,filler
  );
}

//call getdir with an Filler object
static int rf_getdir(const char *path, fuse_dirh_t dh, fuse_dirfil_t df)
{
  VALUE fuse_module;
  VALUE rfiller_class;
  VALUE rfiller_instance;
  VALUE args[2];
  VALUE res;
  struct filler_t *fillerc;
  int error = 0;

  //create a filler object
  args[0]=rb_str_new2(path);

  fuse_module = rb_const_get(rb_cObject, rb_intern("RFuse"));

  rfiller_class    = rb_const_get(fuse_module,rb_intern("Filler"));
  rfiller_instance = rb_funcall(rfiller_class,rb_intern("new"),0);

  Data_Get_Struct(rfiller_instance,struct filler_t,fillerc);

  fillerc->df = df;
  fillerc->dh = dh;

  args[1]=rfiller_instance;

  res = rb_protect((VALUE (*)())unsafe_getdir, (VALUE)args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------MKNOD

static VALUE unsafe_mknod(VALUE *args)
{
  VALUE path = args[0];
  VALUE mode = args[1];
  VALUE dev  = args[2];
  struct fuse_context *ctx=fuse_get_context();
  return rb_funcall(fuse_object,rb_intern("mknod"),4,wrap_context(ctx),path,mode,dev);
}

static int rf_mknod(const char *path, mode_t mode,dev_t dev)
{
  VALUE args[3];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2FIX(mode);
  args[2]=INT2FIX(dev);
  res=rb_protect((VALUE (*)())unsafe_mknod,(VALUE) args,&error);
  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------GETATTR

static VALUE unsafe_getattr(VALUE *args)
{
  VALUE path = args[0];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("getattr"),2,wrap_context(ctx),path);
}

//calls getattr with path and expects something like FuseStat back
static int rf_getattr(const char *path, struct stat *stbuf)
{
  VALUE args[1];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  res=rb_protect((VALUE (*)())unsafe_getattr,(VALUE) args,&error);

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

//----------------------MKDIR

static VALUE unsafe_mkdir(VALUE *args)
{
  VALUE path = args[0];
  VALUE mode = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("mkdir"),3,wrap_context(ctx),path,mode);
}

//calls getattr with path and expects something like FuseStat back
static int rf_mkdir(const char *path, mode_t mode)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2FIX(mode);
  res=rb_protect((VALUE (*)())unsafe_mkdir,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------OPEN

static VALUE unsafe_open(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  =  args[1];
  struct fuse_context *ctx=fuse_get_context();
  return rb_funcall(fuse_object,rb_intern("open"),3,wrap_context(ctx),path,ffi);
}

//calls getattr with path and expects something like FuseStat back
static int rf_open(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  //GG: is args[1] kept on the stack and thus referenced from the GC's perspective?
  args[1]=wrap_file_info(ffi);
  res=rb_protect((VALUE (*)())unsafe_open,(VALUE) args,&error);
  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    if (TYPE(ffi->fh) != T_NONE) {
       //Make sure the GC doesn't collect our FileHandle
       rb_gc_register_address((VALUE*) &ffi->fh);
    }
    return 0;
  }
}

//----------------------RELEASE

static VALUE unsafe_release(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("release"),3,wrap_context(ctx),path,ffi);
}

static int rf_release(const char *path, struct fuse_file_info *ffi)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=wrap_file_info(ffi);
  res=rb_protect((VALUE (*)())unsafe_release,(VALUE) args,&error);
  if (TYPE(ffi->fh) != T_NONE) {
     rb_gc_unregister_address((VALUE*) &ffi->fh);
  }
  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------FSYNC
static VALUE unsafe_fsync(VALUE *args) {
  VALUE path     = args[0];
  VALUE datasync = args[1];
  VALUE ffi      = args[2];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("fsync"), 4, wrap_context(ctx),
    path, datasync, ffi);
}

static int rf_fsync(const char *path, int datasync, struct fuse_file_info *ffi)
{
  VALUE args[3];
  VALUE res;
  int error = 0;

  args[0] = rb_str_new2(path);
  args[1] = INT2NUM(datasync);
  args[2] = wrap_file_info(ffi);

  res = rb_protect((VALUE (*)())unsafe_fsync,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------FLUSH

static VALUE unsafe_flush(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("flush"),3,wrap_context(ctx),path,ffi);
}

static int rf_flush(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=wrap_file_info(ffi);
  res=rb_protect((VALUE (*)())unsafe_flush,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------TRUNCATE

static VALUE unsafe_truncate(VALUE *args)
{
  VALUE path   = args[0];
  VALUE offset = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("truncate"),3,wrap_context(ctx),path,offset);
}

static int rf_truncate(const char *path,off_t offset)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2FIX(offset);
  res=rb_protect((VALUE (*)())unsafe_truncate,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------UTIME

static VALUE unsafe_utime(VALUE *args)
{
  VALUE path    = args[0];
  VALUE actime  = args[1];
  VALUE modtime = args[2];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("utime"),4,wrap_context(ctx),path,actime,modtime);
}

static int rf_utime(const char *path,struct utimbuf *utim)
{
  VALUE args[3];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2NUM(utim->actime);
  args[2]=INT2NUM(utim->modtime);
  res=rb_protect((VALUE (*)())unsafe_utime,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------CHOWN

static VALUE unsafe_chown(VALUE *args)
{
  VALUE path = args[0];
  VALUE uid  = args[1];
  VALUE gid  = args[2];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("chown"),4,wrap_context(ctx),path,uid,gid);
}

static int rf_chown(const char *path,uid_t uid,gid_t gid)
{
  VALUE args[3];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2FIX(uid);
  args[2]=INT2FIX(gid);
  res=rb_protect((VALUE (*)())unsafe_chown,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------CHMOD

static VALUE unsafe_chmod(VALUE *args)
{
  VALUE path = args[0];
  VALUE mode = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("chmod"),3,wrap_context(ctx),path,mode);
}

static int rf_chmod(const char *path,mode_t mode)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2FIX(mode);
  res=rb_protect((VALUE (*)())unsafe_chmod,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------UNLINK

static VALUE unsafe_unlink(VALUE *args)
{
  VALUE path = args[0];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("unlink"),2,wrap_context(ctx),path);
}

static int rf_unlink(const char *path)
{
  VALUE args[1];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  res=rb_protect((VALUE (*)())unsafe_unlink,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------RMDIR

static VALUE unsafe_rmdir(VALUE *args)
{
  VALUE path = args[0];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("rmdir"),2,wrap_context(ctx),path);
}

static int rf_rmdir(const char *path)
{
  VALUE args[1];
  VALUE res;
  int error = 0;
  args[0] = rb_str_new2(path);
  res = rb_protect((VALUE (*)())unsafe_rmdir, (VALUE) args ,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------SYMLINK

static VALUE unsafe_symlink(VALUE *args){
  VALUE path = args[0];
  VALUE as   = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("symlink"),3,wrap_context(ctx),path,as);
}

static int rf_symlink(const char *path,const char *as)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=rb_str_new2(as);
  res=rb_protect((VALUE (*)())unsafe_symlink,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------RENAME

static VALUE unsafe_rename(VALUE *args)
{
  VALUE path = args[0];
  VALUE as   = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("rename"),3,wrap_context(ctx),path,as);
}

static int rf_rename(const char *path,const char *as)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=rb_str_new2(as);
  res=rb_protect((VALUE (*)())unsafe_rename,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------LINK

static VALUE unsafe_link(VALUE *args)
{
  VALUE path = args[0];
  VALUE as   = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("link"),3,wrap_context(ctx),path,as);
}

static int rf_link(const char *path,const char * as)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=rb_str_new2(as);
  res=rb_protect((VALUE (*)())unsafe_link,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------READ

static VALUE unsafe_read(VALUE *args)
{
  VALUE path   = args[0];
  VALUE size   = args[1];
  VALUE offset = args[2];
  VALUE ffi    = args[3];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("read"),5,
        wrap_context(ctx),path,size,offset,ffi);
}



char*
rb_str2cstr(str, len)
    VALUE str;
    long *len;
{
    StringValue(str);
    if (len) *len = RSTRING_LEN(str);
    else if (RTEST(ruby_verbose) &&  RSTRING_LEN(str) != ((long)strlen(RSTRING_PTR(str))) )  {
	rb_warn("string contains \\0 character");
    }
    return RSTRING_PTR(str);
}


static int rf_read(const char *path,char * buf, size_t size,off_t offset,struct fuse_file_info *ffi)
{
  VALUE args[4];
  VALUE res;
  int error = 0;
  long length=0;
  char* rbuf;

  args[0]=rb_str_new2(path);
  args[1]=INT2NUM(size);
  args[2]=INT2NUM(offset);
  args[3]=wrap_file_info(ffi);

  res=rb_protect((VALUE (*)())unsafe_read,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    length = NUM2LONG(rb_funcall(res, rb_intern("length"), 0));
    rbuf = rb_str2cstr(res, &length);
    if (length<=(long)size)
    {
      memcpy(buf,rbuf,length);
      return length;
    }
    else
    {
      //This cannot happen => IO error.
      return -(return_error(ENOENT));
    }
  }
}

//----------------------WRITE

static VALUE unsafe_write(VALUE *args)
{
  VALUE path   = args[0];
  VALUE buffer = args[1];
  VALUE offset = args[2];
  VALUE ffi    = args[3];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("write"),5,
        wrap_context(ctx),path,buffer,offset,ffi);
}

static int rf_write(const char *path,const char *buf,size_t size,
  off_t offset,struct fuse_file_info *ffi)
{
  VALUE args[4];
  VALUE res;
  int error = 0;

  args[0]=rb_str_new2(path);
  args[1]=rb_str_new(buf, size);
  args[2]=INT2NUM(offset);
  args[3]=wrap_file_info(ffi);

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

//----------------------STATFS
static VALUE unsafe_statfs(VALUE *args)
{
  VALUE path = args[0];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("statfs"),2,
        wrap_context(ctx),path);
}

static int rf_statfs(const char * path, struct statvfs * vfsinfo)
{
  VALUE args[1];
  VALUE res;
  int error = 0;

  args[0] = rb_str_new2(path);

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

//----------------------SETXATTR

static VALUE unsafe_setxattr(VALUE *args)
{

  VALUE path  = args[0];
  VALUE name  = args[1];
  VALUE value = args[2];
  VALUE size  = args[3];
  VALUE flags = args[4];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("setxattr"),6,
        wrap_context(ctx),path,name,value,size,flags);
}

static int rf_setxattr(const char *path,const char *name,
           const char *value, size_t size, int flags)
{
  VALUE args[5];
  VALUE res;
  int error = 0;

  args[0]=rb_str_new2(path);
  args[1]=rb_str_new2(name);
  args[2]=rb_str_new(value,size);
  args[3]=INT2NUM(size);
  args[4]=INT2NUM(flags);

  res=rb_protect((VALUE (*)())unsafe_setxattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------GETXATTR

static VALUE unsafe_getxattr(VALUE *args)
{
  VALUE path = args[0];
  VALUE name = args[1];
  VALUE size = args[2];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("getxattr"),4,
        wrap_context(ctx),path,name,size);
}

static int rf_getxattr(const char *path,const char *name,char *buf,
           size_t size)
{
  VALUE args[3];
  VALUE res;
  char *rbuf;
  long length = 0;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=rb_str_new2(name);
  args[2]=INT2NUM(size);
  res=rb_protect((VALUE (*)())unsafe_getxattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    rbuf=rb_str2cstr(res,&length); //TODO protect this, too
    if (buf != NULL)
    {
      memcpy(buf,rbuf,length); //First call is just to get the length
      //TODO optimize
    }
    return length;
  }
}

//----------------------LISTXATTR

static VALUE unsafe_listxattr(VALUE *args)
{
  VALUE path = args[0];
  VALUE size = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("listxattr"),3,
        wrap_context(ctx),path,size);
}

static int rf_listxattr(const char *path,char *buf,
           size_t size)
{
  VALUE args[2];
  VALUE res;
  char *rbuf;
  size_t length =0;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2NUM(size);
  res=rb_protect((VALUE (*)())unsafe_listxattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    rbuf=rb_str2cstr(res,(long *)&length); //TODO protect this, too
    if (buf != NULL)
    {
      if (length<=size)
      {
        memcpy(buf,rbuf,length); //check for size
      } else {
        return -ERANGE;
      }
      printf("destination: %s,%zd\n",buf,size);
      printf("source:      %s,%zd\n",rbuf,length);
      return length;
      //TODO optimize,check lenght
    }
    else
    {
      printf ("not copied: %s, %zd\n",buf,length);
      return length;
    }
  }
}

//----------------------REMOVEXATTR

static VALUE unsafe_removexattr(VALUE *args)
{
  VALUE path = args[0];
  VALUE name = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("removexattr"),3,
        wrap_context(ctx),path,name);
}

static int rf_removexattr(const char *path,const char *name)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=rb_str_new2(name);
  res=rb_protect((VALUE (*)())unsafe_removexattr,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------OPENDIR

static VALUE unsafe_opendir(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("opendir"),3,wrap_context(ctx),path,ffi);
}

static int rf_opendir(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=wrap_file_info(ffi);
  res=rb_protect((VALUE (*)())unsafe_opendir,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------RELEASEDIR

static VALUE unsafe_releasedir(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  = args[1];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("releasedir"),3,wrap_context(ctx),path,ffi);
}

static int rf_releasedir(const char *path,struct fuse_file_info *ffi)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=wrap_file_info(ffi);
  res=rb_protect((VALUE (*)())unsafe_releasedir,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------FSYNCDIR

static VALUE unsafe_fsyncdir(VALUE *args)
{
  VALUE path = args[0];
  VALUE meta = args[1];
  VALUE ffi  = args[2];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("fsyncdir"),4,wrap_context(ctx),path,
        meta,ffi);
}

static int rf_fsyncdir(const char *path,int meta,struct fuse_file_info *ffi)
{
  VALUE args[3];
  VALUE res;
  int error = 0;
  args[0]=rb_str_new2(path);
  args[1]=INT2NUM(meta);
  args[2]=wrap_file_info(ffi);
  res=rb_protect((VALUE (*)())unsafe_fsyncdir,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------INIT
static VALUE unsafe_init(VALUE* args)
{
  VALUE rfuseconninfo = args[0];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("init"),2,wrap_context(ctx),
    rfuseconninfo);
}

static void *rf_init(struct fuse_conn_info *conn)
{
  VALUE args[1];
  VALUE res;
  int error = 0;

  //Create a struct for the conn_info
  VALUE s  = rb_const_get(rb_cObject,rb_intern("Struct"));
  VALUE fci = rb_funcall(s,rb_intern("new"),7,
    ID2SYM(rb_intern("proto_major")),
    ID2SYM(rb_intern("proto_minor")),
    ID2SYM(rb_intern("async_read")),
    ID2SYM(rb_intern("max_write")),
    ID2SYM(rb_intern("max_readahead")),
    ID2SYM(rb_intern("capable")),
    ID2SYM(rb_intern("want"))
  );

  VALUE fcio = rb_funcall(fci,rb_intern("new"),7,
    UINT2NUM(conn->proto_major),
    UINT2NUM(conn->proto_minor),
    UINT2NUM(conn->async_read),
    UINT2NUM(conn->max_write),
    UINT2NUM(conn->max_readahead),
    UINT2NUM(conn->capable),
    UINT2NUM(conn->want)
  );

  args[0] = fcio;

  res = rb_protect((VALUE (*)())unsafe_init,(VALUE) args,&error);

  if (error)
  {
    return NULL;
  }
  else
  {
    return (void *)res;
  }
}

//----------------------DESTROY

static VALUE unsafe_destroy(VALUE* args)
{
  VALUE user_data = args[0];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("destroy"),2,wrap_context(ctx),
    user_data);
}

static void rf_destroy(void *user_data)
{
  VALUE args[1];
  int error = 0;

  args[0] = (VALUE)user_data;

  rb_protect((VALUE (*)())unsafe_destroy,(VALUE) args,&error);
  // TODO: some kind of logging would be nice here.
}

//----------------------ACCESS

static VALUE unsafe_access(VALUE* args)
{
  VALUE path = args[0];
  VALUE mask = args[1];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("access"),3,wrap_context(ctx),
    path, mask);
}

static int rf_access(const char *path, int mask)
{
  VALUE args[2];
  VALUE res;
  int error = 0;
  args[0] = rb_str_new2(path);
  args[1] = INT2NUM(mask);
  res = rb_protect((VALUE (*)())unsafe_access,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------CREATE

static VALUE unsafe_create(VALUE* args)
{
  VALUE path = args[0];
  VALUE mode = args[1];
  VALUE ffi  = args[2];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("create"),4,wrap_context(ctx),
    path, mode, ffi);
}

static int rf_create(const char *path, mode_t mode,
  struct fuse_file_info *ffi)
{
  VALUE args[3];
  VALUE res;
  int error = 0;

  args[0] = rb_str_new2(path);
  args[1] = INT2NUM(mode);
  args[2] = wrap_file_info(ffi);

  res = rb_protect((VALUE (*)())unsafe_create,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------FTRUNCATE

static VALUE unsafe_ftruncate(VALUE* args)
{
  VALUE path = args[0];
  VALUE size = args[1];
  VALUE ffi  = args[2];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("ftruncate"),4,wrap_context(ctx),
    path, size, ffi);
}

static int rf_ftruncate(const char *path, off_t size,
  struct fuse_file_info *ffi)
{
  VALUE args[3];
  VALUE res;
  int error = 0;

  args[0] = rb_str_new2(path);
  args[1] = INT2NUM(size);
  args[2] = wrap_file_info(ffi);

  res = rb_protect((VALUE (*)())unsafe_ftruncate,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------FGETATTR

static VALUE unsafe_fgetattr(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  = args[1];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("fgetattr"),3,wrap_context(ctx),
    path,ffi);
}

static int rf_fgetattr(const char *path, struct stat *stbuf,
  struct fuse_file_info *ffi)
{
  VALUE args[2];
  VALUE res;
  int error = 0;

  args[0] = rb_str_new2(path);
  args[1] = wrap_file_info(ffi);

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

//----------------------LOCK

static VALUE unsafe_lock(VALUE *args)
{
  VALUE path = args[0];
  VALUE ffi  = args[1];
  VALUE cmd  = args[2];
  VALUE lock = args[3];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall(fuse_object,rb_intern("lock"),5,wrap_context(ctx),
    path,ffi,cmd,lock);
}

static int rf_lock(const char *path, struct fuse_file_info *ffi,
  int cmd, struct flock *lock)
{
  VALUE args[4];
  VALUE res;
  int error = 0;

  //Create a struct for the lock structure
  VALUE s  = rb_const_get(rb_cObject,rb_intern("Struct"));
  VALUE lockc = rb_funcall(s,rb_intern("new"),5,
    ID2SYM(rb_intern("l_type")),
    ID2SYM(rb_intern("l_whence")),
    ID2SYM(rb_intern("l_start")),
    ID2SYM(rb_intern("l_len")),
    ID2SYM(rb_intern("l_pid"))
  );

  VALUE locko = rb_funcall(lockc,rb_intern("new"),5,
    UINT2NUM(lock->l_type),
    UINT2NUM(lock->l_whence),
    UINT2NUM(lock->l_start),
    UINT2NUM(lock->l_len),
    UINT2NUM(lock->l_pid)
  );

  args[0] = rb_str_new2(path);
  args[1] = wrap_file_info(ffi);
  args[2] = INT2NUM(cmd);
  args[3] = locko;

  res = rb_protect((VALUE (*)())unsafe_lock,(VALUE) args,&error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------UTIMENS

static VALUE unsafe_utimens(VALUE *args)
{
  VALUE path    = args[0];
  VALUE actime  = args[1];
  VALUE modtime = args[2];

  struct fuse_context *ctx=fuse_get_context();

  return rb_funcall(
    fuse_object,
    rb_intern("utimens"),
    4,
    wrap_context(ctx),
    path,actime,modtime
  );
}

static int rf_utimens(const char * path, const struct timespec tv[2])
{
  VALUE args[3];
  VALUE res;
  int   error = 0;

  args[0] = rb_str_new2(path);

  // tv_sec * 1000000 + tv_nsec
  args[1] = rb_funcall(
    rb_funcall(
      INT2NUM(tv[0].tv_sec), rb_intern("*"), 1, INT2NUM(1000000)
    ),
    rb_intern("+"), 1, INT2NUM(tv[0].tv_nsec)
  );

  args[2] = rb_funcall(
    rb_funcall(
      INT2NUM(tv[1].tv_sec), rb_intern("*"), 1, INT2NUM(1000000)
    ),
    rb_intern("+"), 1, INT2NUM(tv[1].tv_nsec)
  );
  
  res = rb_protect((VALUE (*)())unsafe_utimens,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    return 0;
  }
}

//----------------------BMAP

static VALUE unsafe_bmap(VALUE *args)
{
  VALUE path      = args[0];
  VALUE blocksize = args[1];
  VALUE idx       = args[2];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall( fuse_object, rb_intern("bmap"), 4, wrap_context(ctx),
    path, blocksize, idx);
}

static int rf_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
  VALUE args[3];
  VALUE res;
  int   error = 0;

  args[0] = rb_str_new2(path);
  args[1] = INT2NUM(blocksize);
  args[2] = LL2NUM(*idx);

  res = rb_protect((VALUE (*)())unsafe_bmap,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }
  else
  {
    *idx = NUM2LL(args[2]);
    return 0;
  }
}

//----------------------IOCTL

static VALUE unsafe_ioctl(VALUE *args)
{
  VALUE path  = args[0];
  VALUE cmd   = args[1];
  VALUE arg   = args[2];
  VALUE ffi   = args[3];
  VALUE flags = args[4];
  VALUE data  = args[5];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall( fuse_object, rb_intern("ioctl"), 7, wrap_context(ctx),
    path, cmd, arg, ffi, flags, data);
}

static int rf_ioctl(const char *path, int cmd, void *arg,
  struct fuse_file_info *ffi, unsigned int flags, void *data)
{
  VALUE args[6];
  VALUE res;
  int   error = 0;

  args[0] = rb_str_new2(path);
  args[1] = INT2NUM(cmd);
  args[2] = wrap_buffer(arg);
  args[3] = wrap_file_info(ffi);
  args[4] = INT2NUM(flags);
  args[5] = wrap_buffer(data);

  res = rb_protect((VALUE (*)())unsafe_ioctl,(VALUE) args, &error);

  if (error)
  {
    return -(return_error(ENOENT));
  }

  return 0;
}

//----------------------POLL

static VALUE unsafe_poll(VALUE *args)
{
  VALUE path     = args[0];
  VALUE ffi      = args[1];
  VALUE ph       = args[2];
  VALUE reventsp = args[3];

  struct fuse_context *ctx = fuse_get_context();

  return rb_funcall( fuse_object, rb_intern("poll"), 5, wrap_context(ctx),
    path, ffi, ph, reventsp);
}

static int rf_poll(const char *path, struct fuse_file_info *ffi,
  struct fuse_pollhandle *ph, unsigned *reventsp)
{
  VALUE args[4];
  VALUE res;
  int   error = 0;

  args[0] = rb_str_new2(path);
  args[1] = wrap_file_info(ffi);
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

//----------------------LOOP

static VALUE rf_loop(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  fuse_loop(inf->fuse);
  return Qnil;
}

//----------------------LOOP_MT

static VALUE rf_loop_mt(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  fuse_loop_mt(inf->fuse);
  return Qnil;
}

//----------------------EXIT

VALUE rf_exit(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  fuse_exit(inf->fuse);
  return Qnil;
}

//----------------------UNMOUNT

VALUE rf_unmount(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  fuse_unmount(inf->mountname, inf->fc);
  return Qnil;
}

//----------------------MOUNTNAME

VALUE rf_mountname(VALUE self)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  return rb_str_new2(inf->mountname);
}

//----------------------INVALIDATE

VALUE rf_invalidate(VALUE self,VALUE path)
{
  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);
  return fuse_invalidate(inf->fuse,STR2CSTR(path)); //TODO: check if str?
}

//----------------------FD
// Return /dev/fuse file descriptor for use with IO.select
VALUE rf_fd(VALUE self)
{
 struct intern_fuse *inf;
 Data_Get_Struct(self,struct intern_fuse,inf);
 return INT2NUM(intern_fuse_fd(inf));
}

//----------------------PROCESS
// Process one fuse command from the kernel
// returns < 0 if we're not mounted.. won't be this simple in a mt scenario
VALUE rf_process(VALUE self)
{
 struct intern_fuse *inf;
 Data_Get_Struct(self,struct intern_fuse,inf);
 return INT2NUM(intern_fuse_process(inf));
}

#define RESPOND_TO(obj,methodname) \
  rb_funcall( \
    obj,rb_intern("respond_to?"), \
    1, rb_str_new2(methodname) \
  ) == Qtrue

//-------------RUBY

static VALUE rf_initialize(
  VALUE self,
  VALUE mountpoint,
  VALUE kernelopts,
  VALUE libopts)
{
  Check_Type(mountpoint, T_STRING);
  Check_Type(kernelopts, T_ARRAY);
  Check_Type(libopts, T_ARRAY);

  struct intern_fuse *inf;
  Data_Get_Struct(self,struct intern_fuse,inf);

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
  if (RESPOND_TO(self,"open"))
    inf->fuse_op.open        = rf_open;
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
  if (RESPOND_TO(self,"opendir"))
    inf->fuse_op.opendir     = rf_opendir;
  if (RESPOND_TO(self,"readdir"))
    inf->fuse_op.readdir     = rf_readdir;
  if (RESPOND_TO(self,"releasedir"))
    inf->fuse_op.releasedir  = rf_releasedir;
  if (RESPOND_TO(self,"fsyncdir"))
    inf->fuse_op.fsyncdir    = rf_fsyncdir;
  if (RESPOND_TO(self,"init"))
    inf->fuse_op.init        = rf_init;
  if (RESPOND_TO(self,"destroy"))
    inf->fuse_op.destroy     = rf_destroy;
  if (RESPOND_TO(self,"access"))
    inf->fuse_op.access      = rf_access;
  if (RESPOND_TO(self,"create"))
    inf->fuse_op.create      = rf_create;
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
  if (RESPOND_TO(self,"ioctl"))
    inf->fuse_op.ioctl       = rf_ioctl;
  if (RESPOND_TO(self,"poll"))
    inf->fuse_op.poll        = rf_poll;


  struct fuse_args
    *kargs = rarray2fuseargs(kernelopts),
    *largs = rarray2fuseargs(libopts);

  intern_fuse_init(inf, STR2CSTR(mountpoint), kargs, largs);

  //TODO this won't work with multithreading!!!
  fuse_object=self;

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

VALUE rfuse_init(VALUE module)
{
  VALUE cFuse=rb_define_class_under(module,"Fuse",rb_cObject);

  rb_define_alloc_func(cFuse,rf_new);

  rb_define_method(cFuse,"initialize",rf_initialize,3);
  rb_define_method(cFuse,"loop",rf_loop,0);
  rb_define_method(cFuse,"loop_mt",rf_loop_mt,0); //TODO: multithreading
  rb_define_method(cFuse,"exit",rf_exit,0);
  rb_define_method(cFuse,"invalidate",rf_invalidate,1);
  rb_define_method(cFuse,"unmount",rf_unmount,0);
  rb_define_method(cFuse,"mountname",rf_mountname,0);
  rb_define_method(cFuse,"fd",rf_fd,0);
  rb_define_method(cFuse,"process",rf_process,0);

  return cFuse;
}
