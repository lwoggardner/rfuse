#include "context.h"
#include <fuse.h>

VALUE wrap_context (struct fuse_context *fctx) {
  VALUE rRFuse;
  VALUE rContext;
  rRFuse=rb_const_get(rb_cObject,rb_intern("RFuse"));
  rContext=rb_const_get(rRFuse,rb_intern("Context"));
  return Data_Wrap_Struct(rContext,0,0,fctx); //shouldn't be freed!
}

/*
   @private
*/
VALUE context_initialize(VALUE self){
  return self;
}

//This should never be called!
VALUE context_new(VALUE class){
  VALUE self;
  struct fuse_context *ctx;
  self = Data_Make_Struct(class, struct fuse_context, 0,free,ctx);
  return self;
}

/*
  @return [Integer] User id of the caller
*/
VALUE context_uid(VALUE self){
  struct fuse_context *ctx;
  Data_Get_Struct(self,struct fuse_context,ctx);
  return INT2FIX(ctx->uid);
}

/*
   @return [Integer] Group id of the caller
*/
VALUE context_gid(VALUE self){
  struct fuse_context *ctx;
  Data_Get_Struct(self,struct fuse_context,ctx);
  return INT2FIX(ctx->gid);
}

/*
   @return [Integer] Process id of the calling process
*/
VALUE context_pid(VALUE self){
  struct fuse_context *ctx;
  Data_Get_Struct(self,struct fuse_context,ctx);
  return INT2FIX(ctx->pid);
}

/*
 * @return [Fuse] the fuse object
 */
VALUE context_fuse(VALUE self) {
  struct fuse_context *ctx;
  Data_Get_Struct(self,struct fuse_context,ctx);
  return (VALUE) ctx->private_data;
}
/*
   Document-class:  RFuse::Context
   Context object passed to every fuse operation
*/
void context_init(VALUE module) {
#if 0
    module = rb_define_module("RFuse");
    

#endif

  VALUE cContext=rb_define_class_under(module,"Context",rb_cObject);
  rb_define_alloc_func(cContext,context_new);
  rb_define_method(cContext,"initialize",context_initialize,0);
  rb_define_method(cContext,"uid",context_uid,0);
  rb_define_method(cContext,"gid",context_gid,0);
  rb_define_method(cContext,"pid",context_pid,0);
  rb_define_method(cContext,"fuse",context_fuse,0);
}
