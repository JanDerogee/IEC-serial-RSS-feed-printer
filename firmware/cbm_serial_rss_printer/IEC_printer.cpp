#include <Arduino.h>
#include "IEC_serial.h"
#include "IEC_printer.h"
#include "graphics.h"

/*-----------------------------------*/



/*------------------------------------*/

/*We have this need because a modern programming evironment uses ASCII*/
/*However, the old hardware expects us to send PETSCII, which makes sense for a Brother HR-5C*/
/*which is a model released for the Commodore line of computers, which use PETSCII*/
/*The biggest difference between PETSCII and ASCII is the switched case situation, which is easy to fix*/
unsigned char cbm_switch_case(char data)
{
  unsigned char new_data;
  
  if (data >= 0x41 && data <= 0x5A)       /*Convert from lower to upper (a->A)*/
  {
    new_data = data + 32;
  }
  else if (data >= 0x61 && data <= 0x7A)  /*Convert from upper to lower (A->a)*/
  { 
    new_data = data - 32;
  }
  else
  { 
    new_data = data;                      /*Not an alpha value, no conversion needed*/
  }

  return(new_data);
}

/*Select mode to print, this is to be send before sending text*/
unsigned char cbm_printmode(unsigned char printmode)
{
  unsigned char error;

  error = IEC_write_frame(printmode, 0);
  if(error > 0) {return(error);} 
}


/*Prints text with a C64 compatible printer*/
unsigned char cbm_print(char* text)
{
  unsigned char error;
  unsigned int i;

  /*prevent printing an empty string*/
  if(text[0] == 0)
  {
    return(0);  /*no error detected*/      
  }

  /*print all text except the last character*/
  i=0;
  while(text[i+1] != 0)
  {
    yield();  /*pet the watchdog*/    
    error = IEC_write_frame(cbm_switch_case(text[i]), 0);
    if(error > 0) {return(error);}
    i++;
  }

  /*print the last character of the text*/
  error = IEC_write_frame(cbm_switch_case(text[i]), 1); /*print the last character with EOI*/
  if(error > 0) {return(error);}
  
  return(0);  /*no error detected*/
}

/*Prints a line of text with a CBM-64 compatible printer*/
/*example:
 * 
  char buf[64] = "test\0";  //make the buffer as large as the text you want to print
  cbm_println(buf);
 * 
 */
unsigned char cbm_println(char* text)
{
  unsigned char error;  
  unsigned int i;

 /*prevent printing an empty string*/
  if(text[0] == 0)
  {
    return(0);  /*no error detected*/      
  }

  /*print all text*/
  i=0;
  while(text[i] != 0)
  {
    yield();  /*pet the watchdog*/
    error = IEC_write_frame(cbm_switch_case(text[i]), 0);
    if(error > 0) {return(error);}    

    i++;    
  }

  /*print a CR*/
  error = IEC_write_frame(13, 1); /*new line (signal that this is the last frame by raising the EOI flag)*/
  if(error > 0) {return(error);}    

  return(0);  /*no error detected*/
}

unsigned char cbm_newline(void)
{
  unsigned char error;  
    
  /*print a CR*/
  error = IEC_write_frame(13, 0); /*new line*/
  if(error > 0) {return(error);}    

  return(0);  /*no error detected*/  
}

void cbm_print_graphics(void)
{
  const unsigned int height = 259;
  const unsigned int width = 157;
  
  unsigned int i, j;
  unsigned int p;  

  IEC_write_frame(8,0); /*set printer into graphical (bitmap) mode*/
  for(j=0;j<(height/7);j++)  /*height of the image*/
  {   
    for(i=0;i<width;i++)    /*width of the image*/
    {
      yield();  /*pet the watchdog*/      
      p = i+(j*width);
      IEC_write_frame((pgm_read_byte(img_test+p)) | 128, 0);  /*send 7-bits of the image data byte, do not raise EOI flag*/
    }
    IEC_write_frame(13, 0); /*new line (signal that this is the last frame by raising the EOI flag)*/    
  }  
}

void cbm_print_YouTube_graphics(void)
{
  const unsigned int height = 112;
  const unsigned int width = 480;
  unsigned int i, j;
  unsigned int p;  

  IEC_write_frame(8,0); /*set printer into graphical (bitmap) mode*/
  for(j=0;j<(height/7);j++)  /*height of the image*/
  {   
    for(i=0;i<width;i++)    /*width of the image*/
    {
      yield();  /*pet the watchdog*/      
      p = i+(j*width);
      IEC_write_frame((pgm_read_byte(img_YT_subscribe+p)) | 128, 0);  /*send 7-bits of the image data byte, do not raise EOI flag*/
    }
    IEC_write_frame(13, 0); /*new line (signal that this is the last frame by raising the EOI flag)*/    
  }  
}


/*This routine uses a charset that is NOT in the printer!!!*/
/*In other words, we use our own charset, which basically means*/
/*that we are printing text using bitmap graphics mode*/
void cbm_print_UTF8(char* text, unsigned char printmode)
{
  unsigned char error;
  unsigned int i, j, p;
  unsigned char data;
  unsigned char n;

  const unsigned char italic_bufsize = 7;
  
  unsigned char italic_buff[italic_bufsize];

  /*prevent printing an empty string*/
  if(text[0] == 0)
  {
    return;  /*no error detected*/      
  }

  /*print all text*/
  i=0;
  IEC_write_frame(8,0); /*set printer into graphical (bitmap) mode*/  
  while(text[i] != 0)
  {
    yield();  /*pet the watchdog*/    

    if(text[i] == '\n')  /*detect the new-line character*/
    {
      IEC_write_frame(15,0);  /*set printer into normal text mode*/       
      IEC_write_frame(13,0);  /*send character return*/
      IEC_write_frame(8,0);   /*set printer into graphical (bitmap) mode*/

      /*erase italic buffer*/
      for(n=0; n<italic_bufsize, n++;)
      {
        italic_buff[n] = 0;
      }
    }
    else
    {
      for(j=0;j<6;j++)
      {        
        if(j<5)
        {
          p = (text[i]*5)+j;
          data = pgm_read_byte(UTF8_5x7_font+p);
          data = data >> 1; /*shift charset one bit up to match the printers alignment*/
        }
        else
        {
          data = 0x00;  /*between each character is an empty pixel*/
        }

        /*in underline mode, just add a line to what needs to be printed*/
        if((printmode & UTF8_UNDERLINE) > 0)
        {
          data = data | 0x40;          
        }

        /*in inverse mode, just flip all the bits to print negative*/
        if((printmode & UTF8_INVERSE) > 0)
        {
          data = data ^ 0x7F;
        }        

        /*when italic mode is active, the data is partially shifted/delayed*/
        if((printmode & UTF8_ITALIC) > 0)
        {
            /*45 deg Italic*/
            italic_buff[0] = (data & 0x40) | italic_buff[1];          
            italic_buff[1] = (data & 0x20) | italic_buff[2];
            italic_buff[2] = (data & 0x10) | italic_buff[3];
            italic_buff[3] = (data & 0x08) | italic_buff[4];
            italic_buff[4] = (data & 0x04) | italic_buff[5];            
            italic_buff[5] = (data & 0x02) | italic_buff[6];            
            italic_buff[6] = (data & 0x01);
            data = italic_buff[0];

            /*22deg Italic*/
            //italic_buff[0] = (data & 0x40) | italic_buff[1];
            //italic_buff[1] = (data & 0x30) | italic_buff[2];            
            //italic_buff[2] = (data & 0x0C) | italic_buff[3];            
            //italic_buff[3] = (data & 0x03);
            //data = italic_buff[0];            
        }
  
        IEC_write_frame(data | 128, 0);  /*send 7-bits of the data byte, do not raise EOI flag*/
        if((printmode & UTF8_DOUBLEWIDTH) > 0)
        {
          IEC_write_frame(data | 128, 0);  /*send 7-bits of the data byte, do not raise EOI flag*/                  
        }
        
      }
    }
    i++;   /*increment loop counter*/ 
  }
  IEC_write_frame(15,0); /*set printer into normal text mode*/
}
