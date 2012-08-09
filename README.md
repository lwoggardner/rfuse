RFuse
===============

http://rubygems.org/gems/rfuse

FUSE bindings for Ruby 

FUSE (Filesystem in USErspace) is a simple interface for userspace programs to export a virtual filesystem to the linux kernel. FUSE aims to provide a secure method for non privileged users to create and mount their own filesystem implementations.

This library provides the low-level fuse operations as callbacks into a ruby object.

For a more ruby-ish API for creating filesystems see {http://rubygems.org/gems/rfusefs RFuseFS} which is built on RFuse and takes care of some of the heavy lifting.

Dependencies
--------------

 * Ruby 1.8 or 1.9
 * Fuse 2.8

Installation
---------------

    $ gem install rfuse

Creating a filesystem
---------------------------

Extend and implement the abstract methods of {RFuse::Fuse} (See also {RFuse::FuseDelegator})

For a sample filesystem see sample/test-ruby.rb

To run the example:

     $ mkdir /tmp/fuse
     $ ruby sample/test-ruby.rb /tmp/fuse

..and there should be an empty in-memory filesystem mounted at /tmp/fuse

     $ fusermount -u /tmp/fuse

...should stop the example filesystem program.

HISTORY
======
This project is forked from rfuse-ng which was forked from the original rfuse.

RFuse-NG: Tamás László Fábián <giganetom@gmail.com> et al on Github

Original Rfuse (@rubyforge): Peter Schrammel AT gmx.de

See also {file:CHANGES.md}

CONTRIBUTING
============

1. Fork it on {http://github.com/lwoggardner/rfuse on GitHub}
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Write some specs and make them pass
4. Commit your changes (`git commit -am 'Added some feature'`)
5. Push to the branch (`git push origin my-new-feature`)
6. Create new Pull Request
