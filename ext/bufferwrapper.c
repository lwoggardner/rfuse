#include <stdlib.h>
#include <ruby.h>
#include <fuse.h>

#include "bufferwrapper.h"

VALUE bufferwrapper_new(VALUE class)
{
  rb_raise(rb_eNotImpError, "new() not implemented (it has no use), and should not be called");
  return Qnil;
}

VALUE bufferwrapper_initialize(VALUE self)
{
  return self;
}

VALUE bufferwrapper_getdata(VALUE self, VALUE size)
{
  void *buf;

  Data_Get_Struct(self, void, buf);

  return rb_str_new(buf, FIX2INT(size));
}

VALUE wrap_buffer(void *buf)
{
  VALUE rRFuse;
  VALUE rBufferWrapper;

  rRFuse         = rb_const_get(rb_cObject,rb_intern("RFuse"));
  rBufferWrapper = rb_const_get(rRFuse,rb_intern("BufferWrapper"));

  return Data_Wrap_Struct(rBufferWrapper, NULL, NULL, buf);
}

VALUE bufferwrapper_init(VALUE module)
{
  VALUE cBufferWrapper = rb_define_class_under(module,"BufferWrapper",rb_cObject);

  rb_define_alloc_func(cBufferWrapper,bufferwrapper_new);

  rb_define_method(cBufferWrapper,"initialize",bufferwrapper_initialize,0);
  rb_define_method(cBufferWrapper,"getData",bufferwrapper_getdata,1);

  return cBufferWrapper;
}
