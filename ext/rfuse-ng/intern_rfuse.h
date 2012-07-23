#include <fuse.h>

#define MOUNTNAME_MAX 1024

struct intern_fuse {
  struct fuse_chan *fc;
  struct fuse *fuse;
  struct fuse_operations fuse_op;
  char *mountpoint;
};

struct intern_fuse *intern_fuse_new();

int intern_fuse_init(
  struct intern_fuse *inf,
  struct fuse_args *args,
  void *user_data
);

int intern_fuse_fd(struct intern_fuse *inf);
int intern_fuse_process(struct intern_fuse *inf);
int intern_fuse_destroy(struct intern_fuse *inf);
