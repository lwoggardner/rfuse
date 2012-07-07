#include <fuse.h>
#include <ruby.h>

VALUE wrap_pollhandle(struct fuse_pollhandle *ph);

VALUE pollhandle_new(VALUE class);
VALUE pollhandle_initialize(VALUE self);
VALUE pollhandle_notify_poll(VALUE self);

VALUE pollhandle_init(VALUE module);
