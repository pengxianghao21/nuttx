/****************************************************************************
 * libs/libc/stream/lib_fileoutstream.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <nuttx/fs/fs.h>

#include "libc.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rawoutstream_puts
 ****************************************************************************/

static ssize_t fileoutstream_puts(FAR struct lib_outstream_s *self,
                                  FAR const void *buf, size_t len)
{
  FAR struct lib_fileoutstream_s *stream =
                                  (FAR struct lib_fileoutstream_s *)self;
  ssize_t nwritten;

  do
    {
      nwritten = file_write(stream->file, buf, len);
      if (nwritten >= 0)
        {
          self->nput += nwritten;
          return nwritten;
        }

      /* The only expected error is EINTR, meaning that the write operation
       * was awakened by a signal.  Zero would not be a valid return value
       * from _NX_WRITE().
       */

      DEBUGASSERT(nwritten < 0);
    }
  while (nwritten == -EINTR);

  return nwritten;
}

/****************************************************************************
 * Name: rawoutstream_putc
 ****************************************************************************/

static void fileoutstream_putc(FAR struct lib_outstream_s *self, int ch)
{
  char tmp = ch;
  fileoutstream_puts(self, &tmp, 1);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lib_fileoutstream
 *
 * Description:
 *   Initializes a stream for use with a file descriptor.
 *
 * Input Parameters:
 *   stream - User allocated, uninitialized instance of struct
 *            lib_rawoutstream_s to be initialized.
 *   file   - User provided FILE instance (must have been opened for
 *            write access).
 *
 * Returned Value:
 *   None (User allocated instance initialized).
 *
 ****************************************************************************/

void lib_fileoutstream(FAR struct lib_fileoutstream_s *stream,
                       FAR struct file *file)
{
  stream->common.putc  = fileoutstream_putc;
  stream->common.puts  = fileoutstream_puts;
  stream->common.flush = lib_noflush;
  stream->common.nput  = 0;
  stream->file         = file;
}
