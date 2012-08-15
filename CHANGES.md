1.0.2- 2012/08/09
-----------------

Support Ruby 1.8 (tested with 1.8.7)

Bugfixes

 * {RFuse::Fuse#utimens} fixed to correctly convert time to nanoseconds
 * Exceptions in {RFuse::Fuse#getattr} fixed to output backtrace etc

1.0.0
----------------

Become rfuse once again with Ruby 1.9 support and documentation

Note that 1.0.0 does not signify anything more meaningful than a new release
with API breaking changes since the various 0.x.y series of rfuse and rfuse-ng
(as per rubygems recommended semantic versioning)


### API breaking changes

 * {RFuse::Fuse#initialize}: combined options arrays (libopts,kernelopts) into a single arg array
 * {RFuse::Fuse#mknod}: split device integer into major and minor numbers
 * {RFuse::Fuse#setxattr}: removed unnecessary size parameter
 * {RFuse::Fuse#getxattr}: removed unnecessary size parameter
 * {RFuse::Fuse#listxattr}: return a String array which will be packed into a list of
                            NULL terminated strings

### Removed methods
 
 * RFuse::loop_mt - never worked, (see multithread branch on github)
 * RFuse::destroy - callback was being called during finalizer which was not safe

### Other major changes

  {RFuse::Fuse#loop} is now done in ruby using IO.select on the underlying
     FUSE file descriptor and using {RFuse::Fuse#process} to handle one fuse
     command at a time. This allows the Ruby interpreter to retain control
     so you can use other threads, Signal.trap etc...

  Various {RFuse::Fuse} methods will throw {RFuse::Error} if the filesystem is
  not mounted, which can be tested after initialisation with {RFuse::Fuse#mounted?}

  Implemented {RFuse::FuseDelegator} so your filesystem can be implemented (and tested/debugged)
  without needing to be a subclass of RFuse::Fuse
  
  Implemented {RFuse::Stat} and {RFuse::StatVfs} helper classes

