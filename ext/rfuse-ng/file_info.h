#include <fuse.h>
#include <ruby.h>

VALUE wrap_file_info(struct fuse_file_info *ffi);

VALUE file_info_initialize(VALUE self);
VALUE file_info_new(VALUE class);
VALUE file_info_flags(VALUE self);
VALUE file_info_writepage(VALUE self);

VALUE file_info_init(VALUE module);
