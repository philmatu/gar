#include <LiquidCrystal.h>
#include <chibi.h>

#define ME 1234 // this is my address (e.g. a stop id), in the bronx, 
//TODO: need someway to reduce this to 16 bits or 65535 max

#define CHANNEL 3
#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define FREAKDUINO_LONG_RANGE 1

uint8_t AESKEY = 329093092;

const int linelength = 16;
const int maxlines = 2;

LiquidCrystal lcd(9, 8, 5, 4, 3, 2);

void lcdprintstr(String data){
    Serial.print("Writing to display: ");
    Serial.println(data);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    //lcd.print(millis()/1000);
    lcd.print(data.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(data.substring(16, 32));
    //lcd.print(millis()/1000);
    delay(100);
}

void setup()
{
  
  Serial.begin(9600);
  
  // Init the chibi wireless stack
  chibiInit();
  chibiSetChannel(CHANNEL);
  chibiAesInit(&AESKEY);
  chibiSetMode(BPSK_MODE);
  chibiSetDataRate(CHB_RATE_250KBPS);
  chibiHighGainModeEnable();
  
  chibiSetShortAddr(ME);
  
  lcd.begin(16, 2);
  
  String initmsg = "MTA Bus Time 1.0Waiting for data";
  lcdprintstr(initmsg);
  
  Serial.println("Ready!");
}

void loop()
{ 
  //constantly receive
  if (chibiDataRcvd() == true)
  {
     byte rssi = chibiGetRSSI();
     Serial.print("RSSI = "); 
     Serial.println(rssi, HEX);
      
     byte buff[256];
     chibiGetData(buff);
      
     char data[256];
     strncpy(data, (char*) buff, 256);
    
     String temp = "";
    
     char* line = strtok(data, "\n");
     while(line != 0){
      int type = 0;//0 is minutes, 1 is stops
      char* separator = strchr(line, '@');//@ is minute symbol, # is stops away symbol
      if(separator == 0){
        separator = strchr(line, '#');
        type = 1;
      }
      
      if(separator != 0){
        //parse the information
        *separator = 0;
        Serial.print("Route: ");
        Serial.print(line);
        
        ++separator;
        int distance = atoi(separator);
        
        temp = temp + line + ": " + distance;
        
        if(type == 0){
          Serial.print(" Minutes Away: ");
          temp = temp + "min\n";
        }else{
          Serial.print(" Stops Away: ");
          temp = temp + "stops\n";
        }
        
        Serial.print(distance);
        Serial.println();
        
      }
      
      line = strtok(0, "\n");
    }
    
    String out = temp.substring(0, 16) + "-" + String(rssi, DEC) + "db up:" + (millis()/1000);
    
    lcdprintstr(out);
    
  }

}
