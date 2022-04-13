#ifndef _IEC_SERIAL_H_
#define _IEC_SERIAL_H_


//#define     BLABLA                0

/* 
  pinnout CBM-64 serial port:
  ---------------------------
  1 =  n/c
  2  = GND
  3  = IEC_ATN_OUT
  4  = IEC_CLK_OUT
  5  = IEC_DATA_OUT & IEC_DATA_IN
  6  = IEC_RESET_OUT
*/


/*Mapping of CBM-64's serial port lines to ESP8266 digital I/O pins
 *because the ESP8266 is 3.3V and the IEC printer is 5V and opencollector
 *signalling is required, transistors are used for connecting it all together
 *The only bi-directional dataline is the dataline itself, this keeps things easy.
 */
/*With Brother HR-5C, we need to be able to read only the data line*/
#define IEC_RESET     13
#define IEC_ATN       12
#define IEC_CLK       14
#define IEC_DATA_OUT  4
#define IEC_DATA_IN   5

/*Brother HR-5C printer modes (graphic|business, different char set)*/
//#define CHARSET_PETSCII    0
//#define CHARSET_ASCII      7

/*------------------------------------------*/
/*IEC IO-related macros in order to move the IO correctly*/

#define IEC_RST_release     digitalWrite(IEC_RESET, LOW);
#define IEC_RST_pulldown    digitalWrite(IEC_RESET, HIGH);

#define IEC_CLK_release     digitalWrite(IEC_CLK, LOW);
#define IEC_CLK_pulldown    digitalWrite(IEC_CLK, HIGH);

#define IEC_ATN_release     digitalWrite(IEC_ATN, LOW);
#define IEC_ATN_pulldown    digitalWrite(IEC_ATN, HIGH);

#define IEC_DAT_release     digitalWrite(IEC_DATA_OUT, LOW);
#define IEC_DAT_pulldown    digitalWrite(IEC_DATA_OUT, HIGH);

/*------------------------------------------*/

//void supportfunction(unsigned char value);

/*--------------------------------------------------------*/

void IEC_init(void);
void IEC_reset(void);
unsigned char IEC_attention(boolean attention);
unsigned char IEC_open(unsigned char device_number, unsigned char sec_addr);
unsigned char IEC_close(unsigned char device_number, unsigned char sec_addr);
unsigned char IEC_device_ready(void);
unsigned char IEC_write_frame(unsigned char data, unsigned char last_frame);


#endif
