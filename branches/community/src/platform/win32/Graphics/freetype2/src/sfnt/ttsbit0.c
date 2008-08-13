/***************************************************************************/
/*                                                                         */
/*  ttsbit0.c                                                              */
/*                                                                         */
/*    TrueType and OpenType embedded bitmap support (body).                */
/*    This is a heap-optimized version.                                    */
/*                                                                         */
/*  Copyright 2005 by                                                      */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include <ft2build.h>
#include FT_INTERNAL_DEBUG_H
#include FT_INTERNAL_STREAM_H
#include FT_TRUETYPE_TAGS_H
#include "ttsbit.h"

#include "sferrors.h"


  /*************************************************************************/
  /*                                                                       */
  /* The macro FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  FT_COMPONENT
#define FT_COMPONENT  trace_ttsbit


    static const FT_Frame_Field  tt_sbit_line_metrics_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_SBit_LineMetricsRec

      /* no FT_FRAME_START */
        FT_FRAME_CHAR( ascender ),
        FT_FRAME_CHAR( descender ),
        FT_FRAME_BYTE( max_width ),

        FT_FRAME_CHAR( caret_slope_numerator ),
        FT_FRAME_CHAR( caret_slope_denominator ),
        FT_FRAME_CHAR( caret_offset ),

        FT_FRAME_CHAR( min_origin_SB ),
        FT_FRAME_CHAR( min_advance_SB ),
        FT_FRAME_CHAR( max_before_BL ),
        FT_FRAME_CHAR( min_after_BL ),
        FT_FRAME_CHAR( pads[0] ),
        FT_FRAME_CHAR( pads[1] ),
      FT_FRAME_END
    };

    static const FT_Frame_Field  tt_strike_start_fields[] =
    {
#undef  FT_STRUCTURE
#define FT_STRUCTURE  TT_SBit_StrikeRec

      /* no FT_FRAME_START */
        FT_FRAME_ULONG( ranges_offset ),
        FT_FRAME_SKIP_LONG,
        FT_FRAME_ULONG( num_ranges ),
        FT_FRAME_ULONG( color_ref ),
      FT_FRAME_END
    };

    static const FT_Frame_Field  tt_strike_end_fields[] =
    {
      /* no FT_FRAME_START */
        FT_FRAME_USHORT( start_glyph ),
        FT_FRAME_USHORT( end_glyph ),
        FT_FRAME_BYTE  ( x_ppem ),
        FT_FRAME_BYTE  ( y_ppem ),
        FT_FRAME_BYTE  ( bit_depth ),
        FT_FRAME_CHAR  ( flags ),
      FT_FRAME_END
    };


  FT_LOCAL_DEF( FT_Error )
  tt_face_load_sbit_strikes( TT_Face    face,
                             FT_Stream  stream )
  {
    FT_Error   error  = SFNT_Err_Ok;
    FT_Fixed   version;
    FT_ULong   num_strikes, table_size;
    FT_Byte*   p;
    FT_Byte*   p_limit;
    FT_UInt    nn, count;


    face->sbit_num_strikes = 0;

    /* this table is optional */
    error = face->goto_table( face, TTAG_EBLC, stream, &table_size );
    if ( error )
      error = face->goto_table( face, TTAG_bloc, stream, &table_size );
    if ( error )
      goto Exit;

    if ( table_size < 8 )
    {
      FT_ERROR(( "%s: table too short!\n", "tt_face_load_sbit_strikes" ));
      error = SFNT_Err_Invalid_File_Format;
      goto Exit;
    }

    if ( FT_FRAME_EXTRACT( table_size, face->sbit_table ) )
      goto Exit;

    face->sbit_table_size = table_size;

    p       = face->sbit_table;
    p_limit = p + table_size;

    version     = FT_NEXT_ULONG( p );
    num_strikes = FT_NEXT_ULONG( p );

    if ( version != 0x00020000UL || num_strikes >= 0x10000UL )
    {
      FT_ERROR(( "%s: invalid table version!\n",
                 "tt_face_load_sbit_strikes" ));
      error = SFNT_Err_Invalid_File_Format;
      goto Fail;
    }

    /*
     *  Count the number of strikes available in the table.  We are a bit
     *  paranoid there and don't trust the data.
     */
    count = (FT_UInt)num_strikes;
    if ( 8 +48UL * count > table_size )
      count = (FT_UInt)( ( p_limit - p ) / 48 );

    face->sbit_num_strikes = count;

    /*
     *  Now allocate the root array of FT_Bitmap_Size records and
     *  populate them.  Unfortunately, it isn't possible to indicate bit
     *  depths in the FT_Bitmap_Size record.  This is a design error.
     */
    {
      FT_Memory  memory  = face->root.stream->memory;
      FT_UInt    em_size = (FT_UInt) face->header.Units_Per_EM;
      FT_Short   height  = (FT_Short)( face->horizontal.Ascender -
                                       face->horizontal.Descender +
                                       face->horizontal.Line_Gap );

      FT_Short   avgwidth = face->os2.xAvgCharWidth;


      if ( FT_NEW_ARRAY( face->root.available_sizes, count ) )
        goto Fail;

      for ( nn = 0; nn < count; nn++ )
      {
        FT_Bitmap_Size*  bsize = face->root.available_sizes + nn;
        FT_UInt          x_ppem, y_ppem;


        x_ppem = p[44];
        y_ppem = p[45];

        bsize->x_ppem = (FT_Pos)(x_ppem << 6);
        bsize->y_ppem = (FT_Pos)(y_ppem << 6);

        bsize->height = (FT_Short)( height*y_ppem   + em_size / 2 ) / em_size;
        bsize->width  = (FT_Short)( avgwidth*y_ppem + em_size / 2 ) / em_size;
        bsize->size   = bsize->y_ppem;

        p += 48;
      }

      face->root.face_flags     |= FT_FACE_FLAG_FIXED_SIZES;
      face->root.num_fixed_sizes = count;
    }

  Exit:
    return error;

  Fail:
    FT_FRAME_RELEASE( face->sbit_table );
    face->sbit_table_size = 0;
    goto Exit;
  }


  FT_LOCAL_DEF( void )
  tt_face_free_sbit_strikes( TT_Face  face )
  {
    FT_Stream  stream = face->root.stream;


    FT_FRAME_RELEASE( face->sbit_table );
    face->sbit_table_size  = 0;
    face->sbit_num_strikes = 0;
  }


  FT_LOCAL_DEF( FT_Error )
  tt_face_set_sbit_strike( TT_Face    face,
                           FT_UInt    x_ppem,
                           FT_UInt    y_ppem,
                           FT_ULong  *astrike_index )
  {
    FT_UInt   nn, count;
    FT_Byte*  p;
    FT_Byte*  p_limit;


    if ( x_ppem > 255               ||
         y_ppem < 1 || y_ppem > 255 )
      return SFNT_Err_Invalid_PPem;

    p       = face->sbit_table + 8;
    p_limit = p + face->sbit_table_size;
    count   = face->sbit_num_strikes;

    for ( nn = 0; nn < count; nn++ )
    {
      if ( x_ppem == (FT_UInt)p[44] && y_ppem == (FT_UInt)p[45] )
      {
        *astrike_index = (FT_ULong)nn;
        return SFNT_Err_Ok;
      }
      p += 48;
    }

    return SFNT_Err_Invalid_PPem;
  }


  typedef struct
  {
    TT_Face          face;
    FT_Stream        stream;
    FT_Bitmap*       bitmap;
    TT_SBit_Metrics  metrics;
    FT_Bool          metrics_loaded;
    FT_Bool          bitmap_allocated;
    FT_Byte          bit_depth;

    FT_ULong         ebdt_start;
    FT_ULong         ebdt_size;

    FT_ULong         strike_index_array;
    FT_ULong         strike_index_count;
    FT_Byte*         eblc_base;
    FT_Byte*         eblc_limit;

  } TT_SBitDecoderRec, *TT_SBitDecoder;


  static FT_Error
  tt_sbit_decoder_init( TT_SBitDecoder       decoder,
                        TT_Face              face,
                        FT_ULong             strike_index,
                        TT_SBit_MetricsRec*  metrics )
  {
    FT_Error   error;
    FT_Stream  stream     = face->root.stream;
    FT_ULong   ebdt_size;


    error = face->goto_table( face, TTAG_EBDT, stream, &ebdt_size );
    if ( error )
      error = face->goto_table( face, TTAG_bdat, stream, &ebdt_size );
    if ( error )
      goto Exit;

    decoder->face    = face;
    decoder->stream  = stream;
    decoder->bitmap  = &face->root.glyph->bitmap;
    decoder->metrics = metrics;

    decoder->metrics_loaded   = 0;
    decoder->bitmap_allocated = 0;

    decoder->ebdt_start = FT_STREAM_POS();
    decoder->ebdt_size  = ebdt_size;

    decoder->eblc_base  = face->sbit_table;
    decoder->eblc_limit = face->sbit_table + face->sbit_table_size;

    /* now find the strike corresponding to the index */
    {
      FT_Byte*  p = decoder->eblc_base + 8 + 48 * strike_index;


      decoder->strike_index_array = FT_NEXT_ULONG( p );
      p                          += 4;
      decoder->strike_index_count = FT_NEXT_ULONG( p );
      p                          += 34;
      decoder->bit_depth          = *p;
    }

  Exit:
    return error;
  }


  static void
  tt_sbit_decoder_done( TT_SBitDecoder  decoder )
  {
    FT_UNUSED( decoder );
  }


  static FT_Error
  tt_sbit_decoder_alloc_bitmap( TT_SBitDecoder  decoder )
  {
    FT_Error    error = SFNT_Err_Ok;
    FT_UInt     width, height;
    FT_Bitmap*  map = decoder->bitmap;
    FT_Long     size;


    if ( !decoder->metrics_loaded )
    {
      error = SFNT_Err_Invalid_Argument;
      goto Exit;
    }

    width  = decoder->metrics->width;
    height = decoder->metrics->height;

    map->width = (int)width;
    map->rows  = (int)height;

    switch ( decoder->bit_depth )
    {
    case 1:
      map->pixel_mode = FT_PIXEL_MODE_MONO;
      map->pitch      = ( map->width + 7 ) >> 3;
      break;

    case 2:
      map->pixel_mode = FT_PIXEL_MODE_GRAY2;
      map->pitch      = ( map->width + 3 ) >> 2;
      break;

    case 4:
      map->pixel_mode = FT_PIXEL_MODE_GRAY4;
      map->pitch      = ( map->width + 1 ) >> 1;
      break;

    case 8:
      map->pixel_mode = FT_PIXEL_MODE_GRAY;
      map->pitch      = map->width;
      break;

    default:
      error = SFNT_Err_Invalid_File_Format;
      goto Exit;
    }

    size = map->rows * map->pitch;

    /* check that there is no empty image */
    if ( size == 0 )
      goto Exit;     /* exit successfully! */

    error = ft_glyphslot_alloc_bitmap( decoder->face->root.glyph, size );
    if ( error )
      goto Exit;

    decoder->bitmap_allocated = 1;

  Exit:
    return error;
  }


  static FT_Error
  tt_sbit_decoder_load_metrics( TT_SBitDecoder  decoder,
                                FT_Byte*       *pp,
                                FT_Byte*        limit,
                                FT_Bool         big )
  {
    FT_Byte*         p       = *pp;
    TT_SBit_Metrics  metrics = decoder->metrics;


    if ( p + 5 > limit )
      goto Fail;

    if ( !decoder->metrics_loaded )
    {
      metrics->height       = p[0];
      metrics->width        = p[1];
      metrics->horiBearingX = (FT_Char)p[2];
      metrics->horiBearingY = (FT_Char)p[3];
      metrics->horiAdvance  = p[4];
    }

    p += 5;
    if ( big )
    {
      if ( p + 3 > limit )
        goto Fail;

      if ( !decoder->metrics_loaded )
      {
        metrics->vertBearingX = (FT_Char)p[0];
        metrics->vertBearingY = (FT_Char)p[1];
        metrics->vertAdvance  = p[2];
      }

      p += 3;
    }

    decoder->metrics_loaded = 1;
    *pp = p;
    return 0;

  Fail:
    return SFNT_Err_Invalid_Argument;
  }


  /* forward declaration */
  static FT_Error
  tt_sbit_decoder_load_image( TT_SBitDecoder  decoder,
                              FT_UInt         glyph_index,
                              FT_Int          x_pos,
                              FT_Int          y_pos );

  typedef FT_Error  (*TT_SBitDecoder_LoadFunc)( TT_SBitDecoder  decoder,
                                                FT_Byte*        p,
                                                FT_Byte*        plimit,
                                                FT_Int          x_pos,
                                                FT_Int          y_pos );


  static FT_Error
  tt_sbit_decoder_load_byte_aligned( TT_SBitDecoder  decoder,
                                     FT_Byte*        p,
                                     FT_Byte*        limit,
                                     FT_Int          x_pos,
                                     FT_Int          y_pos )
  {
    FT_Error    error = SFNT_Err_Ok;
    FT_Byte*    line;
    FT_Int      bit_height, bit_width, pitch, width, height, h;
    FT_Bitmap*  bitmap;


    if ( !decoder->bitmap_allocated )
    {
      error = tt_sbit_decoder_alloc_bitmap( decoder );
      if ( error )
        goto Exit;
    }

    /* check that we can write the glyph into the bitmap */
    bitmap     = decoder->bitmap;
    bit_width  = bitmap->width;
    bit_height = bitmap->rows;
    pitch      = bitmap->pitch;
    line       = bitmap->buffer;

    width  = decoder->metrics->width;
    height = decoder->metrics->height;

    if ( x_pos < 0 || x_pos + width > bit_width   ||
         y_pos < 0 || y_pos + height > bit_height )
    {
      error = SFNT_Err_Invalid_File_Format;
      goto Exit;
    }

    if ( p + ( ( width + 7 ) >> 3 ) * height > limit )
    {
      error = SFNT_Err_Invalid_File_Format;
      goto Exit;
    }

    /* now do the blit */
    line  += y_pos * pitch + ( x_pos >> 3 );
    x_pos &= 7;

    if ( x_pos == 0 )  /* the easy one */
    {
      for ( h = height; h > 0; h--, line += pitch )
      {
        FT_Byte*  write = line;
        FT_Int    w;


        for ( w = width; w >= 8; w -= 8 )
        {
          write[0] = (FT_Byte)( write[0] | *p++ );
          write   += 1;
        }

        if ( w > 0 )
          write[0] = (FT_Byte)( write[0] | ( *p++ & ( 0xFF00U >> w ) ) );
      }
    }
    else  /* x_pos > 0 */
    {
      for ( h = height; h > 0; h--, line += pitch )
      {
        FT_Byte*  write = line;
        FT_Int    w;
        FT_UInt   wval = 0;


        for ( w = width; w >= 8; w -= 8 )
        {
          wval      = (FT_UInt)( wval | *p++ );
          write[0]  = (FT_Byte)( write[0] | ( wval >> x_pos ) );
          write    += 1;
          wval    <<= 8;
        }

        if ( w > 0 )
          wval = (FT_UInt)(wval | ( *p++ & ( 0xFF00U >> w ) ) );

        write[0] = (FT_Byte)( write[0] | ( wval >> x_pos ) );
      }
    }

  Exit:
    return error;
  }


  static FT_Error
  tt_sbit_decoder_load_bit_aligned( TT_SBitDecoder  decoder,
                                    FT_Byte*        p,
                                    FT_Byte*        limit,
                                    FT_Int          x_pos,
                                    FT_Int          y_pos )
  {
    FT_Error    error = SFNT_Err_Ok;
    FT_Byte*    line;
    FT_Int      bit_height, bit_width, pitch, width, height, h;
    FT_Bitmap*  bitmap;
    FT_UInt32   rval;


    if ( !decoder->bitmap_allocated )
    {
      error = tt_sbit_decoder_alloc_bitmap( decoder );
      if ( error )
        goto Exit;
    }

    /* check that we can write the glyph into the bitmap */
    bitmap     = decoder->bitmap;
    bit_width  = bitmap->width;
    bit_height = bitmap->rows;
    pitch      = bitmap->pitch;
    line       = bitmap->buffer;

    width  = decoder->metrics->width;
    height = decoder->metrics->height;

    if ( x_pos < 0 || x_pos + width  > bit_width  ||
         y_pos < 0 || y_pos + height > bit_height )
    {
      error = SFNT_Err_Invalid_File_Format;
      goto Exit;
    }

    if ( p + ( ( width + 7 ) >> 3 ) * height > limit )
    {
      error = SFNT_Err_Invalid_File_Format;
      goto Exit;
    }

    /* now do the blit */
    line  += y_pos * pitch + ( x_pos >> 3 );
    x_pos &= 7;
    rval   = 0x10000UL;

    for ( h = height; h > 0; h--, line += pitch )
    {
      FT_Byte*   write = line;
      FT_UInt32  wval  = 0x100 << x_pos;
      FT_Int     w;


      for ( w = width; w >= 8; w -= 8 )
      {
        if ( rval & 0x10000UL )
          rval = 0x100 | *p++;

        wval |= rval & 0x80;

        wval <<= 1;
        rval <<= 1;

        if ( wval & 0x10000UL )
        {
          write[0] = (FT_Byte)( write[0] | ( wval >> 8 ) );
          write   += 1;
          wval     = 0x100;
        }
      }

      if ( wval != 0x100 )
      {
        while ( wval > 0x1FF )
          wval >>= 1;

        write[0] = (FT_Byte)( write[0] | wval );
      }
    }

  Exit:
    return error;
  }


  static FT_Error
  tt_sbit_decoder_load_compound( TT_SBitDecoder  decoder,
                                 FT_Byte*        p,
                                 FT_Byte*        limit,
                                 FT_Int          x_pos,
                                 FT_Int          y_pos )
  {
    FT_Error  error = SFNT_Err_Ok;
    FT_UInt   num_components, nn;


    if ( p + 2 > limit )
      goto Fail;

    num_components = FT_NEXT_USHORT( p );
    if ( p + 4 * num_components > limit )
      goto Fail;

    for ( nn = 0; nn < num_components; nn++ )
    {
      FT_UInt   gindex = FT_NEXT_USHORT( p );
      FT_Byte   dx     = FT_NEXT_BYTE( p );
      FT_Byte   dy     = FT_NEXT_BYTE( p );


      /* NB: a recursive call */
      error = tt_sbit_decoder_load_image( decoder, gindex,
                                          x_pos + dx, y_pos + dy );
      if ( error )
        break;
    }

  Exit:
    return error;

  Fail:
    error = SFNT_Err_Invalid_File_Format;
    goto Exit;
  }


  static FT_Error
  tt_sbit_decoder_load_bitmap( TT_SBitDecoder  decoder,
                               FT_UInt         glyph_format,
                               FT_ULong        glyph_start,
                               FT_ULong        glyph_size,
                               FT_Int          x_pos,
                               FT_Int          y_pos )
  {
    FT_Error   error;
    FT_Stream  stream = decoder->stream;
    FT_Byte*   p;
    FT_Byte*   p_limit;
    FT_Byte*   data;


    /* seek into the EBDT table now */
    if ( glyph_start + glyph_size > decoder->ebdt_size )
    {
      error = SFNT_Err_Invalid_Argument;
      goto Exit;
    }

    if ( FT_STREAM_SEEK( decoder->ebdt_start + glyph_start ) ||
         FT_FRAME_EXTRACT( glyph_size, data )                )
      goto Exit;

    p       = data;
    p_limit = p + glyph_size;

    /* read the data, depending on the glyph format */
    switch ( glyph_format )
    {
    case 1:
    case 2:
    case 8:
      error = tt_sbit_decoder_load_metrics( decoder, &p, p_limit, 0 );
      break;

    case 6:
    case 7:
    case 9:
      error = tt_sbit_decoder_load_metrics( decoder, &p, p_limit, 1 );
      break;

    default:
      error = SFNT_Err_Ok;
    }

    if ( error )
      goto Fail;

    {
      TT_SBitDecoder_LoadFunc  loader;


      switch ( glyph_format )
      {
      case 1:
      case 6:
        loader = tt_sbit_decoder_load_byte_aligned;
        break;

      case 2:
      case 5:
      case 7:
        loader = tt_sbit_decoder_load_bit_aligned;
        break;

      case 8:
        if ( p + 1 > p_limit )
          goto Fail;

        p += 1;  /* skip padding */
        /* fall-through */

      case 9:
        loader = tt_sbit_decoder_load_compound;
        break;

      default:
        goto Fail;
      }

      error = loader( decoder, p, p_limit, x_pos, y_pos );
    }

  Fail:
    FT_FRAME_RELEASE( data );

  Exit:
    return error;
  }


  static FT_Error
  tt_sbit_decoder_load_image( TT_SBitDecoder  decoder,
                              FT_UInt         glyph_index,
                              FT_Int          x_pos,
                              FT_Int          y_pos )
  {
    /*
     *  First, we find the correct strike range that applies to this
     *  glyph index.
     */

    FT_Byte*  p          = decoder->eblc_base + decoder->strike_index_array;
    FT_Byte*  p_limit    = decoder->eblc_limit;
    FT_ULong  num_ranges = decoder->strike_index_count;
    FT_UInt   start, end, index_format, image_format;
    FT_ULong  image_start = 0, image_end = 0, image_offset;


    if ( p + 8 * num_ranges > p_limit )
      goto NoBitmap;

    for ( ; num_ranges > 0; num_ranges-- )
    {
      start = FT_NEXT_USHORT( p );
      end   = FT_NEXT_USHORT( p );

      if ( glyph_index >= start && glyph_index <= end )
        goto FoundRange;

      p += 4;  /* ignore index offset */
    }
    goto NoBitmap;

  FoundRange:
    image_offset = FT_NEXT_ULONG( p );
    p = decoder->eblc_base + decoder->strike_index_array + image_offset;
    if ( p + 8 > p_limit )
      goto NoBitmap;

    /* now find the glyph's location and extend within the ebdt table */
    index_format = FT_NEXT_USHORT( p );
    image_format = FT_NEXT_USHORT( p );
    image_offset = FT_NEXT_ULONG ( p );

    switch ( index_format )
    {
    case 1: /* 4-byte offsets relative to `image_offset' */
      {
        p += 4 * ( glyph_index - start );
        if ( p + 8 > p_limit )
          goto NoBitmap;

        image_start = FT_NEXT_ULONG( p );
        image_end   = FT_NEXT_ULONG( p );

        if ( image_start == image_end )  /* missing glyph */
          goto NoBitmap;
      }
      break;

    case 2: /* big metrics, constant image size */
      {
        FT_ULong  image_size;


        if ( p + 12 > p_limit )
          goto NoBitmap;

        image_size = FT_NEXT_ULONG( p );

        if ( tt_sbit_decoder_load_metrics( decoder, &p, p_limit, 1 ) )
          goto NoBitmap;

        image_start = image_offset + image_size * ( glyph_index - start );
        image_end   = image_start  + image_size;
      }
      break;

    case 3: /* 2-byte offsets relative to 'image_offset' */
      {
        p += 2 * ( glyph_index - start );
        if ( p + 4 > p_limit )
          goto NoBitmap;

        image_start = FT_NEXT_USHORT( p );
        image_end   = FT_NEXT_USHORT( p );

        if ( image_start == image_end )  /* missing glyph */
          goto NoBitmap;
      }
      break;

    case 4: /* sparse glyph array with (glyph,offset) pairs */
      {
        FT_ULong  mm, num_glyphs;


        if ( p + 4 > p_limit )
          goto NoBitmap;

        num_glyphs = FT_NEXT_ULONG( p );
        if ( p + ( num_glyphs + 1 ) * 4 > p_limit )
          goto NoBitmap;

        for ( mm = 0; mm < num_glyphs; mm++ )
        {
          FT_UInt  gindex = FT_NEXT_USHORT( p );


          if ( gindex == glyph_index )
          {
            image_start = FT_NEXT_USHORT( p );
            p          += 2;
            image_end   = FT_PEEK_USHORT( p );
            break;
          }
          p += 2;
        }

        if ( mm >= num_glyphs )
          goto NoBitmap;
      }
      break;

    case 5: /* constant metrics with sparse glyph codes */
      {
        FT_ULong  image_size, mm, num_glyphs;


        if ( p + 16 > p_limit )
          goto NoBitmap;

        image_size = FT_NEXT_ULONG( p );

        if ( tt_sbit_decoder_load_metrics( decoder, &p, p_limit, 1 ) )
          goto NoBitmap;

        num_glyphs = FT_NEXT_ULONG( p );
        if ( p + 2 * num_glyphs > p_limit )
          goto NoBitmap;

        for ( mm = 0; mm < num_glyphs; mm++ )
        {
          FT_UInt  gindex = FT_NEXT_USHORT( p );


          if ( gindex == glyph_index )
            break;
        }

        if ( mm >= num_glyphs )
          goto NoBitmap;

        image_start = image_offset + image_size*mm;
        image_end   = image_start + image_size;
      }
      break;

    default:
      goto NoBitmap;
    }

    if ( image_start > image_end )
      goto NoBitmap;

    image_end  -= image_start;
    image_start = image_offset + image_start;

    return tt_sbit_decoder_load_bitmap( decoder,
                                        image_format,
                                        image_start,
                                        image_end,
                                        x_pos,
                                        y_pos );

  NoBitmap:
    return SFNT_Err_Invalid_Argument;
  }


  FT_LOCAL( FT_Error )
  tt_face_load_sbit_image( TT_Face              face,
                           FT_ULong             strike_index,
                           FT_UInt              glyph_index,
                           FT_UInt              load_flags,
                           FT_Stream            stream,
                           FT_Bitmap           *map,
                           TT_SBit_MetricsRec  *metrics )
  {
    TT_SBitDecoderRec  decoder[1];
    FT_Error           error;

    FT_UNUSED( load_flags );
    FT_UNUSED( stream );
    FT_UNUSED( map );


    error = tt_sbit_decoder_init( decoder, face, strike_index, metrics );
    if ( !error )
    {
      error = tt_sbit_decoder_load_image( decoder, glyph_index, 0, 0 );
      tt_sbit_decoder_done( decoder );
    }

    return error;
  }

/* EOF */
