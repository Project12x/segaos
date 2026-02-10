/* Freestanding string.h for m68k-elf-gcc
 * Provides inline implementations of basic string/memory functions.
 * No standard library available — everything is implemented here.
 */
#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* ---- Memory functions ---- */

static inline void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  while (n--) {
    *p++ = (unsigned char)c;
  }
  return s;
}

static inline void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  while (n--) {
    *d++ = *s++;
  }
  return dest;
}

static inline void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  if (d < s) {
    while (n--) {
      *d++ = *s++;
    }
  } else {
    d += n;
    s += n;
    while (n--) {
      *--d = *--s;
    }
  }
  return dest;
}

static inline int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *a = (const unsigned char *)s1;
  const unsigned char *b = (const unsigned char *)s2;
  while (n--) {
    if (*a != *b) {
      return *a - *b;
    }
    a++;
    b++;
  }
  return 0;
}

/* ---- String functions ---- */

static inline size_t strlen(const char *s) {
  const char *p = s;
  while (*p) {
    p++;
  }
  return (size_t)(p - s);
}

static inline char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++) != '\0')
    ;
  return dest;
}

static inline char *strncpy(char *dest, const char *src, size_t n) {
  char *d = dest;
  while (n && (*d++ = *src++) != '\0') {
    n--;
  }
  while (n--) {
    *d++ = '\0';
  }
  return dest;
}

static inline int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static inline int strncmp(const char *s1, const char *s2, size_t n) {
  while (n && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }
  if (n == 0)
    return 0;
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static inline char *strcat(char *dest, const char *src) {
  char *d = dest;
  while (*d)
    d++;
  while ((*d++ = *src++) != '\0')
    ;
  return dest;
}

static inline char *strchr(const char *s, int c) {
  while (*s) {
    if (*s == (char)c)
      return (char *)s;
    s++;
  }
  return (c == '\0') ? (char *)s : (char *)0;
}

#endif /* _STRING_H */
