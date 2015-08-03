#include <LiquidCrystal.h>
#include <chibi.h>

#define ME 101924 // this is my address (e.g. a stop id), in the bronx

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
    char data[maxsize + 1];
    data[maxsize] = '\0';
    
    byte buf[maxsize];
    chibiGetData(buf);
    strncpy(data, (char*) buf, maxsize-1);
    
    String lcdtext(data);
    lcdprintstr(lcdtext);
  }

}
