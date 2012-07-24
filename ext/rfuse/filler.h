#include <fuse.h>
#include <ruby.h>

struct filler_t {
  fuse_fill_dir_t filler;
  void            *buffer;
  fuse_dirh_t     dh;
  fuse_dirfil_t   df;
};

VALUE rfiller_initialize(VALUE self);
VALUE rfiller_new(VALUE class);
VALUE rfiller_push(VALUE self, VALUE name, VALUE stat, VALUE offset);

void rfiller_init(VALUE module);
