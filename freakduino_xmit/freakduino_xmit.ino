#include <chibi.h>
#include <String.h>
#include <SPI.h>
#include <Ethernet.h>

#define CHANNEL 3
#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define FREAKDUINO_LONG_RANGE 1

char* myXmitId = "A\0";
char stopid[] = "101924";

uint8_t AESKEY = 329093092;

//ETHERNET CARD
//connected to pins 0-1, 9-13
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetClient outboundclient;//for making outbound requests

//request to remote api
char GETURL[] = "GET /?lines=4&id=%s HTTP/1.1";
char serverUrl[] = "gar.mtabuscis.net";
char* url = (char *) malloc(128);//1 malloc for entire program
char tempTime[10];
char* data = (char*)malloc(256*sizeof(char));

void setup()
{
  
  Serial.begin(9600);
  
  Serial.println("Starting... ");
  
  // Init the chibi wireless stack
  chibiInit();
  chibiSetChannel(CHANNEL);
  chibiAesInit(&AESKEY);
  chibiSetMode(BPSK_MODE);
  chibiSetDataRate(CHB_RATE_250KBPS);
  chibiHighGainModeEnable();
  
  chibiSetShortAddr(0);
  
  Ethernet.select(8);
  Ethernet.begin(mac);
  Serial.println(Ethernet.localIP());          // start to listen for clients
  
  delay(1000);
  
  Serial.println("Ready!");
}

void loop(){
  
  ltoa(millis()/1000,tempTime,10);
  
  strcpy(data+0, myXmitId);
  strcpy(data+strlen(data), tempTime);
  strcpy(data+strlen(data), "\n");
  
  char* dldData = gethttp();
  int len = strlen(dldData);
  
  strcpy(data+strlen(data), dldData);
  
  if(len < 2){
    delay(5000);
    return;
  }
  
  //char* out;
  //chibiAesEncrypt(len, (uint8_t*)data, (uint8_t*)out);
  
  transmit(data);//just transmit the minimized data!
  
  // transmit every 10 seconds
  delay(10000);
}

char* gethttp(){
  sprintf(url, GETURL, stopid);
  
  Serial.print("Requesting url: ");
  Serial.println(url);
  
  int pointer = 0;
  char data[256];
  if (outboundclient.connect(serverUrl, 80)) {
    delay(50);
    outboundclient.println(url);
    outboundclient.print("Host: ");
    outboundclient.println(serverUrl);
    outboundclient.println("Connection: close\r\n");
    delay(50);
    boolean start = false;
    int stage = 0;
    while(true){
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
        delay(50);
        data[pointer] = '\0';//termination of string
        outboundclient.stop();
        delay(50);
        return data;
      }
    }
  }
}

void transmit(char* msg){
  
  int len = strlen(msg)+1;
  
  byte data[len];
  
  strncpy((char *)data, msg, len);
  data[len] = '\0';
  
  Serial.print("___XMIT___=");
  Serial.print((char *)data);
  Serial.println("___");
  
  //BROADCAST_ADDR
  chibiTx(BROADCAST_ADDR, data, len);//includes null byte on xmit

/*
  //xmit duplication
  delay(1500);
  Serial.println("duplico");
  chibiTx(BROADCAST_ADDR, data, len);//includes null byte on xmit
*/

  int i = 0;
  //wipe out data var
  for(i=0; i<len; i++){
    data[i] = '\0';
  }
}
