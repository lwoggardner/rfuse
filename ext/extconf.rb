require 'mkmf'

$CFLAGS << ' -Wall'
$CFLAGS << ' -Werror'
$CFLAGS << ' -D_FILE_OFFSET_BITS=64'
$CFLAGS << ' -DFUSE_USE_VERSION=26'

if have_library('fuse')
  create_makefile('rfuse_ng')
else
  puts "No FUSE install available"
end
