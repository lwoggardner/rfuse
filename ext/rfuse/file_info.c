#include "file_info.h"
#include <fuse.h>

//creates a FileInfo object from an already allocated ffi
VALUE wrap_file_info(struct fuse_context *ctx, struct fuse_file_info *ffi) {
  VALUE rRFuse;
  VALUE rFileInfo;
  
  VALUE rffi;
 
  VALUE open_files;
  VALUE key;

  rRFuse=rb_const_get(rb_cObject,rb_intern("RFuse"));
  rFileInfo=rb_const_get(rRFuse,rb_intern("FileInfo"));

  rffi = Data_Wrap_Struct(rFileInfo,0,0,ffi);

  //store the wrapped ffi back into the struct so we don't have to keep wrapping it
  ffi->fh = rffi;
 
  //also store it in an open_files hash on the fuse_object
  //so it doesn't get GC'd
  open_files = rb_iv_get((VALUE) ctx->private_data,"@open_files");
  key        = rb_funcall(rffi,rb_intern("object_id"),0);
  rb_hash_aset(open_files,key,rffi);

  return rffi;
};

//returns a previously wrapped ffi
VALUE get_file_info(struct fuse_file_info *ffi) {
    
  if (TYPE(ffi->fh) == T_DATA )
      return (VALUE) ffi->fh;
  else
      return Qnil;
  
};

//Allow the FileInfo object to be GC'd 
VALUE release_file_info(struct fuse_context *ctx, struct fuse_file_info *ffi)
{

  if (TYPE(ffi->fh) == T_DATA) {
      VALUE rffi = ffi->fh;
      VALUE fuse_object = (VALUE) ctx->private_data;
      VALUE open_files = rb_iv_get(fuse_object,"@open_files");
      VALUE key = rb_funcall(rffi,rb_intern("object_id"),0);
      rb_hash_delete(open_files,key);

      return rffi;
  } 

  return Qnil;

}

/*
 * @private
*/
VALUE file_info_initialize(VALUE self){
  return self;
}

//TODO FT: test: this _should_not_ be called, an exception would do the trick :)
VALUE file_info_new(VALUE class){
  rb_raise(rb_eNotImpError, "new() not implemented (it has no use), and should not be called");
  return Qnil;
}

VALUE file_info_writepage(VALUE self) {
  struct fuse_file_info *f;
  Data_Get_Struct(self,struct fuse_file_info,f);
  return INT2FIX(f->writepage);
}

VALUE file_info_flags(VALUE self) {
  struct fuse_file_info *f;
  Data_Get_Struct(self,struct fuse_file_info,f);
  return INT2FIX(f->flags);
}

VALUE file_info_direct(VALUE self) {
   struct fuse_file_info *f;
   Data_Get_Struct(self,struct fuse_file_info,f);
  if (TYPE(f->direct_io) != T_NONE) {
    return (VALUE) f->direct_io;
  } else {
    return Qnil;
  }
}

VALUE file_info_direct_assign(VALUE self,VALUE value) {
   struct fuse_file_info *f;
   Data_Get_Struct(self,struct fuse_file_info,f);
   f->direct_io = value;
   return value;
}

VALUE file_info_nonseekable(VALUE self) {
   struct fuse_file_info *f;
   Data_Get_Struct(self,struct fuse_file_info,f);
  if (TYPE(f->nonseekable) != T_NONE) {
    return (VALUE) f->nonseekable;
  } else {
    return Qnil;
  }
}

VALUE file_info_nonseekable_assign(VALUE self,VALUE value) {
   struct fuse_file_info *f;
   Data_Get_Struct(self,struct fuse_file_info,f);
   f->nonseekable = value;
   return value;
}

/*
   Document-class: RFuse::FileInfo

   Represents an open file (or directory) that is reused
   across multiple fuse operations

*/
void file_info_init(VALUE module) {
#if 0
    //Trick Yardoc
    module = rb_define_module("RFuse");
#endif
  VALUE cFileInfo=rb_define_class_under(module,"FileInfo",rb_cObject);
  rb_define_alloc_func(cFileInfo,file_info_new);
  rb_define_method(cFileInfo,"initialize",file_info_initialize,0);
  rb_define_method(cFileInfo,"flags",file_info_flags,0);
  rb_define_method(cFileInfo,"writepage",file_info_writepage,0);
  rb_define_method(cFileInfo,"direct",file_info_direct,0);
  rb_define_method(cFileInfo,"direct=",file_info_direct_assign,1);
  rb_define_method(cFileInfo,"nonseekable",file_info_nonseekable,0);
  rb_define_method(cFileInfo,"nonseekable=",file_info_nonseekable_assign,1);
  
  /*
     @return [Object] user specified filehandle object. See {Fuse#open}
  */
  rb_define_attr(cFileInfo,"fh",1,1);
}
