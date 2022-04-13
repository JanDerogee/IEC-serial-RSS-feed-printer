/*
   Created on: somewhere around march 2022

   Blabla... if you use it... bla bla bla... at your own risk.
   Blabla... if you use it for commercial purposes feel free to share some of the profit with me...
   Blabla... if you use it as a basis for your own project, please mention me in your credits.

   Jan Derogee,
   April 2022

   The IEC serial related parts of this project are heavily based upon the work of Tapani Rantakokko (tapani@rantakokko.net)
*/

/*--------------------------------------------------------------*/

#include <Arduino.h>
#include "IEC_serial.h"
#include "IEC_printer.h"

#include <ESP8266WiFi.h>        /*The ESP8266WiFi library provides a wide collection of C++ methods (functions) and properties to configure and operate an ESP8266 module in station and / or soft access point mode.*/
#include <ESP8266WiFiMulti.h>   /*ESP8266WiFiMulti. h can be used to connect to a WiFi network with strongest WiFi signal (RSSI). This requires registering one or more access points with SSID and password. It automatically switches to another WiFi network when the WiFi connection is lost.*/

ESP8266WiFiMulti WiFiMulti;

/*------------------------------------------------------*/

char ssid[]     = "yournetworkname";    
char password[] = "yournetworkpassword";

 /*-------------------------------------------------------------*/

/*CBM printer device addresses*/
#define PRINTER_PRIM_ADDR       4   /*Typically CBM printer is device 4 or 5*/
#define PRINTER_SEC_ADDR        7   /*7 = printer mode*/

/*------------------------------------------------------*/

WiFiClientSecure client;  /*Use WiFiClient class to create TCP connections*/  

/*------------------------------------------------------*/

unsigned char scan_for_tag(char* outputstring, int outputmaxsize);    /*get the first available tag*/
unsigned long read_data(char* outputstring, int outputmaxsize);

void substitute_escape_codes(char* datastring);
void substitute_emphasis_codes(char* datastring);

/*------------------------------------------------------*/
/*                      I N I T                         */
/*------------------------------------------------------*/
void setup()
{
  Serial.begin(115200);
  Serial.println();

  IEC_init();

  Serial.println("Setting up wifi connection...");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  Serial.println("Arduino based ESP8266 IEC serial printer RSS");
}

/*------------------------------------------------------*/
/*                      M A I N                         */
/*------------------------------------------------------*/
void loop()
{
  const int httpPort = 443;                   /*80 is for HTTP / 443 is for HTTPS! */

  //const char* host = "www.nu.nl";
  //const char* url = "/rss/Algemeen";

  const char* host = "feeds.bbci.co.uk";
  const char* url = "/news/world/rss.xml";


  #define MAXNUMBEROFMESSAGES 10

  char printer_buf[128];
  char tag_found[64];       /*just make this buffer big enough to hold the required tags, we don't care about other tags (that might not fit), these will be ignored anyway*/
  char title_data[256];
  char descr_data[1024];
  char date_data[128];

  unsigned char lp;
  unsigned char i;  
  boolean process_data;

  /*to prevent printing the same message over and over again, each message is assigned a checksum or fingerprint value (which is nothing more than the sum of all received characters)*/
  unsigned char message_not_printed;
  unsigned long checksum[MAXNUMBEROFMESSAGES];
  unsigned long prev_checksum[MAXNUMBEROFMESSAGES];

  IEC_open(PRINTER_PRIM_ADDR, PRINTER_SEC_ADDR);        /*Prepare for communication with a device on the IEC-bus*/

  //cbm_print_YouTube_graphics(); /*simple bitmap image to pusrade people to subscribe*/
  //cbm_print_graphics();   /*simple test image to see if we can send bitmap data*/
    
  cbm_printmode(WIDTH_SINGLE);  /*normal*/    
  cbm_printmode(NEGATIVE_DISABLED);  /*negative OFF*/
  sprintf(printer_buf, "\n\nProject: RSS feed printer, by Jan Derogee (2022)\n\n");
  cbm_println(printer_buf);      
 


  
//  IEC_close(PRINTER_PRIM_ADDR, PRINTER_SEC_ADDR);       /*End communication with a device on the IEC-bus*/

 /*attention programmer:
 * do not close the IEC-bus, because for some reason (most likely a bug in my own code) the device might not re-open
 * so by keeping it open we avoid that problem (just a quick and dirty fix, I know)
 */

  /*clear list of checksums*/
  for(lp=0;lp<MAXNUMBEROFMESSAGES; lp++)
  {
    checksum[lp] = 0;
  }

  while(1)
  {
    if ((WiFiMulti.run() == WL_CONNECTED))    /*check/update WiFi connection*/
    {
      //Serial.print("connecting to ");
      //Serial.println(host);    
      client.setInsecure();     /*this is the magical line that makes everything work without silly constantly expiring fingerprint-thingies*/      
      if (!client.connect(host, httpPort))
      { 
        Serial.println("connection failed");
        return;
      }
       
      /*send the request to the server*/
      //Serial.printf("Requesting URL: %s\n",url);     
      client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");           
      delay(10000); /*allow client some time to respond*/
      if(client.connected()) /*only get the top-item from the entire RSS feed*/
      {
        for(lp=0; lp<MAXNUMBEROFMESSAGES; lp++)
        {
          /*clear buffers*/
          process_data = false;          
          title_data[0] = 0;
          descr_data[0] = 0;
          date_data[0] = 0;         
          prev_checksum[lp] = checksum[lp]; /*update list of the items from the RSS feed we've recently read, by preserving all the message checksum/fingerprint*/          
          checksum[lp] = 0;

          while(scan_for_tag(tag_found, sizeof(tag_found))) /*see what we find and if we can use it*/
          {
            /*We are only interested in the data that is in between the <item> tags*/
            /*Check for the presence of the opening tag in the RSS datastream*/
            if(strcmp(tag_found, "<item>") == 0)
            {
              process_data = true;
              //Serial.printf("Start of item detected, start gathering data");              
            }            
 
            /*After finding the opening tag of interest, we are allowed to process the data*/
            if(process_data == true)
            {
              if(strcmp(tag_found, "<title>") == 0)
              {
                checksum[lp] = checksum[lp] + read_data(title_data, sizeof(title_data));
              }
              else if(strcmp(tag_found, "<description>") == 0)
              {
                checksum[lp] = checksum[lp] + read_data(descr_data, sizeof(descr_data));
              }
              else if(strcmp(tag_found, "<pubDate>") == 0)
              {
                //checksum[lp] = checksum[lp] + read_data(date_data, sizeof(date_data));
                read_data(date_data, sizeof(date_data));  /*the timestamp does not deserves to be included in the checksum, because someties this is the only thing that changes, cause a duplicate message to be printed*/
              }
              else
              {
                //Serial.printf("unexpected tag found: %s\n", tag_found);  /*for debugging purposes only*/
              }
            }

            /*Check for the presence of the closing tag in the RSS datastream*/          
            if(strcmp(tag_found, "</item>") == 0)
            {
              process_data = false;
              //Serial.printf("End of item reached, time to process the gathered data"\n);
               
              if(checksum[lp] > 0) /*check if the message contains data*/
              {
                /*check if the message is recently printed by scanning through the list of previous checksums*/
                message_not_printed = true;
                for(i=0; i<MAXNUMBEROFMESSAGES; i++)
                {
                  //Serial.printf("checksum[%d]=%d, prev_checksum[%d]=%d\n",lp, checksum[lp], i, prev_checksum[i]);
                  if(checksum[lp] == prev_checksum[i])  /*when this message is already printed, just exit*/
                  {
                    //Serial.println("Match, skip this one");
                    message_not_printed = false;
                    break;  /*exit the for loop*/
                  }              
                }
    
                /*when the message wasn't in the list, it wasn't printed before, so print the message now*/
                if(message_not_printed == true)
                { 
                  yield();  /*pet the watchdog*/                               
                  Serial.printf("checksum[%d]=%d\n",lp, checksum[lp]);
                  Serial.printf("%s\n%s\n%s\n\n",date_data,title_data,descr_data);      
    
                  if(cbm_printmode(WIDTH_SINGLE) > 0)     {break;}                                                
                  sprintf(printer_buf, "================================================================================");
                  cbm_println(printer_buf);      
                  if(cbm_printmode(WIDTH_DOUBLE) > 0)     {break;}                
                  if(cbm_println(date_data) > 0)          {break;}                                
                  if(cbm_printmode(WIDTH_SINGLE) > 0)     {break;}                                
                  if(cbm_println(title_data) > 0)         {break;}
                  sprintf(printer_buf, "--------------------------------------------------------------------------------");
                  cbm_println(printer_buf);                      
                  if(cbm_printmode(WIDTH_SINGLE) > 0)     {break;}                                                 
                  if(cbm_println(descr_data) > 0)         {break;}
                  cbm_newline();                                                 
                }           
              }             

              break;  /*exit the while loop*/ 
            }           
            
          }
        }
      }          
    }

    
    delay(60000); /*wait a minute, then check again*/
    Serial.println("---------------------------------");           

  }
}


/*-------------------------------------------------------------*/

/*get the first available tag, which could be an opening tag like <...> or*/
/*a closing tag like </...> return false if no tag could be found*/
unsigned char scan_for_tag(char* outputstring, int outputmaxsize)
{
  char ch;
  unsigned char i;
  boolean capture_data;

  outputstring[0] = 0;  /*erase the string*/
  i=0;
  capture_data = false;
  while(client.connected()) /*only attempt to get something if we are connected to a host*/
  {
    yield();  /*pet the watchdog*/
    
    ch = static_cast<char>(client.read());  /*get a single character*/
    //Serial.print(ch); /*debug only, so that we can see the data coming in*/

    if(ch == '<')
    {
      capture_data = true;
    }

    if(capture_data == true)
    {
      outputstring[i++] = ch;
      outputstring[i] = 0;      
      if(ch == '>')
      {        
        //Serial.printf("tag detected:%s\n", tag_found); /*debug only, so that we can see the data coming in*/        
        return(true); /*we've found something (opening or closing tag)*/
      }
    }

    if(i >= (outputmaxsize-1)) /*we check for outputmaxsize-1 because we need to store the end-of-string marker too*/
    {
      //Serial.printf("Error: exceeding tag buffer size, tag found:%s\n", tag_found); /*debug only, so that we can see the data coming in*/              
      return(true); /*although this is an error, we did find a tag, so ne reason to panic at this point*/
    }

  }
  return(false); /*oops... something went wrong*/
}

/*This routine is to be called when the data is at the point of interest. Meaning*/
/*that all this routine has to do is read until it finds the start of a closing tag*/
unsigned long read_data(char* outputstring, int outputmaxsize)
{
  char ch;
  unsigned long total = 0;;
  boolean exit_found = false;
  unsigned int lp = 0;

  outputstring[0] = 0;  
  while(client.connected()) /*only attempt to get something if we are connected to a host*/
  {
    yield();  /*pet the watchdog*/
    
    ch = static_cast<char>(client.read());  /*get a single character*/
    //Serial.print(ch); /*debug only, so that we can see the data coming in*/

    if(ch>127) {ch='*';}  /*prevent printing strange characters if outside ASCII 7-bit range*/

    if(lp < (outputmaxsize-1))  /*make sure we aren't writing too much*/
    {
      //Serial.printf("lp=%d, ch=%c\n",lp , ch);                 
      outputstring[lp++] = ch;  /*add char to string array and increment pointer*/
      outputstring[lp] = 0;     /*end string properly*/
      total = total + ch; /*simply add all char values*/
      //Serial.println(total);  /*debug only*/
    }

    /*check for end-trigger, if found completely, then remove that part from the gathered string*/
    if(exit_found == false)
    {
      if(ch =='<') /*check if incoming char is equal to the beginning of an end tag*/
      {
        exit_found = true;
      }
    }
    else
    {
      if(ch =='/') /*check if incoming char is equal to the beginning of an end tag*/
      {
        lp--;                     /*remove the '/'     */        
        outputstring[lp] = 0;     
        lp--;                     /*remove the '<'     */
        outputstring[lp] = 0;

        /*the gathered data might require some post processing, so we do that here*/
        substitute_escape_codes(outputstring);      /*scan the string for escape-codes and process them accordingly*/          
        substitute_emphasis_codes(outputstring);    /*scan the string for text that needs to be emphasized*/                                  
        return(total); /*mission complete, exit*/            
      }
      else
      {
        exit_found = false;
      }
    }
  }
  return(0); /*hmmm... I guess something went wrong, although we might have been partially succesful, better not use the data*/
}


/*XML text might contain escaped characters, which we need to process here*/
void substitute_escape_codes(char* datastring)
{
  boolean pure_text;
  unsigned int p1;
  unsigned int p2;  
  p1=0;
  p2=0;

  //Serial.println(datastring); /*before (debug only)*/

  pure_text = false;
  while(datastring[p1] > 0)
  {
    yield();
    if ((datastring[p1] == '<') && (datastring[p1+1] == '!') && (datastring[p1+2] == '[') && (datastring[p1+3] == 'C') && (datastring[p1+4] == 'D')  && (datastring[p1+5] == 'A') && (datastring[p1+6] == 'T') && (datastring[p1+7] == 'A')  && (datastring[p1+8] == '['))  {pure_text = true;   p1=p1+9;}
    if ((datastring[p1] == ']') && (datastring[p1+1] == ']') && (datastring[p1+2] == '>'))                                                                                                                                                     {pure_text = false;  p1=p1+3;}
      
    if((datastring[p1] == '&') && (pure_text == false)) /*when pure text, do not process the contents for escape codes*/
    {
      if      ((datastring[p1+1] == 'a') && (datastring[p1+2] == 'm') && (datastring[p1+3] == 'p') && (datastring[p1+4] == ';'))                                {datastring[p2] = '&';  p1=p1+4;} /* &amp;  = Ampersand '&'    */
      else if ((datastring[p1+1] == 'l') && (datastring[p1+2] == 't') && (datastring[p1+3] == ';'))                                                             {datastring[p2] = '<';  p1=p1+3;} /* &lt;   = Less-than '<'    */
      else if ((datastring[p1+1] == 'g') && (datastring[p1+2] == 't') && (datastring[p1+3] == ';'))                                                             {datastring[p2] = '>';  p1=p1+3;} /* &gt;   = Greater-than '>' */
      else if ((datastring[p1+1] == 'q') && (datastring[p1+2] == 'u') && (datastring[p1+3] == 'o') && (datastring[p1+4] == 't')  && (datastring[p1+5] == ';'))  {datastring[p2] = '"';  p1=p1+5;} /* &quot; = Quotes '"'       */
      else if ((datastring[p1+1] == 'a') && (datastring[p1+2] == 'p') && (datastring[p1+3] == 'o') && (datastring[p1+4] == 's')  && (datastring[p1+5] == ';'))  {datastring[p2] = 0x27; p1=p1+5;} /* &apos; = Apostrophe '''   */         
    }
    else
    {
      datastring[p2] = datastring[p1];
    }
    p1++;
    p2++;
  }
  datastring[p2] = 0;
  //Serial.println(datastring); /*after (debug only)*/
}

/*XML text might contain <em> and </em> codes, to indicate text that needs to be emphasized*/
void substitute_emphasis_codes(char* datastring)
{
  unsigned int p1;
  unsigned int p2;  
  p1=0;
  p2=0;

  //Serial.println(datastring); /*before (debug only)*/

  while(datastring[p1] > 0)
  {
    yield();
    if(datastring[p1] == '<')
    {
      if      ((datastring[p1+1] == 'e') && (datastring[p1+2] == 'm') && (datastring[p1+3] == '>'))                               {datastring[p2] = NEGATIVE_ENABLED;   p1=p1+3;} /*enable emphasis*/
      else if ((datastring[p1+1] == '/') && (datastring[p1+2] == 'e') && (datastring[p1+3] == 'm') && (datastring[p1+4] == '>'))  {datastring[p2] = NEGATIVE_DISABLED;  p1=p1+4;} /*disable emphasis*/
    }
    else
    {
      datastring[p2] = datastring[p1];
    }
    p1++;
    p2++;
  }
  datastring[p2] = 0;
  //Serial.println(datastring); /*after (debug only)*/
  //Serial.println("---------------"); /*after (debug only)*/
}          



/*-----------------------------------------------------------------*/
/*        UNUSED CODE (might be of use someday, but not today)     */
/*-----------------------------------------------------------------*/

//  sprintf(printer_buf, "UTF8_NORMAL\n");
//  cbm_print_UTF8(printer_buf, UTF8_NORMAL);

//  sprintf(printer_buf, "UTF8_DOUBLEWIDTH\n");
//  cbm_print_UTF8(printer_buf, UTF8_DOUBLEWIDTH);

//  sprintf(printer_buf, "UTF8_INVERSE\n");
//  cbm_print_UTF8(printer_buf, UTF8_INVERSE);

//  sprintf(printer_buf, "UTF_ITALIC dit is een test\n");
//  cbm_print_UTF8(printer_buf, UTF8_ITALIC);

//  sprintf(printer_buf, "UTF_UNDERLINE\n");
//  cbm_print_UTF8(printer_buf, UTF_UNDERLINE);

//  sprintf(printer_buf, "print with all options\n");
//  cbm_print_UTF8(printer_buf, UTF8_DOUBLEWIDTH | UTF8_INVERSE | UTF_ITALIC | UTF_UNDERLINE);

///*hmmm... for some reason the codes received aren't UTF8, so printing UTF8 chars gives even worse results*/
///*instead of a single UTF8 character, the special character is combined of two other characters, no idea how this works, figure it out some other time...*/
////                cbm_print_UTF8(date_data, UTF8_ITALIC);
////                cbm_newline();
////                cbm_print_UTF8(title_data, UTF8_UNDERLINE);
////                cbm_newline();                
////                cbm_print_UTF8(descr_data, UTF8_NORMAL);
////                cbm_newline();                
