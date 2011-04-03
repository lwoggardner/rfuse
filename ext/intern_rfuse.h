#include <fuse.h>

#define MOUNTNAME_MAX 1024

struct intern_fuse {
  struct fuse_chan *fc;
  struct fuse *fuse;
  struct fuse_operations fuse_op;
  struct fuse_context *fuse_ctx;
  char   mountname[MOUNTNAME_MAX];
  int state; //created,mounted,running
};

struct intern_fuse *intern_fuse_new();

int intern_fuse_init(
  struct intern_fuse *inf,
  const char *mountpoint, 
  struct fuse_args *args,
  struct fuse_args *libopts
);

int intern_fuse_fd(struct intern_fuse *inf);
int intern_fuse_process(struct intern_fuse *inf);
int intern_fuse_destroy(struct intern_fuse *inf);
