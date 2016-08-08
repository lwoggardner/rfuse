#ifndef __APPLE__
#include <ruby.h>
#include <fuse.h>

#include "pollhandle.h"

static int pollhandle_destroy(struct fuse_pollhandle *ph)
{
  fuse_pollhandle_destroy(ph);
  return 0;
}

VALUE pollhandle_new(VALUE class)
{
  rb_raise(rb_eNotImpError, "new() not implemented (it has no use), and should not be called");
  return Qnil;
}

VALUE pollhandle_initialize(VALUE self)
{
  return self;
}

VALUE pollhandle_notify_poll(VALUE self)
{
  struct fuse_pollhandle *ph;
  Data_Get_Struct(self, struct fuse_pollhandle, ph);
  return INT2FIX(fuse_notify_poll(ph));
}

VALUE wrap_pollhandle(struct fuse_pollhandle *ph)
{
  VALUE rRFuse;
  VALUE rPollHandle;

  rRFuse      = rb_const_get(rb_cObject,rb_intern("RFuse"));
  rPollHandle = rb_const_get(rRFuse,rb_intern("PollHandle"));

  // We need a mark function here to ensure the ph is not GC'd
  // between poll and notify_poll
  return Data_Wrap_Struct(rPollHandle, NULL, pollhandle_destroy, ph);

}

VALUE pollhandle_init(VALUE module)
{
  VALUE cPollHandle = rb_define_class_under(module,"PollHandle",rb_cObject);

  rb_define_alloc_func(cPollHandle,pollhandle_new);

  rb_define_method(cPollHandle,"initialize",pollhandle_initialize,0);
  rb_define_method(cPollHandle,"notifyPoll",pollhandle_notify_poll,0);

  return cPollHandle;
}
#endif
