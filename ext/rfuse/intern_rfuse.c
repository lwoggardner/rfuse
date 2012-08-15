#include "intern_rfuse.h"
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct intern_fuse *intern_fuse_new() {
  struct intern_fuse *inf;
  inf = (struct intern_fuse *) malloc(sizeof(struct intern_fuse));
  memset(inf, 0, sizeof(*inf));
  memset(&inf->fuse_op, 0, sizeof(struct fuse_operations));
  return inf;
}


int intern_fuse_destroy(struct intern_fuse *inf){
  //you have to take care, that fuse is unmounted yourself!
  if(inf->fuse)
    fuse_destroy(inf->fuse);
  if(inf->mountpoint)
     free(inf->mountpoint);
  free(inf);
  return 0;
}

int intern_fuse_init(struct intern_fuse *inf, struct fuse_args *args, void* user_data)
{
  struct fuse_chan* fc;
  char* mountpoint;
  mountpoint = inf->mountpoint;
  fc = fuse_mount(mountpoint,args);

  if (fc == NULL) {
    return -1;
  }

  inf->fuse=fuse_new(fc, args, &(inf->fuse_op), sizeof(struct fuse_operations), user_data);

  if (inf->fuse == NULL) {
      fuse_unmount(inf->mountpoint, fc);
      return -1;
  }

  inf->fc = fc;

  return 0;
}

// Return the /dev/fuse file descriptor for use with IO.select
int intern_fuse_fd(struct intern_fuse *inf)
{
   if (inf->fc == NULL) {
     return -1;
   }

  return fuse_chan_fd(inf->fc);
}

//Process one fuse command (ie after IO.select)
int intern_fuse_process(struct intern_fuse *inf)
{
  struct fuse_cmd *cmd;

  if (inf->fuse == NULL) {
    return -1;
  }

  if (fuse_exited(inf->fuse)) {
    return -1;
  }

  cmd = fuse_read_cmd(inf->fuse);

  if (cmd != NULL) {
    fuse_process_cmd(inf->fuse, cmd);
  }

  return 0;
}
