#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "yxml.h"

//#define DEBUG

#define YXMLBUFSIZE 4096

/* 1mb */
#define BUFSIZE 10485760

#define EOS '\0'

#define CBUFMAX 1024

#ifdef DEBUG
#define DPRINT(...) fprintf(stdout, __VA_ARGS__)
#else /* !DEBUG */
#define DPRINT(...)
#endif /* !DEBUG */

#define FATAL(...) { fprintf(stdout, __VA_ARGS__) ; exit(1); }

typedef enum {
  CSTATE_FIELD_NONE,

  /* on all types */
  CSTATE_FIELD_BEGIN,
  CSTATE_FIELD_DONE,
  CSTATE_FIELD_ID,
  CSTATE_FIELD_NAME,

  /* on labels data */
  CSTATE_FIELD_SUBLABEL,

  /* on artists data */
  CSTATE_FIELD_ALIAS,
  CSTATE_FIELD_MEMBER,

  /* on releases data */
  CSTATE_FIELD_ARTIST,
  CSTATE_FIELD_EXTRAARTIST,
  CSTATE_FIELD_LABEL,
  CSTATE_FIELD_TRACK_ARTIST
} cstate_field_e;

#define CTX_D_NONE -1

/* context hints for parser */
typedef enum {
  /* on labels data */
  /* on artists data */
  CSTATE_CTX_NONE,
  CSTATE_CTX_ALIASES,
  CSTATE_CTX_MEMBERS,
  /* on releases data */
  CSTATE_CTX_ARTISTS,
  CSTATE_CTX_EXTRAARTISTS,
  CSTATE_CTX_TRACKLIST,
  CSTATE_CTX_LABELS
} cstate_ctx_e;

typedef enum {
  CSTATE_MODE_PASS,
  CSTATE_MODE_TRUNCATE, // truncate after we hit CBUFMAX
  CSTATE_MODE_ASSERT    // assert if we hit CBUFMAX
} cstate_mode_e;

typedef struct {
  cstate_ctx_e ctx;
  int ctx_d;
  cstate_mode_e mode;
  cstate_field_e field;
  ptrdiff_t cbuflen;
  Bytef *cbuf;
} cstate_t;

const char *cstate_field_e_toname(cstate_field_e e) {
  switch (e) {
    case CSTATE_FIELD_NONE:         return "O";

    case CSTATE_FIELD_BEGIN:        return "B";
    case CSTATE_FIELD_DONE:         return "D";

    case CSTATE_FIELD_ID:           return "I";
    case CSTATE_FIELD_NAME:         return "N";

/* label-specific */
    case CSTATE_FIELD_SUBLABEL:     return "S";
/* artist-specific */
    case CSTATE_FIELD_ALIAS:        return "A";
    case CSTATE_FIELD_MEMBER:       return "M";
/* release-specific */
    case CSTATE_FIELD_ARTIST:       return "R";
    case CSTATE_FIELD_EXTRAARTIST:  return "E";
    case CSTATE_FIELD_LABEL:        return "L";
    case CSTATE_FIELD_TRACK_ARTIST: return "T";
    default:                        return "U";
  }
}

const char *yxml_ret_t_toname(yxml_ret_t r) {
  switch (r) {
    case YXML_ELEMSTART:  return "YXML_ELEMSTART";
    case YXML_ATTRSTART:  return "YXML_ATTRSTART";
    case YXML_ELEMEND:    return "YXML_ELEMEND";
    case YXML_ATTREND:    return "YXML_ATTREND";
    case YXML_CONTENT:    return "YXML_CONTENT";
    case YXML_ATTRVAL:    return "YXML_ATTRVAL";
    default:              return "UNMAPPED";
  }
}

#define match(e, dep, tname) if (r == e && *d == dep && strcmp(name, tname) == 0)
#define match2(e, ctxv, dep, tname) if (r == e && cstate->ctx == ctxv && *d == dep && strcmp(name, tname) == 0) 

void start(
    const yxml_ret_t r,
    const char * const name,
    int * const d,
    cstate_t * const cstate
    ) {
  DPRINT("START %s %d %s\n", yxml_ret_t_toname(r), *d, name);

  short start = 0;

#ifdef LABELS
  match(YXML_ELEMSTART, 1, "label")  { start = 1; }
  match(YXML_ELEMSTART, 2, "id")     { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_ID; }
  match(YXML_ELEMSTART, 2, "name")   { cstate->mode = CSTATE_MODE_TRUNCATE; cstate->field = CSTATE_FIELD_NAME; }
  match(YXML_ATTRSTART, 4, "id")     { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_SUBLABEL; }
#endif

#ifdef ARTISTS
  match(YXML_ELEMSTART, 1, "artist")   { start = 1; }
  match(YXML_ELEMSTART, 2, "id")       { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_ID; }
  match(YXML_ELEMSTART, 2, "name")     { cstate->mode = CSTATE_MODE_TRUNCATE; cstate->field = CSTATE_FIELD_NAME; }
  match(YXML_ELEMSTART, 2, "aliases")  { cstate->ctx = CSTATE_CTX_ALIASES; cstate->ctx_d = 2; }
  match(YXML_ELEMSTART, 2, "members")  { cstate->ctx = CSTATE_CTX_MEMBERS; cstate->ctx_d = 2; }
  match2(YXML_ATTRSTART, CSTATE_CTX_ALIASES, 4, "id") { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_ALIAS; }
  match2(YXML_ATTRSTART, CSTATE_CTX_MEMBERS, 4, "id") { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_MEMBER; }
#endif

#ifdef RELEASES
  match(YXML_ELEMSTART, 1, "release")        { start = 1; }
  match(YXML_ATTRSTART, 2, "id")             { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_ID; }
  match(YXML_ELEMSTART, 2, "title")          { cstate->mode = CSTATE_MODE_TRUNCATE; cstate->field = CSTATE_FIELD_NAME; }
  match(YXML_ELEMSTART, 2, "artists")        { cstate->ctx = CSTATE_CTX_ARTISTS; cstate->ctx_d = 2; }
  match(YXML_ELEMSTART, 2, "extraartists")   { cstate->ctx = CSTATE_CTX_EXTRAARTISTS; cstate->ctx_d = 2; }
  match(YXML_ELEMSTART, 2, "labels")         { cstate->ctx = CSTATE_CTX_LABELS; cstate->ctx_d = 2; }
  match(YXML_ELEMSTART, 2, "tracklist")      { cstate->ctx = CSTATE_CTX_TRACKLIST; cstate->ctx_d = 2; }
  match2(YXML_ELEMSTART, CSTATE_CTX_TRACKLIST, 6, "id")    { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_TRACK_ARTIST; }
  match2(YXML_ELEMSTART, CSTATE_CTX_ARTISTS, 4, "id")      { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_ARTIST; }
  match2(YXML_ATTRSTART, CSTATE_CTX_LABELS, 4, "id")       { cstate->mode = CSTATE_MODE_ASSERT; cstate->field = CSTATE_FIELD_LABEL; }
#endif

  if (start) {
    printf("%s\n", cstate_field_e_toname(CSTATE_FIELD_BEGIN));
  }

  if (cstate->mode) {
    cstate->cbuflen = 0;
  }

  *d += 1;
}

void end(
    const yxml_ret_t r,
    int * const d,
    cstate_t * const cstate
    ) {
  *d -= 1;

  DPRINT("END %s %d\n", yxml_ret_t_toname(r), *d);

  if (r == YXML_ELEMEND && *d == 1) {
    printf("%s\n", cstate_field_e_toname(CSTATE_FIELD_DONE));
  }

  if (*d == cstate->ctx_d) {
    cstate->ctx = CSTATE_CTX_NONE;
    cstate->ctx_d = CTX_D_NONE;
  }

  if (!cstate->mode) return;

  assert(cstate->cbuflen <= CBUFMAX);
  cstate->cbuf[cstate->cbuflen] = EOS;
  cstate->mode = 0;

  printf("%s%s\n", cstate_field_e_toname(cstate->field), cstate->cbuf);
}

int main(int argc, char *argv[]) {
  yxml_t *x = malloc(sizeof(yxml_t) + YXMLBUFSIZE);
  yxml_init(x, x+1 /* ptr to buffer */, YXMLBUFSIZE);

  /*
   * buffer for gz read from disk
   * TODO is this needed if we have pagecache?
   */
  Bytef *gzbuf = malloc(BUFSIZE);

  /* parser depth */
  int d = 0;

  Bytef cbuf[CBUFMAX];
  cstate_t cstate = { 
    .ctx      = CSTATE_CTX_NONE,
    .ctx_d    = CTX_D_NONE,
    .mode     = CSTATE_MODE_PASS,
    .field    = CSTATE_FIELD_NONE,
    .cbuflen  = 0,
    .cbuf     = cbuf
  };

  /* bytes read off from gz */
  int read;

  struct gzFile_s *infile = gzopen(argv[1], "rb");

  gzbuffer(infile, BUFSIZE);

  while (1) {
    read = gzread(infile, gzbuf, BUFSIZE);
    assert(read >= 0);
    /* nothing left to read */
    if (read == 0) break;

    for (ssize_t byte = 0; byte < read; byte++) {
      /* for each read byte, feed to parser */
      yxml_ret_t r = yxml_parse(x, gzbuf[byte]);

      switch (r) {
        case YXML_ELEMSTART:
          start(r, x->elem, &d, &cstate);
          break;
        case YXML_ATTRSTART:
          start(r, x->attr, &d, &cstate);
          break;
        case YXML_ELEMEND:
        case YXML_ATTREND:
          end(r, &d, &cstate);
          break;
        case YXML_CONTENT:
        case YXML_ATTRVAL:
          if (!cstate.mode) break;

          for (ptrdiff_t idx = 0; x->data[idx] != EOS; idx++) {
            assert(cstate.cbuflen <= CBUFMAX - 1);

            /* we've reached data write limit. Either assert
             * or drop additional content */
            if (cstate.cbuflen == CBUFMAX - 1) {
              assert(cstate.mode != CSTATE_MODE_ASSERT);
              break;
            }

            cstate.cbuf[cstate.cbuflen] = x->data[idx];
            cstate.cbuflen += 1;
          }

          break;
        default:
      }

      if (r < 0) { 
        FATAL("fatal when parsing");
        exit(1);
      }
    }
  }


  free(gzbuf);

  free(x);
  exit(0);
}
