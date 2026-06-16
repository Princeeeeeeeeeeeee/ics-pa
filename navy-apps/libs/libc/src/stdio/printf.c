
#include <_ansi.h>
#include <stdio.h>

#ifdef _HAVE_STDC

#include <stdarg.h>

int
_printf_r (struct _reent *ptr, const char *fmt, ...)
{
  int ret;
  va_list ap;

  va_start (ap, fmt);
  ret = _vfprintf_r (ptr, _stdout_r (ptr), fmt, ap);
  va_end (ap);
  return ret;
}

#else

#include <varargs.h>

int
_printf_r (ptr, fmt, va_alist)
     struct _reent *ptr;
     char *fmt;
     va_dcl
{
  int ret;
  va_list ap;

  va_start (ap);
  ret = _vfprintf_r (ptr, _stdout_r (ptr), fmt, ap);
  va_end (ap);
  return ret;
}

#endif


#ifndef _REENT_ONLY

#ifdef _HAVE_STDC

#include <stdarg.h>

int
printf (const char *fmt, ...)
{
  int ret;
  va_list ap;

  va_start (ap, fmt);
  _stdout_r (_REENT)->_data = _REENT;
  ret = vfprintf (_stdout_r (_REENT), fmt, ap);
  va_end (ap);
  return ret;
}

#else

#include <varargs.h>

int
printf (fmt, va_alist)
     char *fmt;
     va_dcl
{
  int ret;
  va_list ap;

  va_start (ap);
  _stdout_r (_REENT)->_data = _REENT;
  ret = vfprintf (_stdout_r (_REENT), fmt, ap);
  va_end (ap);
  return ret;
}

#endif /* ! _HAVE_STDC */

#endif /* ! _REENT_ONLY */
/*
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern int _write(int fd, const void *buf, size_t count);

static int utoa(unsigned long v, unsigned base, char *out) {
  const char *d = "0123456789abcdef";
  char tmp[32];
  int t = 0;
  if (v == 0) { out[0] = '0'; out[1] = 0; return 1; }
  while (v) { tmp[t++] = d[v % base]; v /= base; }
  int n = 0;
  while (t--) out[n++] = tmp[t];
  out[n] = 0;
  return n;
}

int printf(const char *fmt, ...) {
  char buf[512];
  char *p = buf;
  va_list ap;
  va_start(ap, fmt);
  while (*fmt && (p - buf) < (int)(sizeof(buf)-1)) {
    if (*fmt != '%') { *p++ = *fmt++; continue; }
    fmt++;
    if (*fmt == 's') {
      char *s = va_arg(ap, char*);
      while (*s && (p - buf) < (int)(sizeof(buf)-1)) *p++ = *s++;
    } else if (*fmt == 'd' || *fmt == 'u') {
      int v = va_arg(ap, int);
      if (*fmt == 'd' && v < 0) { *p++ = '-'; v = -v; }
      char num[32]; int n = utoa((unsigned)v,10,num);
      for (int i=0;i<n && (p-buf)<(int)(sizeof(buf)-1); i++) *p++ = num[i];
    } else if (*fmt == 'x') {
      unsigned v = va_arg(ap, unsigned);
      char num[32]; int n = utoa(v,16,num);
      for (int i=0;i<n && (p-buf)<(int)(sizeof(buf)-1); i++) *p++ = num[i];
    } else if (*fmt == 'c') {
      int c = va_arg(ap, int); *p++ = (char)c;
    } else if (*fmt == '%') {
      *p++ = '%';
    }
    fmt++;
  }
  *p = 0;
  va_end(ap);
  int len = p - buf;
  if (len > 0) _write(1, buf, len);
  return len;
}*/