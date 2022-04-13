#include <Arduino.h>
#include "IEC_serial.h"

/*--------------------------------------------------------*/

void IEC_write_bit(unsigned char data);
void IEC_write_byte(unsigned char data);

/*--------------------------------------------------------*/

/*this routine will initialize the IEC-port*/
void IEC_init(void)
{
  pinMode(IEC_RESET, OUTPUT);
  pinMode(IEC_CLK, OUTPUT);
  pinMode(IEC_CLK, OUTPUT);
  pinMode(IEC_ATN, OUTPUT);
  pinMode(IEC_DATA_OUT, OUTPUT); 
  pinMode(IEC_DATA_IN, INPUT);  
  
  IEC_RST_release;
  IEC_ATN_release;
  IEC_CLK_release;
  IEC_DAT_release;

  IEC_reset();   /*Reset all devices connected to the IEC-bus*/  
}

/*Resets all devices on the IEC-bus*/
void IEC_reset(void)
{
  IEC_RST_pulldown;
  delay(100);                           /*duration of reset pulse*/
  IEC_RST_release;
  delay(3000);                          /*time to allow the device(s) to come recover from reset*/
}

/*attention = TRUIE create an attention situation*/
/*if no device responds by pulling data low, then we have device not present*/
/*attention = FALSE drop attention*/
unsigned char IEC_attention(boolean attention)
{
  unsigned char lp;
  unsigned char error;

  if(attention == true)
  {    
    IEC_ATN_pulldown;
    delayMicroseconds(2000); /*wait for 2 ms*/
    IEC_CLK_pulldown
    delayMicroseconds(2000); /*wait for 2 ms*/
  
    lp=0;  /*if the data line doesn't go low within 1000us, then we have a "device not present" situation*/
    {
      while(IEC_device_ready())     /*Device sets DATA low when it begins processing the data (busy)*/
      {
        delayMicroseconds(10);
        lp++;
        if(lp>100)
        {
            Serial.println("Error 1: device not present");
            return(1);  /*no listener did respond*/
        }
      }      
    }  
  }
  else
  {
    IEC_ATN_release;
    delayMicroseconds(100);    /*allow some time between dropping attention and releasing the clock*/
    IEC_CLK_release;  
  }
  
  return(0);  /*0=OK, everything went perfectly fine*/
}

/*Open communication with a device on the IEC-bus*/
unsigned char IEC_open(unsigned char device_number, unsigned char sec_addr)
{
  unsigned char error;
  error = IEC_attention(true);
  if(error > 0) {return(error);}    /*if error, exit with the error code*/
  
  error = IEC_write_frame(0x20 + device_number, 0);   /*Write primary (the device we want to communicate with) address*/  
  if(error > 0) {return(error);}    /*if error, exit with the error code*/

  /*By opening channel 7, we put the printer in the Administrative characterset mode*/
  /*This allows to print in the upper and lower case characters*/
  error = IEC_write_frame(0x60 + sec_addr, 0);  /*Write secondary address*/
  if(error > 0) {return(error);}    /*if error, exit with the error code*/  

  IEC_attention(false);
  delay(100); /*add a delay, so that the listener has time to process the open*/
  return(0);  /*everything went fine, no error detected*/
}


/*Close communication with a device on the IEC-bus*/
unsigned char IEC_close(unsigned char device_number, unsigned char sec_addr)
{
  unsigned char error;

  error = IEC_attention(true);
  if(error > 0) {return(error);}    /*if error, exit with the error code*/   

// The code below isn't send by the C64 either, so why should we
//  error = IEC_write_frame(0xE0 + sec_addr, 0);  /*Write secondary address*/ 
//  if(error > 0) {return(error);}    /*if error, exit with the error code*/ 
  
  error = IEC_write_frame(0x3F, 0); /*Write unlisten command*/
  if(error > 0) {return(error);}    /*if error, exit with the error code*/ 
 
  IEC_attention(false);
  delay(100); /*add a delay, so that the listener has time to process the close*/                  
  return(0);  /*everything went fine, no error detected*/
}

/*Request the status of the currently opened IEC device*/
/*0 = device is busy*/
/*1 = device is ready*/
unsigned char IEC_device_ready(void)
{
  unsigned char data;

  data = digitalRead(IEC_DATA_IN);
  
  return(data);
}

/*Writes one bit (data byte's LSB) to CBM serial line*/
void IEC_write_bit(unsigned char data)
{
  if(data==1)
  {
    IEC_DAT_release;         
  }
  else
  {
    IEC_DAT_pulldown;      
  }
  delayMicroseconds(100);  /*allow some time for the signal to stabilize before changing the clock*/
  IEC_CLK_release;
  delayMicroseconds(60);
  IEC_CLK_pulldown;
  IEC_DAT_release;        /*this doesn't really make sense, but since the C64 does it, we doe this too*/
  delayMicroseconds(40);  
}

/*Writes one byte to CBM serial line (LSB first, MSB last)*/
void IEC_write_byte(unsigned char data)
{
  unsigned char lp;
  for(lp=0;lp<8;lp++) /*send the data, LSB is send first*/
  {
    IEC_write_bit(data & 0x01); 
    data = data >>1;
  }
}

/*Writes one data frame to CBM serial line*/
unsigned char IEC_write_frame(unsigned char data, unsigned char last_frame)
{  
  unsigned char lp;

  //Serial.print("*");  
  //Serial.print(data, HEX);  
  //Serial.print("*");  
  
  IEC_CLK_release;
  while(!IEC_device_ready())    /*Device sets DATA high when ready to receive data (not busy)*/
  {
    delayMicroseconds(10);
    yield();  /*pet the watchdog*/
  }

  //if(last_frame == 1)              /*Last frame signalling to listener*/
  if(last_frame == 255) /*debug only, 255 will never be sent, so we'll never EOI*/
  {
    
    /*sender generates EOI*/
    delayMicroseconds(250);         /*<250us, this IS the last byte (EOI situation)*/

    /*listener should respond to EOI with an ack*/
    /*and will pull down the dataline for at least 60us*/
    while(IEC_device_ready())    /*Device sets DATA high when ready to receive data (not busy)*/
    {
      delayMicroseconds(10);
      /*DO NOT pet the watchdog, as this might result in taking too long creating all sorts of problems, listener responds within in time anyway, so we won't be long and the dogg stays happy*/      
    }

    while(!IEC_device_ready())    /*Device sets DATA high when ready to receive data (not busy)*/
    {
      delayMicroseconds(10);
      /*DO NOT pet the watchdog, as this might result in taking too long creating all sorts of problems, listener responds within in time anyway, so we won't be long and the dogg stays happy*/      
    }
    
    IEC_CLK_pulldown;               /*pull clock down to signal the sender is ready too*/   
  }
  else
  {
    IEC_CLK_pulldown;           /*pull clock down to signal the sender is ready too (no EOI)*/       
  }
  delayMicroseconds(40);        

  IEC_write_byte(data);         /*Send data byte to the listener*/

  delayMicroseconds(20);
   
  lp=0;  /*if the listerner doesn't respond with "busy" (a pulled down data line) within 1000us, there is a problem*/
  {
    while(IEC_device_ready())     /*Device sets DATA low when it begins processing the data (busy)*/
    {
      delayMicroseconds(10);
      yield();  /*pet the watchdog*/      
      lp++;
      if(lp>100)
      {
          Serial.println("Timeout: listener did not acknowledge");
          return(1);  /*Timeout: listener did not acknowledge*/
      }
    }      
  }  

  delayMicroseconds(100);         /*Delay between frames*/
  return(0);  /*everything went fine, no error detected*/  
}
