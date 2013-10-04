#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <vector>
#include <windows.h>

using namespace std;


enum
{
	FILE_TYPE_GIF =	1,
	FILE_TYPE_BMP
};

typedef struct
{
  unsigned char r;
  unsigned char g;
  unsigned char b;
}
rgb;

typedef struct
{
  unsigned short image_left_position;
  unsigned short image_top_position;
  unsigned short image_width;
  unsigned short image_height;
  unsigned char fields;
}
image_descriptor_t;

typedef struct
{
  unsigned char byte;
  int prev;
  int len;
}
dictionary_entry_t;

typedef struct
{
  unsigned char extension_code;
  unsigned char block_size;
}
extension_t;

typedef struct
{
  unsigned char fields;
  unsigned short delay_time;
  unsigned char transparent_color_index;
}
graphic_control_extension_t;

typedef struct
{
  unsigned char application_id[ 8 ];
  unsigned char version[ 3 ];
}
application_extension_t;

typedef struct
{
  unsigned short left;
  unsigned short top;
  unsigned short width;
  unsigned short height;
  unsigned char cell_width;
  unsigned char cell_height;
  unsigned char foreground_color;
  unsigned char background_color;
}
plaintext_extension_t;

typedef struct
{
	vector<unsigned char> data;
}
frame_t;

typedef struct
{
  vector<frame_t> frames;
  unsigned short width;
  unsigned short height;
  unsigned short interval;
}
frame_store_t;

void dprintf(int debug_level, char *fmt, ...);
void OutputHelp(void);
void DumbBufferToHex(unsigned char *buffer, int length);
void DumpImg(unsigned char *buffer, int length, int width);
int ProcessGifFile(void);
int process_image_descriptor(unsigned char **file, rgb *gct, int gct_size, int resolution_bits);
static int read_sub_blocks(unsigned char **gif_file, unsigned char **data );
int uncompress(int code_length, const unsigned char *input, int input_length, unsigned char *out);
static int process_extension(unsigned char **gif_file);
void StoreFrame(const unsigned char *data, int length, int width, int height);
void PrintFrames(frame_store_t *frames);
void OutputToArray(char *filename, frame_store_t *frames);

int DebugSetting = 3;


#define EXTENSION_INTRODUCER   0x21
#define IMAGE_DESCRIPTOR       0x2C
#define TRAILER                0x3B

#define GRAPHIC_CONTROL        0xF9
#define APPLICATION_EXTENSION  0xFF
#define COMMENT_EXTENSION      0xFE
#define PLAINTEXT_EXTENSION    0x01

#define MAXFILELEN 1000000
char Filename[256];
unsigned char FileBuffer[MAXFILELEN + 1];
int FileLength;
char FileType;

frame_store_t Frames;

//====================================================================================
int main(int argc, char *argv[])
{
	int bytes_read;

	// Print application header
	printf("==========================================================================\r\n");
	printf("|                            ImgToArray                                  |\r\n");
	printf("| www.BlackRamElectronics.com   --   www.GitHub.com/BlackRamElectronics  |\r\n");
	printf("| This aplication converts an image file to a C array                    |\r\n");
	printf("| Currently only supporting monochrome output and GIF files              |\r\n");
	printf("==========================================================================\r\n");

	// Check that a file has been passed
	if(argc <= 1)
	{
		OutputHelp();
		return(1);
	}

	// Store the file name
	strncpy(Filename, argv[1], sizeof(Filename));
	
	printf("Opening file %s\r\n", Filename);
	
	// Check file extension for file type, we will confirm this once the file is opened
	if((strncmp(&Filename[strlen(Filename) - 3], "gif", 3) == 0) ||
		(strncmp(&Filename[strlen(Filename) - 3], "GIF", 3) == 0))
	{
		printf("Processing file as GIF\r\n");
		FileType = FILE_TYPE_GIF;
	}
	else
	{
		printf("Unsupported file extension\r\n");
		return(0);
	}
	
	// Read file to buffer
	FILE *fp = fopen(Filename, "rb");
	
	// obtain file size:
	fseek (fp , 0 , SEEK_END);
	FileLength = ftell (fp);
	rewind (fp);
	printf("File size: %d bytes\r\n", FileLength);
	
	if(fp != NULL)
	{
		//bytes_read = fread(FileBuffer, sizeof(char), FileLength, fp);
		bytes_read = fread(FileBuffer, 1, FileLength, fp);
		if(FileLength != bytes_read)
		{
			printf("Error reading file. %d of %d read.\r\n", bytes_read, FileLength);
			return(0);
		}
		else
		{
			//FileLength+=635;
			//FileBuffer[++FileLength] = '\0';
		}
		fclose(fp);
	}
	
	//DumbBufferToHex(FileBuffer, FileLength);
	
	ProcessGifFile();
	
	//PrintFrames(&Frames);
	
	OutputToArray(Filename, &Frames);
	
	return(0);
}

//====================================================================================
void dprintf(int debug_level, char *fmt, ...)
{
	if(DebugSetting >= debug_level)
	{
		va_list arguments;	// A place to store our list of arguments
		va_start(arguments, fmt);	// Initialise arguments
		vprintf(fmt, arguments);
		va_end(arguments);			// Clean up after use
	}
}

//====================================================================================
void OutputHelp(void)
{
	printf("ImgToArray requires an image file as an argument\r\n");
	printf("For example:\r\n");
	printf("\t\"ImgToArray MyPicture.gif\"");
}

//====================================================================================
void DumbBufferToHex(unsigned char *buffer, int length)
{
	int count, line_count = 0;
	
	dprintf(3, "%2.2X): ", line_count);
	
	for(count = 1; count <= length; count++)
	{
		dprintf(3, "%2.2X ", (unsigned char)*buffer++);
		if((count % 16) == 0)
		{
			line_count++;
			if(count < length)
			{
				dprintf(3, "\r\n");
				dprintf(3, "%2.2X): ", line_count);
			}
		}
	}
	
	dprintf(3, "\r\n");
}

//====================================================================================
void DumpImg(unsigned char *buffer, int length, int width)
{
	int count, line_width = 0;
	
	for(count = 1; count <= length; count++)
	{
		if(line_width++ >= width)
		{
			line_width = 1;
			dprintf(3, "\r\n");
		}
	
		if((unsigned char)*buffer++ == 0x01)
		{
			dprintf(3, " ");
		}
		else
		{
			dprintf(3, "*");
		}
	}
	dprintf(3, "\r\n");
}

//====================================================================================
void BumpColourMap(rgb *colour_map, int colour_count)
{
	int i;
	for(i = 0; i < colour_count; i++)
	{
		dprintf(2, "Colour %d: R %2.2X, G %2.2X, B %2.2X\r\n", i, colour_map->r, colour_map->g, colour_map->b);
		colour_map++;
	}
}

//====================================================================================
int ProcessGifFile(void)
{
	char temp[256];
	int i, img_width, img_height, img_field, img_background, img_aspect;
	int global_color_table_size, color_resolution_bits;  // number of entries in global_color_table
	rgb *global_color_table;
	rgb *colour_ptr;
	unsigned char *file_ptr;
	unsigned char block_type = 0x0;

	// Output header info
	dprintf(2, "GIF file inforamtion\r\n");	// Output file ID tag
	temp[0] = FileBuffer[0];
	temp[1] = FileBuffer[1];
	temp[2] = FileBuffer[2];
	temp[3] = 0x00;
	dprintf(2, "File ID:\t%s\r\n", temp);	// Output file ID tag

	temp[0] = FileBuffer[3];
	temp[1] = FileBuffer[4];
	temp[2] = FileBuffer[5];
	temp[3] = 0x00;
	dprintf(2, "File version:\t%s\r\n", temp);	// Output file version

	img_width = (FileBuffer[7] << 8) + FileBuffer[6];
	dprintf(2, "image width:\t%d\r\n", img_width);	// Output image width
	img_height = (FileBuffer[9] << 8) + FileBuffer[8];
	dprintf(2, "image height:\t%d\r\n", img_height);	// Output image height
	
	img_field = FileBuffer[10];
	global_color_table_size = ((img_field & 0x70) >> 4) + 1;
	dprintf(2, "Field:\t\t0x%X\r\n", img_field);
	if(img_field & 0x80)
	{
		dprintf(2, "\tGlobal colour table true\r\n");
	}
	else
	{
		dprintf(2, "\tGlobal colour table false\r\n");	
	}
	dprintf(2, "\tBits in global colour table: %d\r\n", (img_field & 0x03));
	dprintf(2, "\tColour colour table size:\t%d\r\n", global_color_table_size);
	
	img_background = FileBuffer[11];
	dprintf(2, "Background:\t%d\r\n", img_background);
	img_aspect = FileBuffer[12];
	dprintf(2, "Pixel Aspect:\t%d\r\n", img_aspect);
	
	// First three byts are file type
	if(memcmp(FileBuffer, "GIF", 3) != 0)
	{
		printf("Error in file header, incorrect type\r\n");
		return(0);
	}
	
	// Byte 3-5 are version info, we only support 89a
	if(memcmp(&FileBuffer[3], "89a", 3) != 0)
	{
		printf("Error in file header, incorrect file version\r\n");
		return(0);
	}

	// Read the global colour table
	global_color_table = (rgb *)malloc(3 * global_color_table_size);
	colour_ptr = global_color_table;
	file_ptr = &FileBuffer[13];
	for(i = 0; i < global_color_table_size; i++)
	{
		memcpy(colour_ptr, file_ptr, 3);
		file_ptr += 3;
		colour_ptr++;
	}
	dprintf(2, "Global colour table:\r\n");
	BumpColourMap(global_color_table, global_color_table_size);
	
	// TODO: fix this!
	while(*file_ptr != IMAGE_DESCRIPTOR)
	{
		//printf("T = %2.2X\r\n",*file_ptr); 
		file_ptr++;
	}
	
	while(block_type != TRAILER)
	{
		block_type = *file_ptr++;
		printf("block_type: %2.2X\r\n", block_type);
		switch(block_type)
		{
			case IMAGE_DESCRIPTOR:
				if(!process_image_descriptor(&file_ptr, global_color_table, 
					global_color_table_size, color_resolution_bits))
				{
					printf("Error processing image descriptor\r\n");
					return(0);
				}
				break;
			case EXTENSION_INTRODUCER:
				if(!process_extension(&file_ptr))
				{
					return(0);
				}
				break;
			case TRAILER:
				dprintf(2, "Trailing block type found");
				break;
			default:
				printf("Error: unrecognized block type %.02x\n", block_type);
				return(0);
		}
	}
	
	return(0);
}

//====================================================================================
int process_image_descriptor(unsigned char **file, rgb *gct, int gct_size, int resolution_bits)
{
	image_descriptor_t image_descriptor;
	int disposition;
	unsigned char lzw_code_size;
	int compressed_data_length;	
	unsigned char *compressed_data = NULL;
	int uncompressed_data_length = 0;
	unsigned char *uncompressed_data = NULL;
	
	int i;
	unsigned char *ptr;

	memcpy(&image_descriptor, *file, 9);
	*file += 9;

	// TODO if LCT = true, read the LCT

	disposition = 1;

	/*if ( read( gif_file, &lzw_code_size, 1 ) < 1 )
	{
		perror( "Invalid GIF file (too short) [8]: " );
		disposition = 0;
		goto done;
	}*/
	lzw_code_size = **file;
	*file += 1;
	printf("lzw_code_size: %d\r\n", lzw_code_size);

	compressed_data_length = read_sub_blocks(file, &compressed_data);
	if(compressed_data_length == -1)
	{
		printf("Error reading sub blocks!\r\n");
	}
	 
	uncompressed_data_length = image_descriptor.image_width * image_descriptor.image_height;
  
	uncompressed_data = (unsigned char*)malloc( uncompressed_data_length );

	uncompress( lzw_code_size, compressed_data, compressed_data_length, uncompressed_data );

	printf("compressed_data_length: %d\r\n", compressed_data_length);
	printf("uncompressed_data_length: %d\r\n", uncompressed_data_length);
	printf("Dump of uncompressed data\r\n");
	//DumpImg(uncompressed_data, uncompressed_data_length, image_descriptor.image_width);
	//DumbBufferToHex(uncompressed_data, uncompressed_data_length);
	StoreFrame(uncompressed_data, uncompressed_data_length, image_descriptor.image_width, image_descriptor.image_height);
	
	printf("looking for stop code\r\n");
	ptr = compressed_data;
	for(i = 0; i < compressed_data_length; i++)
	{
		if(*ptr == 0x05)
		{
			printf("StopCode found at: %\r\n", i);
		}
	}
	
done:
  if ( compressed_data )
    free( compressed_data );

  if ( uncompressed_data )
    free( uncompressed_data );

  return disposition;
}

//====================================================================================
static int read_sub_blocks(unsigned char **gif_file, unsigned char **data )
{
  int data_length;
  int index;
  unsigned char block_size, data_read;

  // Everything following are data sub-blocks, until a 0-sized block is
  // encountered.
  data_length = 0;
  *data = NULL;
  index = 0;

  while(1)
  {	
	block_size = **gif_file;
	*gif_file += 1;

    if(block_size == 0)  // end of sub-blocks
    {
		printf("End of sub blocks\n\r");
		break;
    }
	
	printf("block_size: %d\r\n", block_size);

    data_length += block_size;
    *data = (unsigned char*)realloc( *data, data_length );

    // TODO this could be split across block size boundaries
	
	memcpy(*data + index, *gif_file, block_size);
	*gif_file += block_size;
	
	/*data_read = read( gif_file, *data + index, block_size );
    if (data_read < block_size )
    {
		printf("block_size read: %d of %d\r\n", data_read, block_size);
      perror( "Invalid GIF file (too short) [10]: ");
      //return -1;
    }*/

    index += block_size;
  }
	printf("data_length: %d\r\n", data_length);
  return data_length;
}


//====================================================================================
int uncompress( int code_length,
                const unsigned char *input,
                int input_length,
                unsigned char *out )
{
  int maxbits;
  int i, bit;
  int code, prev = -1;
  dictionary_entry_t *dictionary;
  int dictionary_ind;
  unsigned int mask = 0x01;
  int reset_code_length;
  int clear_code; // This varies depending on code_length
  int stop_code;  // one more than clear code
  int match_len;

  clear_code = 1 << ( code_length );
  stop_code = clear_code + 1;
  // To handle clear codes
  reset_code_length = code_length;

  // Create a dictionary large enough to hold "code_length" entries.
  // Once the dictionary overflows, code_length increases
  dictionary = ( dictionary_entry_t * ) 
    malloc( sizeof( dictionary_entry_t ) * ( 1 << ( code_length + 1 ) ) );

  // Initialize the first 2^code_len entries of the dictionary with their
  // indices.  The rest of the entries will be built up dynamically.

  // Technically, it shouldn't be necessary to initialize the
  // dictionary.  The spec says that the encoder "should output a
  // clear code as the first code in the image data stream".  It doesn't
  // say must, though...
  for ( dictionary_ind = 0; 
        dictionary_ind < ( 1 << code_length ); 
        dictionary_ind++ )
  {
    dictionary[ dictionary_ind ].byte = dictionary_ind;
    // XXX this only works because prev is a 32-bit int (> 12 bits)
    dictionary[ dictionary_ind ].prev = -1;
    dictionary[ dictionary_ind ].len = 1;
  }

  // 2^code_len + 1 is the special "end" code; don't give it an entry here
  dictionary_ind++;
  dictionary_ind++;
  
  // TODO verify that the very last byte is clear_code + 1
  while ( input_length )
  {
    code = 0x0;
    // Always read one more bit than the code length
    for ( i = 0; i < ( code_length + 1 ); i++ )
    {
      // This is different than in the file read example; that 
      // was a call to "next_bit"
      bit = ( *input & mask ) ? 1 : 0;
      mask <<= 1;

      if ( mask == 0x100 )
      {
        mask = 0x01;
        input++;
        input_length--;
      }

      code = code | ( bit << i );
    }

    if ( code == clear_code )
    {
      code_length = reset_code_length;
      dictionary = ( dictionary_entry_t * ) realloc( dictionary,
        sizeof( dictionary_entry_t ) * ( 1 << ( code_length + 1 ) ) );

      for ( dictionary_ind = 0; 
            dictionary_ind < ( 1 << code_length ); 
            dictionary_ind++ )
      {
        dictionary[ dictionary_ind ].byte = dictionary_ind;
        // XXX this only works because prev is a 32-bit int (> 12 bits)
        dictionary[ dictionary_ind ].prev = -1;
        dictionary[ dictionary_ind ].len = 1;
      }
      dictionary_ind++;
      dictionary_ind++;
      prev = -1;
      continue;
    }
    else if ( code == stop_code )
    {
      if ( input_length > 1 )
      {
		printf("stop_code = 0x%2.2X, input_length = %d\r\n", stop_code, input_length);
        fprintf( stderr, "Malformed GIF (early stop code)\n" );
        //exit( 0 );
      }
      break;
    }

    // Update the dictionary with this character plus the _entry_
    // (character or string) that came before it
    if ( ( prev > -1 ) && ( code_length < 12 ) )
    {
      if ( code > dictionary_ind )
      {
        fprintf( stderr, "code = %.02x, but dictionary_ind = %.02x\n",
          code, dictionary_ind );
        exit( 0 );
      }

      // Special handling for KwKwK
      if ( code == dictionary_ind )
      {
        int ptr = prev;

        while ( dictionary[ ptr ].prev != -1 )
        {
          ptr = dictionary[ ptr ].prev;
        }
        dictionary[ dictionary_ind ].byte = dictionary[ ptr ].byte;
      }
      else
      {
        int ptr = code;
        while ( dictionary[ ptr ].prev != -1 )
        {
          ptr = dictionary[ ptr ].prev;
        }
        dictionary[ dictionary_ind ].byte = dictionary[ ptr ].byte;
      }

      dictionary[ dictionary_ind ].prev = prev;

      dictionary[ dictionary_ind ].len = dictionary[ prev ].len + 1;

      dictionary_ind++;

      // GIF89a mandates that this stops at 12 bits
      if ( ( dictionary_ind == ( 1 << ( code_length + 1 ) ) ) &&
           ( code_length < 11 ) )
      {
        code_length++;

        dictionary = ( dictionary_entry_t * ) realloc( dictionary,
          sizeof( dictionary_entry_t ) * ( 1 << ( code_length + 1 ) ) );
      }
    }

    prev = code;

    // Now copy the dictionary entry backwards into "out"
    match_len = dictionary[ code ].len;
    while ( code != -1 )
    {
      out[ dictionary[ code ].len - 1 ] = dictionary[ code ].byte;
      if ( dictionary[ code ].prev == code )
      {
        fprintf( stderr, "Internal error; self-reference." );
        exit( 0 );
      }
      code = dictionary[ code ].prev;
    }

    out += match_len;
  }
}


//====================================================================================
static int process_extension(unsigned char **gif_file)
{
  extension_t extension;
  graphic_control_extension_t gce;
  application_extension_t application;
  plaintext_extension_t plaintext;
  unsigned char *extension_data = NULL;
  int extension_data_length;

  /*if ( read( gif_file, &extension, 2 ) < 2 )
  {
    perror( "Invalid GIF file (too short) [7]: " );
    return 0;
  }*/
  
  memcpy(&extension, *gif_file, 2);
  *gif_file += 2;
  
  printf("In process_extension\r\n");

  switch ( extension.extension_code )
  {
    case GRAPHIC_CONTROL:
      /*if ( read( gif_file, &gce, 4 ) < 4 )
      {
        perror( "Invalid GIF file (too short) [6]: " );
        return 0;
      }*/
		printf("GRAPHIC_CONTROL found\r\n");
		memcpy(&gce, *gif_file, 4);
		*gif_file += 4;

      break;
    case APPLICATION_EXTENSION:
      /*if ( read( gif_file, &application, 11 ) < 11 )
      {
        perror( "Invalid GIF file (too short) [5]: " );
        return 0;
      }*/
		printf("APPLICATION_EXTENSION found\r\n");
		memcpy(&application, *gif_file, 11);
		*gif_file += 11;
      break;
    case 0xFE:
      // comment extension; do nothing - all the data is in the
      // sub-blocks that follow.
      break;
    case 0x01:
      /*if ( read( gif_file, &plaintext, 12 ) < 12 )
      {
        perror( "Invalid GIF file (too short) [4]: " );
        return 0;
      }*/
		printf("plaintext found\r\n");
		memcpy(&plaintext, *gif_file, 12);
		*gif_file += 12;
      break;
    default:
      fprintf( stderr, "Unrecognized extension code.\n" );
      exit( 0 );
  }

  // All extensions are followed by data sub-blocks; even if it's
  // just a single data sub-block of length 0
  extension_data_length = read_sub_blocks( gif_file, &extension_data );

  if ( extension_data != NULL )
    free( extension_data );

  return 1;
}

//====================================================================================
void StoreFrame(const unsigned char *data, int length, int width, int height)
{
	frame_t frame;
	int i;
	
	for(i = 0; i < length; i++)
	{
		frame.data.push_back(*data++);
	}
	
	Frames.frames.push_back(frame);
	Frames.width = width;
	Frames.height = height;
}

//====================================================================================
void PrintFrames(frame_store_t *frames)
{
	unsigned char *data, *data_ptr;
	unsigned short line_width = 1;
	unsigned short repeat = 10;
	
	vector<frame_t>::iterator frames_iter = frames->frames.begin();
	
	data = (unsigned char*)malloc((*frames_iter).data.size() + (frames->height * 2) + 1);
	
	while(repeat-- > 0)
	{
		for(frames_iter = frames->frames.begin(); frames_iter != frames->frames.end(); frames_iter++)
		{
		
			data_ptr = data;
			
			vector<unsigned char>::iterator data_iter;
			for(data_iter = (*frames_iter).data.begin(); data_iter != (*frames_iter).data.end(); data_iter++)
			{
				if(line_width++ >= frames->width)
				{
					line_width = 1;
					*data_ptr++ = '\r';
					*data_ptr++ = '\n';
				}
			
				//*data_ptr++ = *data_iter;
				if(*data_iter == 0x01)
				{
					*data_ptr++ = ' ';
				}
				else
				{
					*data_ptr++ = '#';
				}
			}
			
			*data_ptr = 0;
			
			printf("%s", data);
			
			Sleep(250);
		}
	}
	free(data);
}

//====================================================================================
void OutputToArray(char *filename, frame_store_t *frames)
{
	char new_filename[256], h_def_filename[256], array_name[256];
	int i, j, frame_number, col_count, pixel_count;
	char *input_ptr, *output_ptr;
	unsigned char data_byte;
	
	strcpy(new_filename, filename);
	new_filename[strlen(filename) - 3] = 'h';
	new_filename[strlen(filename) - 2] = 0x00;
	
	input_ptr = strrchr(new_filename, '/') + 1;
	output_ptr = h_def_filename;
	while(*input_ptr != 0)
	{
		if(*input_ptr == '.')
		{
			*output_ptr++ = '_';
			*output_ptr++ = 'H';
			*output_ptr++ = 0x00;
			break;
		}
		
		if(*input_ptr == ' ')
		{
			*output_ptr++ = '_';
		}
		else
		{
			if((*input_ptr >= 'a') && (*input_ptr <= 'z'))
			{
				*output_ptr++ = *input_ptr - 0x20;
			}
			else
			{
				*output_ptr++ = *input_ptr;
			}
		}
		input_ptr++;
	}
	*output_ptr = 0x00;
	
	strcpy(array_name, h_def_filename);
	array_name[strlen(array_name) - 2] = 0x00;
	
	FILE * output;
    output = fopen(new_filename, "w");
	fprintf(output, "#ifndef __%s__\n", h_def_filename);
	fprintf(output, "#define __%s__\n\n", h_def_filename);

	vector<frame_t>::iterator frames_iter;
	frame_number = 0;
	col_count = 0;
	for(frames_iter = frames->frames.begin(); frames_iter != frames->frames.end(); frames_iter++)
	{
		if(frames->frames.size() == 1)
		{
			fprintf(output, "unsigned char %s[]=\n{", array_name);
			pixel_count = 0;
			while(pixel_count <(frames->width * frames->height))
			{
				data_byte = 0;
				for(i = 0; i < 8; i++)
				{
					if((*frames_iter).data[pixel_count + (i * frames->width)] != 0x01)
					{
						data_byte |= (0x01 << i);
					}
				}
				
				pixel_count++;
				if((pixel_count % frames->width) == 0)
				{
					pixel_count += 7 * frames->width;
				}
				
				if((col_count++ % 16) == 0)
				{
					fprintf(output, "\n\t");
				}
				fprintf(output, "0x%2.2X, ", data_byte);
			}
		}
		else
		{
			fprintf(output, "unsigned char %s%d[]=\n{", array_name, frame_number);
			pixel_count = 0;
			while(pixel_count <(frames->width * frames->height))
			{
				data_byte = 0;
				for(i = 0; i < 8; i++)
				{
					if((*frames_iter).data[pixel_count + (i * frames->width)] != 0x01)
					{
						data_byte |= (0x01 << i);
					}
				}
				
				pixel_count++;
				if((pixel_count % frames->width) == 0)
				{
					pixel_count += 7 * frames->width;
				}
				
				if((col_count++ % 16) == 0)
				{
					fprintf(output, "\n\t");
				}
				fprintf(output, "0x%2.2X, ", data_byte);
			}
		}
		fprintf(output, "\n};\n\n");
		frame_number++;
	}
	fprintf(output, "#endif /*__%s__*/\n", h_def_filename);
    fclose(output);
}
