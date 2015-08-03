#include <LiquidCrystal.h>
#include <chibi.h>

#define ME 101924 // this is my address (e.g. a stop id), in the bronx

const int linelength = 16;
const int maxlines = 2;
const int maxsize = (linelength * maxlines) + 1;//(lines * length) + 1

void lcdprintstr(String data){
    Serial.print("Writing to display: ");
    Serial.println(data);
    // set up the LCD's number of columns and rows:
    LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
    lcd.begin(16, 2);
    lcd.noCursor();
    lcd.noDisplay();
    lcd.clear();
    delay(500);
    lcd.setCursor(0, 0);
    lcd.print(data.substring(0, 16).c_str());
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print(millis(), DEC);
    delay(500);
    lcd.display();
}

void setup()
{
  
  Serial.begin(9600);
  
  String initmsg = "MTA Bus Time 1.0Waiting for data";
  lcdprintstr(initmsg);
  
  // Init the chibi wireless stack
  chibiInit();
  
  chibiSetShortAddr(ME);
  
  Serial.println("Ready!");
}

void loop()
{ 
  String initmsg(millis());
  lcdprintstr(initmsg);
  delay(500);
  
  return;

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
