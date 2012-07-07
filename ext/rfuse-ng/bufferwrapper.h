#include <fuse.h>
#include <ruby.h>

VALUE wrap_buffer(void *buf);

VALUE bufferwrapper_new(VALUE class);
VALUE bufferwrapper_getdata(VALUE self, VALUE size);
VALUE bufferwrapper_init(VALUE module);

VALUE bufferwrapper_initialize(VALUE self);
