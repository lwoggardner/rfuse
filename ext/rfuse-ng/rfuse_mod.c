#include "rfuse.h"
#include "filler.h"
#include "file_info.h"
#include "context.h"

void Init_rfuse() {
  VALUE mRFuse=rb_define_module("RFuse");
  file_info_init(mRFuse);
  context_init(mRFuse);
  rfiller_init(mRFuse);
  rfuse_init(mRFuse);
}
