#include "file_info.h"
#include <fuse.h>


static void file_info_mark(struct fuse_file_info *ffi) {
  if (TYPE(ffi->fh) != T_NONE) {
     rb_gc_mark((VALUE) ffi->fh);
  } 
}

//creates a FileInfo object from an already allocated ffi
VALUE wrap_file_info(struct fuse_file_info *ffi) {
  VALUE rRFuse;
  VALUE rFileInfo;
  rRFuse=rb_const_get(rb_cObject,rb_intern("RFuse"));
  rFileInfo=rb_const_get(rRFuse,rb_intern("FileInfo"));
  //TODO GG: we need a mark function here to ensure the ffi-fh value is not GC'd
  //between open and release
  return Data_Wrap_Struct(rFileInfo,file_info_mark,0,ffi); //shouldn't be freed!

};


VALUE file_info_initialize(VALUE self){
  return self;
}

//TODO GG: This probably needs a free function and be converted to alloc/initialize
//but this probably never gets called anyway
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

//fh is possibly a pointer to a ruby object and can be set
VALUE file_info_fh(VALUE self) {
   struct fuse_file_info *f;
   Data_Get_Struct(self,struct fuse_file_info,f);
  if (TYPE(f->fh) != T_NONE) {
    return (VALUE) f->fh;
  } else {
    return Qnil;
  }
}

VALUE file_info_fh_assign(VALUE self,VALUE value) {
   struct fuse_file_info *f;
   Data_Get_Struct(self,struct fuse_file_info,f);
   f->fh = value;
   return value;
}

VALUE file_info_init(VALUE module) {
  VALUE cFileInfo=rb_define_class_under(module,"FileInfo",rb_cObject);
  rb_define_alloc_func(cFileInfo,file_info_new);
  rb_define_method(cFileInfo,"initialize",file_info_initialize,0);
  rb_define_method(cFileInfo,"flags",file_info_flags,0);
  rb_define_method(cFileInfo,"writepage",file_info_writepage,0);
  rb_define_method(cFileInfo,"fh",file_info_fh,0);
  rb_define_method(cFileInfo,"fh=",file_info_fh_assign,1);
  return cFileInfo;
}
