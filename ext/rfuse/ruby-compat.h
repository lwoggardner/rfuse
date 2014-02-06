#ifdef HAVE_RUBY_ENCODING_H
#include <ruby/encoding.h>
#define rb_filesystem_encode(STR) (rb_enc_associate(STR,rb_filesystem_encoding()))
#else
#define rb_filesystem_encode(STR)
#endif

#ifndef HAVE_RB_ERRINFO
#define rb_errinfo() ruby_errinfo
#endif

#ifndef HAVE_RB_SET_ERRINFO
#define rb_set_errinfo(v)  /*no-op*/
#endif

/* SIZET macros from ruby 1.9 */
#ifndef SIZET2NUM
#if SIZEOF_SIZE_T > SIZEOF_LONG && defined(HAVE_LONG_LONG)
# define SIZET2NUM(v) ULL2NUM(v)
# define SSIZET2NUM(v) LL2NUM(v)
#elif SIZEOF_SIZE_T == SIZEOF_LONG
# define SIZET2NUM(v) ULONG2NUM(v)
# define SSIZET2NUM(v) LONG2NUM(v)
#else
# define SIZET2NUM(v) UINT2NUM(v)
# define SSIZET2NUM(v) INT2NUM(v)
#endif
#endif

#ifndef NUM2SIZET
#if defined(HAVE_LONG_LONG) && SIZEOF_SIZE_T > SIZEOF_LONG
# define NUM2SIZET(x) ((size_t)NUM2ULL(x))
# define NUM2SSIZET(x) ((ssize_t)NUM2LL(x))
#else
# define NUM2SIZET(x) NUM2ULONG(x)
# define NUM2SSIZET(x) NUM2LONG(x)
#endif
#endif

