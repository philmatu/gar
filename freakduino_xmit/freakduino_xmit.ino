#include <chibi.h>
#include <String.h>
#include <SPI.h>
#include <Ethernet.h>
#include <AESLib.h>

//ethernet from https://github.com/Scott216/Weather_Station_Data/tree/master/Ethernet%20Lib

#define CHANNEL 3
#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define FREAKDUINO_LONG_RANGE 1
#define ARRAYSIZE 128

char* myXmitId = "A0";
char stopid[] = "400171"; //out front of 2 broadway
int xmitdelay = 5;//seconds
uint8_t AESKEY[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};

//ETHERNET CARD
//connected to pins 0-1, 9-13
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetClient outboundclient;//for making outbound requests

//request to remote api
char GETURL[] = "GET /?lines=4&id=%s HTTP/1.1";
char serverUrl[] = "gar.mtabuscis.net";
char* url = (char *) malloc(128);//1 malloc for entire program
char tempTime[10];
uint8_t* plain = (uint8_t*)malloc(ARRAYSIZE*sizeof(uint8_t));

void setup()
{
  
  Serial.begin(9600);
  
  Serial.println("Starting... ");
  delay(1000);//give ethernet and wireless time to start
  
  // Init the chibi wireless stack
  chibiInit();
  chibiSetChannel(CHANNEL);
  chibiSetMode(BPSK_MODE);
  chibiSetDataRate(CHB_RATE_250KBPS);
  
  chibiSetShortAddr(0);
  
  Ethernet.select(8);
  Ethernet.begin(mac);
  
  delay(5000);
  
  Serial.println(Ethernet.localIP());          // start to listen for clients
  
  Serial.println("Ready!");
}

void loop(){
  //clean variables
  memset(plain, 0, ARRAYSIZE*sizeof(uint8_t));
  
  ltoa(millis()/1000,tempTime,10);
  
  strcpy((char*)plain+0, myXmitId);
  strcpy((char*)plain+strlen((char*)plain), tempTime);
  strcpy((char*)plain+strlen((char*)plain), "\n");
  
  char* dldData = gethttp();
  int len = strlen(dldData);
  
  if(len < 2){
    Serial.println("Nothing received.");
    delay(2500);//delay in ethernet... try again
    return;
  }
  
  strcpy((char*)plain+strlen((char*)plain), dldData);
  
  len = strlen((char*)plain);
  int mod = 16 - (len % 16);//for AES
  if((len + mod) < ARRAYSIZE-1) //ARRAYSIZE is size of data as a whole
  {
    int k = 0;
    for(k=0; k<mod; k++){
      strcpy((char*)plain+strlen((char*)plain), " ");
    }
  }else{
    Serial.println("Critical problem... not enough buffer to handle the data!");
    return;
  }
  
  //encrypt
  len = strlen((char*)plain);
  
  Serial.print("Len ");
  Serial.print(len);
  Serial.print(": ");
  Serial.println((char*)plain);
  
  if(len%16 != 0 || len < 20){
    Serial.println("Incomplete data detected, skipping transmission");
    return;
  }
  
  for(size_t ix = 0; ix < len; ix += 16) {
    aes256_enc_single(AESKEY, plain+ix);
  }
  
  Serial.print("Encrypted Output is ");
  Serial.print(strlen((char*)plain));
  Serial.print(" bytes : ");
  int k = 0;
  for(k=0; k<ARRAYSIZE; k++){
    Serial.print(plain[k]);
    Serial.print(",");
  }
  Serial.println();
  
  secure_transmit(len);//assumes out is populated
  
  // transmit every x seconds
  delay(xmitdelay * 1000);
}

char* gethttp(){
  sprintf(url, GETURL, stopid);
  
  Serial.print("Requesting url: ");
  Serial.println(url);
  
  int pointer = 0;
  char data[ARRAYSIZE];
  if (outboundclient.connect(serverUrl, 80)) {
    delay(250);
    outboundclient.println(url);
    outboundclient.print("Host: ");
    outboundclient.println(serverUrl);
    outboundclient.println("Connection: close\r\n");
    delay(500);
    boolean start = false;
    int stage = 0;
    while(true){
      //Serial.println("loop");
      if (outboundclient.available()) {
        char c = outboundclient.read();
        if(start){
          //Serial.println("p");
          data[pointer] = c;
          pointer++;
        }else{
          //look for \r\n\r\n signifying that the headers have been sent fully
          if(stage == 0){
            if(c == '\r'){
              //Serial.println("h1");
              stage = 1;
            }
          }else if(stage == 1){
            if(c == '\n'){
              //Serial.println("h2");
              stage = 2;
            }else{
              //Serial.println("s01");
              stage = 0;
            }
          }else if(stage == 2){
            if(c == '\r'){
              //Serial.println("h3");
              stage = 3;
            }else{
              //Serial.println("s02");
              stage = 0;
            }
          }else if(stage == 3){
            if(c == '\n'){
              //Serial.println("hf");
              start = true;
            }else{
              //Serial.println("s03");
              stage = 0;
            }
          }
        }
      }
    
      if (!outboundclient.connected()) {
        delay(100);
        data[pointer] = '\0';//termination of string
        outboundclient.stop();
        delay(100);
        return data;
      }
    }
  }
}

void secure_transmit(int len){
  Serial.print("Securely transmitting");
  chibiTx(BROADCAST_ADDR, plain, len);
}
