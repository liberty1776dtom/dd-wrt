/* Output stream referring to a file descriptor.
   Copyright (C) 2006-2007 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2006.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "fd-ostream.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "full-write.h"
#include "xalloc.h"
#include "gettext.h"

#define _(str) gettext (str)

struct fd_ostream : struct ostream
{
fields:
  int fd;
  char *filename;
  char *buffer;                 /* A buffer, or NULL.  */
  size_t avail;                 /* Number of bytes available in the buffer.  */
};

#define BUFSIZE 4096

/* Implementation of ostream_t methods.  */

static void
fd_ostream::write_mem (fd_ostream_t stream, const void *data, size_t len)
{
  if (len > 0)
    {
      if (stream->buffer != NULL)
        {
          /* Buffered.  */
          assert (stream->avail > 0);
          #if 0 /* unoptimized */
          do
            {
              size_t n = (len <= stream->avail ? len : stream->avail);
              if (n > 0)
                {
                  memcpy (stream->buffer + BUFSIZE - stream->avail, data, n);
                  data = (char *) data + n;
                  stream->avail -= n;
                  len -= n;
                }
              if (stream->avail == 0)
                {
                  if (full_write (stream->fd, stream->buffer, BUFSIZE) < BUFSIZE)
                    error (EXIT_FAILURE, errno, _("error writing to %s"),
                           stream->filename);
                  stream->avail = BUFSIZE;
                }
            }
          while (len > 0);
          #else /* optimized */
          if (len < stream->avail)
            {
              /* Move the data into the buffer.  */
              memcpy (stream->buffer + BUFSIZE - stream->avail, data, len);
              stream->avail -= len;
            }
          else
            {
              /* Split the data into:
                   - a first chunk, which is added to the buffer and output,
                   - a series of chunks of size BUFSIZE, which can be output
                     directly, without going through the buffer, and
                   - a last chunk, which is copied to the buffer.  */
              size_t n = stream->avail;
              memcpy (stream->buffer + BUFSIZE - stream->avail, data, n);
              data = (char *) data + n;
              len -= n;
              if (full_write (stream->fd, stream->buffer, BUFSIZE) < BUFSIZE)
                error (EXIT_FAILURE, errno, _("error writing to %s"),
                       stream->filename);

              while (len >= BUFSIZE)
                {
                  if (full_write (stream->fd, data, BUFSIZE) < BUFSIZE)
                    error (EXIT_FAILURE, errno, _("error writing to %s"),
                           stream->filename);
                  data = (char *) data + BUFSIZE;
                  len -= BUFSIZE;
                }

              if (len > 0)
                memcpy (stream->buffer, data, len);
              stream->avail = BUFSIZE - len;
            }
          #endif
          assert (stream->avail > 0);
        }
      else
        {
          /* Unbuffered.  */
          if (full_write (stream->fd, data, len) < len)
            error (EXIT_FAILURE, errno, _("error writing to %s"),
                   stream->filename);
        }
    }
}

static void
fd_ostream::flush (fd_ostream_t stream)
{
  if (stream->buffer != NULL && stream->avail < BUFSIZE)
    {
      size_t filled = BUFSIZE - stream->avail;
      if (full_write (stream->fd, stream->buffer, filled) < filled)
        error (EXIT_FAILURE, errno, _("error writing to %s"), stream->filename);
      stream->avail = BUFSIZE;
    }
}

static void
fd_ostream::free (fd_ostream_t stream)
{
  fd_ostream_flush (stream);
  free (stream->filename);
  free (stream);
}

/* Constructor.  */

fd_ostream_t
fd_ostream_create (int fd, const char *filename, bool buffered)
{
  fd_ostream_t stream =
    (struct fd_ostream_representation *)
    xmalloc (sizeof (struct fd_ostream_representation)
             + (buffered ? BUFSIZE : 0));

  stream->base.vtable = &fd_ostream_vtable;
  stream->fd = fd;
  stream->filename = xstrdup (filename);
  if (buffered)
    {
      stream->buffer =
        (char *) (void *) stream + sizeof (struct fd_ostream_representation);
      stream->avail = BUFSIZE;
    }
  else
    stream->buffer = NULL;

  return stream;
}
