#include <LiquidCrystal.h>
#include <chibi.h>

#define ME 101924 // this is my address (e.g. a stop id), in the bronx

#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define FREAKDUINO_LONG_RANGE 1

const int linelength = 16;
const int maxlines = 2;
const int maxsize = (linelength * maxlines) + 1;//(lines * length) + 1

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
  chibiSetChannel(2);
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
    
    char data[maxsize + 1];
    data[maxsize] = '\0';
    
    byte buf[maxsize];
    chibiGetData(buf);
    strncpy(data, (char*) buf, maxsize-1);
    
    String temp = String(data);
    String out = temp.substring(0, 16) + "-" + String(rssi, DEC) + "db up:" + (millis()/1000);
    
    lcdprintstr(out);
    
  }

}
