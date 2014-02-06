require 'mkmf'

$CFLAGS << ' -Wall'
#$CFLAGS << ' -Werror'
$CFLAGS << ' -D_FILE_OFFSET_BITS=64'
$CFLAGS << ' -DFUSE_USE_VERSION=26'


have_func("rb_errinfo")

have_func("rb_set_errinfo")

if have_library('fuse')
  create_makefile('rfuse/rfuse')
else
  puts "No FUSE install available"
end
