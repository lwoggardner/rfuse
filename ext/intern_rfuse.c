#include "intern_rfuse.h"
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct intern_fuse *intern_fuse_new() {
  struct intern_fuse *inf;
  inf = (struct intern_fuse *) malloc(sizeof(struct intern_fuse));
  memset(&inf->fuse_op, 0, sizeof(struct fuse_operations));
  return inf;
}

int intern_fuse_destroy(struct intern_fuse *inf){
  //you have to take care, that fuse is unmounted yourself!
  fuse_destroy(inf->fuse);
  free(inf);
  return 0;
}

int intern_fuse_init(
  struct intern_fuse *inf,
  const char *mountpoint, 
  struct fuse_args *kernelopts,
  struct fuse_args *libopts)
{
  struct fuse_chan* fc;

  fc = fuse_mount(mountpoint, kernelopts);

  if (fc == NULL) {
    return -1;
  }

  inf->fuse=fuse_new(fc, libopts, &(inf->fuse_op), sizeof(struct fuse_operations), NULL);
  inf->fc = fc;

  if (strlen(inf->mountname) > MOUNTNAME_MAX) {
    return -1;
  }

  strncpy(inf->mountname, mountpoint, MOUNTNAME_MAX);
  return 0;
}

// Return the /dev/fuse file descriptor for use with IO.select
int intern_fuse_fd(struct intern_fuse *inf)
{
   if (inf->fc == NULL) {
     return -1;
   }

  struct fuse_chan *fc = inf->fc;
  return fuse_chan_fd(fc);
}

//Process one fuse command (ie after IO.select)
int intern_fuse_process(struct intern_fuse *inf)
{
  if (inf->fuse == NULL) {
    return -1;
  }


  if (fuse_exited(inf->fuse)) {
    return -1;
  }

  struct fuse_cmd *cmd;
  cmd = fuse_read_cmd(inf->fuse);

  if (cmd != NULL) {
    fuse_process_cmd(inf->fuse, cmd);
  }

  return 0;
}
