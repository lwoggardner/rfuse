
0.6.0
----------------

Become rfuse once again with Ruby 1.9 support and documentation

API breaking changes

 * {RFuse::Fuse#initialize}: combined options arrays (libopts,kernelopts) into a single arg array
 * {RFuse::Fuse#mknod}: split device integer into major and minor numbers
 * {RFuse::Fuse#setxattr}: removed unnecessary size parameter
 * {RFuse::Fuse#getxattr}: removed unnecessary size parameter
 * {RFuse::Fuse#listxattr}: return a String array which will be packed into a list of
                         NULL terminated strings

Removed methods
 
 * RFuse::loop_mt - never worked, (see multithread branch on github)
 * RFuse::destroy - callback was being called during finalizer which was not safe

