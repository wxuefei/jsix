/* fwrite( const void *, size_t, size_t, FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <string.h>
#include "j6libc/glue.h"

size_t fwrite( const void * restrict ptr, size_t size, size_t nmemb, struct _PDCLIB_file_t * restrict stream )
{
    size_t offset = 0;
    /* TODO: lineend */
    /* int lineend = 0; */
    size_t nmemb_i;
    if ( _PDCLIB_prepwrite( stream ) == EOF )
    {
        return 0;
    }
    for ( nmemb_i = 0; nmemb_i < nmemb; ++nmemb_i )
    {
        size_t size_i;
        for ( size_i = 0; size_i < size; ++size_i )
        {
            if ( ( stream->buffer[ stream->bufidx++ ] = ((char*)ptr)[ nmemb_i * size + size_i ] ) == '\n' )
            {
                /* Remember last newline, in case we have to do a partial line-buffered flush */
                offset = stream->bufidx;
                /* lineend = true; */
            }
            if ( stream->bufidx == stream->bufsize )
            {
                if ( _PDCLIB_flushbuffer( stream ) == EOF )
                {
                    /* Returning number of objects completely buffered */
                    return nmemb_i;
                }
                /* lineend = false; */
            }
        }
    }
    /* Fully-buffered streams are OK. Non-buffered streams must be flushed,
       line-buffered streams only if there's a newline in the buffer.
    */
    switch ( stream->status & ( _IONBF | _IOLBF ) )
    {
        case _IONBF:
            if ( _PDCLIB_flushbuffer( stream ) == EOF )
            {
                /* We are in a pinch here. We have an error, which requires a
                   return value < nmemb. On the other hand, all objects have
                   been written to buffer, which means all the caller had to
                   do was removing the error cause, and re-flush the stream...
                   Catch 22. We'll return a value one short, to indicate the
                   error, and can't really do anything about the inconsistency.
                */
                return nmemb_i - 1;
            }
            break;
        case _IOLBF:
            {
            size_t bufidx = stream->bufidx;
            stream->bufidx = offset;
            if ( _PDCLIB_flushbuffer( stream ) == EOF )
            {
                /* See comment above. */
                stream->bufidx = bufidx;
                return nmemb_i - 1;
            }
            stream->bufidx = bufidx - offset;
            memmove( stream->buffer, stream->buffer + offset, stream->bufidx );
            }
    }
    return nmemb_i;
}
