#include <chibi.h>
#include <String.h>
#include <SPI.h>
#include <Ethernet.h>

#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define FREAKDUINO_LONG_RANGE 1

int linelength = 16;
int maxlines = 2;

//broadcasts this stop id... eventually only send to interested party
char stopid[] = "101924";

//ETHERNET CARD
//connected to pins 0-1, 9-13
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetClient outboundclient;//for making outbound requests

//request to remote api
char GETURL[] = "GET /?lines=2&linelen=16&id=%s HTTP/1.1";
char serverUrl[] = "gar.mtabuscis.net";
char* url = (char *) malloc(256);//1 malloc for entire program

void setup()
{
  Serial.begin(9600);
  
  Serial.println("Starting... ");
  
  // Init the chibi wireless stack
  chibiInit();
  chibiSetChannel(2);
  chibiSetMode(BPSK_MODE);
  chibiSetDataRate(CHB_RATE_250KBPS);
  chibiHighGainModeEnable();
  
  chibiSetShortAddr(0);
  
  Ethernet.select(8);
  
  Ethernet.begin(mac);
  Serial.println(Ethernet.localIP());          // start to listen for clients
  
  delay(5000);
  
  Serial.println("Ready!");
}

void loop(){
  
  String data = gethttp();
  if(data.length() < 4){
    delay(5000);
    return;
  }
  
  Serial.print("++");
  Serial.print(data);
  Serial.println("++");
  
  transmit(data);
  
  delay(5000);
}

String gethttp(){
  sprintf(url, GETURL, stopid);
  
  Serial.print("Requesting url: ");
  Serial.println(url);
  
  String data = "";
  if (outboundclient.connect(serverUrl, 80)) {
    outboundclient.println(url);
    outboundclient.print("Host: ");
    outboundclient.println(serverUrl);
    outboundclient.println("Connection: close\r\n");
    boolean start = false;
    int stage = 0;
    while(true){
      if (outboundclient.available()) {
        char c = outboundclient.read();
        if(start){
          //Serial.println("p");
          data = data + c;
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
        outboundclient.stop();
        return data;
      }
    }
  }
}

void transmit(String msg){
  
  int len = msg.length();
  if(len > 32){
    len = 32;
  }
  
  byte data[len+1];
  
  strncpy((char *)data, msg.c_str(), len);
  data[len] = '\0';
  
  Serial.print("_XMIT__");
  Serial.print((char *)data);
  Serial.println("___");
  
  //BROADCAST_ADDR
  chibiTx(BROADCAST_ADDR, data, len);//includes null byte on xmit

  delay(1000);

  int i = 0;
  //wipe out data var
  for(i=0; i<len+1; i++){
    data[i] = '\0';
  }
}
