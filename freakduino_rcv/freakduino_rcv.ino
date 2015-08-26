#include <LiquidCrystal.h>
#include <chibi.h>

#define ME 1234 // this is my address (e.g. a stop id), in the bronx, 
//TODO: need someway to reduce this to 16 bits or 65535 max

#define CHANNEL 3
#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define FREAKDUINO_LONG_RANGE 1

uint8_t AESKEY = 329093092;
char* dataReceived = "MTA Bus Time 1.1\nWaiting for data";
boolean dataRCVD = false;
unsigned long systemRunningTime = 0;
unsigned short informationDropOffThreshold = 3; // 3 mins
unsigned short informationUpdateRate = 4000; // 4 secs

LiquidCrystal lcd(9, 8, 5, 4, 3, 2);

void setup()
{
  //TODO: remove in production
  Serial.begin(9600);

  chibiInit();
  chibiSetChannel(CHANNEL);
  chibiAesInit(&AESKEY);
  chibiSetMode(BPSK_MODE);
  chibiSetDataRate(CHB_RATE_250KBPS);
  //  chibiHighGainModeEnable();
  chibiSetShortAddr(ME);
  lcd.begin(16, 2);
  ProcessTextToLCD(dataReceived);
}


void loop()
{ 
  dataRCVD = isRCVD();

  if (dataRCVD || millis() % informationUpdateRate == 0)
  {
    ProcessTextToLCD(dataReceived);
    Serial.println(dataReceived);
  }
  else if (!dataRCVD && ((millis() - systemRunningTime)/60000) % informationDropOffThreshold == 0 )
  {
    dataReceived = (char*) "MTA Bus Time 1.2\nWaiting for data";
  }

}
/*
  Fetching Bus Time Data
 
 */
boolean isRCVD()
{ 
  if (chibiDataRcvd() == true)
  {
    byte buff[256];
    chibiGetData(buff);
    systemRunningTime = millis();
    dataReceived = (char*) buff;
    // free (buff);
    return true;
  }
  return false;
}

/*
   Processing Text to LCD
 */
void ProcessTextToLCD(char* dataRecived)
{
  char* dataRecivedCopy = (char*) malloc(strlen(dataRecived) + 1); 
  strcpy(dataRecivedCopy, dataRecived);
  unsigned short int count = 0;

  char * line = strtok(dataRecived, "\n"); 
  while(line != NULL)
  {
    if(count == 2)
    {
      count = 0;
      delay(informationUpdateRate/2);
    }

    updateLCDScreen(getArrival(line), count);  
    line = strtok(0, "\n");
    ++count;
  }

  strcpy(dataRecived,dataRecivedCopy);
  free(dataRecivedCopy);
}

/*
  Process Bus Time Text
 */

String getArrival(char* busArrival)
{     
  //@ Defines Minutes Away
  //# Defines Stops Away

  String temp = "";

  if(char * separator = strchr(busArrival, '@'))
  {
    *separator = 0;
    temp = temp + busArrival+":";

    ++separator;
    if(!dataRCVD){
      int arrivalTime = atoi(separator);
      arrivalTime = countDownTime(arrivalTime);
      temp = temp + arrivalTime+ "mins*";
    }
    else{
      temp = temp + separator+ "mins";
    }
  }
  else if (char * separator = strchr(busArrival, '#'))
  {
    *separator = 0;
    ++separator;
    temp = temp + busArrival+":"+separator + "stops";     

  }
  else{
    temp = temp + busArrival;
  }

  return temp;
}
/*
  Count Down for stale data
 */
int countDownTime(int input)
{
  int staleDataMinutes = (millis()-systemRunningTime)/60000;

  if(staleDataMinutes > input)
    return 0;
  else
    return (input - staleDataMinutes);
}

/*
  Update LCD screen by a given row
 */
void updateLCDScreen(String Line, unsigned short int row)
{
  char clsChar[17] = "                ";
  lcd.setCursor(0, row);
  lcd.print(clsChar);
  lcd.setCursor(0, row);
  lcd.print(Line);

}


// if needed --> Append Char Function 
/*
char* appendCharPointer(char* dest, char src[])
 {
 size_t len_dest = strlen(dest);
 size_t len_src = strlen(src);
 
 char* ret = new char[len_dest+len_src];
 strcpy(ret, len_src);
 for( int i = 0; i < len_src; i++)
 {
 ret[len_dest+i]=a[i];
 }  
 ret[len_dest+1] = '\0';
 
 return ret;
 }
 */












