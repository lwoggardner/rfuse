#include "filler.h"
#include <fuse.h>
#include "helper.h"

VALUE rfiller_initialize(VALUE self){
  return self;
}

/*
* @private Never called
*/
VALUE rfiller_new(VALUE class){
  VALUE self;
  struct filler_t *f;
  self = Data_Make_Struct(class, struct filler_t, 0,free,f);
  return self;
}

/*
 * Add a value into the filler 
 * @param [String] name a file name
 * @param [Stat] stat Stat info representing the file, may be nil
 * @param [Integer] offset index of next entry, or zero
 * 
 * @return self or nil if the buffer is full
 * 
 */
VALUE rfiller_push(VALUE self, VALUE name, VALUE stat, VALUE offset) {
  struct filler_t *f;
  int result;
  
  Data_Get_Struct(self,struct filler_t,f);


  //Allow nil return instead of a stat
  if (NIL_P(stat)) {
    result = f->filler(f->buffer,StringValueCStr(name),NULL,NUM2OFFT(offset));
  } else {
    struct stat st;
    memset(&st, 0, sizeof(st));
    rstat2stat(stat,&st);
    result = f->filler(f->buffer,StringValueCStr(name),&st,NUM2OFFT(offset));
  }

  return result ? Qnil : self;
}

/*
* Document-class: RFuse::Filler
* Used by {Fuse#readdir} to collect directory entries
*/
void rfiller_init(VALUE module) {

#if 0
  module = rb_define_module("RFuse");
#endif
  VALUE cFiller=rb_define_class_under(module,"Filler",rb_cObject);
  rb_define_alloc_func(cFiller,rfiller_new);
  rb_define_method(cFiller,"initialize",rfiller_initialize,0);
  rb_define_method(cFiller,"push",rfiller_push,3);
}
