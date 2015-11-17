#include <LiquidCrystal.h>
#include <chibi.h>
#include <AESLib.h>

//TODO: need someway to reduce this to 16 bits or 65535 max
#define STOP_ID 1235 // what is my local stop id to listen for over the air?

#define VERSION "1.5"
#define CHANNEL 3 // use channels 1-9
#define BPSK_MODE 3
#define CHB_RATE_250KBPS 0
#define DISPLAY_COLUMNS 16 // how many columns are there on the display?
#define DISPLAY_LINES 2 // how many lines are on the display?
#define SCROLL_LINES 2 // how many additional lines should the last row scroll through (doesn't include message lines)
#define SCROLL_RATE 5 // seconds
#define RECEIVE_TIMEOUT 180 // seconds
#define DEFAULT_PSA_TIMEOUT 900 // seconds (900 seconds is 15 minutes), this is only used in the case of the system clock resetting after 57 days
#define ARRAYSIZE 128

uint8_t AESKEY[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};

//for wireless reception
uint8_t* input = (uint8_t*)malloc(ARRAYSIZE*sizeof(uint8_t));

//keeping track of previous messages to avoid a repeat loop (defaults to 6)
int lastHeaderPos = 0;
const int lastHeaderMax = 4;
char lastHeader[lastHeaderMax][12] = {'\0'};
unsigned long lastReceivedTime = 0; // next display update should happen in SCROLL_RATE seconds initially (millis() will be 0 in theory)
unsigned int minutesCountedDown = 0; // tracker to determine if we should remove a minute from the last displayDistance array (for type = minutes)
unsigned long lastScrollTime = 0;
char* delimeter = "\n";
unsigned short debugMode = 0;
unsigned long lastReceivedTimeDebug = 0;
byte receivedRSSI = 0;
boolean scroll = false;

char psa[17] = {'\0'}; // public service announcements can be up to 16 characters long or one standard display line
unsigned long psa_timeout = 0;
int psa_on = 0; // trigger to determine if PSA is currently being displayed
char subbuffpsa[16] = {'\0'};//for psa announcement messages

//holds onto data received over the air
char displayRoute[SCROLL_LINES + DISPLAY_LINES][12]; // worst case: 12 characters, example: "bx142-sbs: \0"
int displayDistance[SCROLL_LINES + DISPLAY_LINES];
int displayType[SCROLL_LINES + DISPLAY_LINES]; // 0 is minutes, 1 is in stops away
int displaySelector = DISPLAY_LINES-1; // select a line to display (after the initial lines have been filled)

char displayText[DISPLAY_LINES][DISPLAY_COLUMNS+1]; // placeholder for what exactly shows up on the display including an additional column for a null character

LiquidCrystal lcd(9, 8, 5, 4, 3, 2);

void setup()
{
  //TODO: remove in production because serial output makes no sense
  Serial.begin(9600);
  Serial.println("Initializing");
  
  chibiInit();
  chibiSetChannel(CHANNEL);
  chibiSetMode(BPSK_MODE);
  chibiSetDataRate(CHB_RATE_250KBPS);
  chibiSetShortAddr(STOP_ID);
  lcd.begin(DISPLAY_COLUMNS, DISPLAY_LINES);
  clearReceivedData();
  clearReceivedPSA();
  
  //update the display with the latest available data
  updateDisplay();
  
  Serial.println("Done.");
}

void loop()
{
  
  boolean updated = false;
  
  //collect and process any received data
  if(isRCVD())
  {
    processReceivedData();
    updated = true;
  }
  
  //check to see if millis() reset due to being on for 57 days+, since this is a rare occasion, we simply remove all existing timeouts and reset them, which in the worst case, isn't too bad
  if(millis() < lastReceivedTime)
  {
    Serial.print("Millis has reset");
    
    //when millis resets, give another RECEIVE_TIMEOUT grace period to the last messages (including PSA's)
    lastReceivedTime = (RECEIVE_TIMEOUT * 1000L) + millis();
    lastScrollTime = 0;
    if(psa_timeout != 0)
    {
      psa_timeout = (DEFAULT_PSA_TIMEOUT * 1000L) + millis();
    }
    
  }
  
  //check to see if the received data is too old
  if( lastReceivedTime != 0 && ( (RECEIVE_TIMEOUT * 1000L) + lastReceivedTime ) < millis() )
  {
    Serial.print("The received data timed out, remov");
    clearReceivedData();
  }
  
  // TODO how about the case when there was no received data, but there is still a PSA?
  if(psa_timeout != 0 && millis() > psa_timeout)
  {
    //remove the PSA from the queue of messages to be displayed
    Serial.print("PSA has timed out, remov");
    clearReceivedPSA();
  }
  
  
  // implement countdown by looking at how long has elapsed
  updated = countDown() || updated; // logical or will update the trigger to update the display
  
  //Trigger a scroll to the next bottom line of data
  if( abs(millis() - lastScrollTime) > (SCROLL_RATE*1000L) )
  {
    lastScrollTime = millis();
    scroll = true;
    updated = true;
  }
  
  //update the display
  if(updated)
  {
    updateDisplay();
  }
  
}

/*
  Receive data from the master transmitter, returns true when something new has been received
 */
boolean isRCVD()
{ 
  if(chibiDataRcvd() == true)
  {
    
    memset(input, 0, ARRAYSIZE);
    int len = chibiGetData(input);
    receivedRSSI = chibiGetRSSI();
    
    memset(input+len, 0, ARRAYSIZE-len);
    
    /*
    Serial.print("Encrypted Input: ");
    int k = 0;
    for(k=0; k<ARRAYSIZE; k++){
      Serial.print(input[k]);
      Serial.print(",");
    }
    Serial.println();
    */
    
    //decrypt
    if(len%16 != 0 || len < 20){
      Serial.print(len);
      Serial.println(" Incomplete data, dropping");
      return false;
    }
    
    for(size_t ix = 0; ix < len; ix += 16) {
      aes256_dec_single(AESKEY, input+ix);
    }
    
    //process the first line of the string received (strtok is destructive, later processing can resume normally as if first line doesn't exist)
    char* dataReceivedPointer = (char*)input;
    char* first = strtok(dataReceivedPointer, delimeter);
    
    int i=0;
    //have we seen this message before?
    for(i=0; i<lastHeaderMax; i++)
    {
      if(strcmp(lastHeader[i], first) == 0)
      {
        //data is same
        Serial.println("Receiving OLD data, nothing happens now.");
        return false;
      }
    }   
    
    Serial.println("Receiving NEW data, retransmission now then processing.");
    strcpy(lastHeader[lastHeaderPos], first);
    lastHeaderPos = (lastHeaderPos + 1) % lastHeaderMax;
    
    //retransmit!
    chibiTx(BROADCAST_ADDR, input, len);
    return true;
    
  }
  
  return false;
}

void processReceivedData()
{

  /*
    Protocol:
    For standard stops:
    If @ exists, $route@$minutes_remaining
    if # exists, $route#$stops_remaining
    
    For broadcast messages (addressed to one or all nodes via the wireless stack):
    if * exists, $expires*$message
    // expires is in seconds, message will be what is displayed
    // this system stores only the last [max] 16-character message separately from normal stop data along with an expiration timeout and round-robins it with the stop data
  */
    
  //clear out the existing array of lcd text
  clearReceivedData();
  debugMode = 0; // disable debug mode
  lastReceivedTimeDebug = 0;
  
  lastReceivedTime = millis();
  int count = 0;
  int maxcount = SCROLL_LINES + DISPLAY_LINES;
  
  char* dataReceivedPointer = (char*)input;
  char* line = strtok(NULL, delimeter); 
  
  while(line != NULL && debugMode < 1 ) // break out of the loop if we're in debug mode
  {
    //only save enough data for the display
    if(count < maxcount)
    {
      processReceivedLine(line, count, true);
      line = strtok(NULL, delimeter);
    }
    else
    {
      //we don't want to save anything into the array now... only look for debug/psa messages
      processReceivedLine(line, count, false);
      line = strtok(NULL, delimeter);
      break;
    }
    ++count;
  }
  
}

void processReceivedLine(char* line, int count, boolean save)
{
  if(line[0] == '?')
  {
    // debug mode enable
    debugMode = 1;
    lastReceivedTimeDebug = millis();
  }
  else if(strstr(line, "*") != NULL)
  {
    clearReceivedPSA();
    // handle broadcast messages
    char* separator = strchr(line, '*');
    int len = separator-line;
    memcpy( subbuffpsa, line, len );
    subbuffpsa[len] = '\0';
    long localTimeout = atoi(subbuffpsa);
    psa_timeout = millis() + (localTimeout * 1000L);
    strcpy(psa, separator+1);
    
  }
  else
  {
    if(save == false)
    {
      return;//don't do anything except process psa/debug messages if we don't want to save anything more.
    }
    
    //handle regular bus arrival messages
    int type = 0; //0 is minutes, 1 is stops
    char displayLine[DISPLAY_COLUMNS] = {'\0'};
    
    char* separator = strchr(line, '@');
    if(separator == 0)
    {
      separator = strchr(line, '#');
      type = 1;
    }
    
    if(separator != 0){
      
      //parse the information
      int x_away = atoi(separator+1);
      
      //prepare the separator for another round
      *separator = 0;
      
      strcpy(displayRoute[count], line);
      
      displayType[count] = type;
      displayDistance[count] = x_away;
      
      /*
      Serial.print("route/distance/type:");
      Serial.print(displayRoute[count]);
      Serial.print("/");
      Serial.print(displayDistance[count]);
      Serial.print("/");
      Serial.println(displayType[count]);
      */
    }
    
  }
}

// main method to display the latest data on the display (and rotate through data points occasionally)
void updateDisplay()
{
  
  whiteoutDisplayText();
  
  // how much data is there?
  int k=0;
  int dataPresent=0;
  for(k=0; k<(DISPLAY_LINES+SCROLL_LINES); k++)
  {
    //0 and 1 are the only valid display types, by default this is set to 9 when nothing is present
    if(displayType[k] == 0 || displayType[k] == 1)
    {
      dataPresent = k + 1;
    }else{
      break; // break the for loop
    }
  }
  
  if(dataPresent == 0)
  {
    // no data exists, print out waiting for data message
    setDataNotAvailableMessage();
    writeDisplayUpdate();
    return; //nothing more to do
  }
  
  // handle the first lines of the display with countdown text
  int i=0;
  while(i < DISPLAY_LINES-1 && i < dataPresent)
  {
    copyData(i, i);
    ++i;
  }
  
  // handle final line of the display
  if(psa_timeout != 0 && psa_on == 0)
  {
     //TODO test this
     // Place the PSA on the final line of the display
     strcpy(displayText[DISPLAY_LINES-1], psa);
     psa_on = 1;
     
  }else{
    
    psa_on = 0;
    
    if(scroll == false)
    {
      //we don't want to quite scroll yet if the update is triggered by new data
      --displaySelector;
      Serial.println("Not scrolling");
    }
    
    //is there enough data to consider writing a final round robin line?
    if( DISPLAY_LINES > dataPresent )
    {
      Serial.println("No data exists");
      if(psa_timeout == 0)
      {
        writeDisplayUpdate();
      }
      // There is a PSA available, continue since it will be caught on the next update momentarily
      return; // no further data available
    }
    
    // round robin display data selector update (if at end of data set, wrap around) | on initial startup, new data will prevent scrolling, this corrects that
    if(displaySelector >= (SCROLL_LINES + DISPLAY_LINES) || displaySelector < DISPLAY_LINES-1)
    {
      displaySelector = DISPLAY_LINES-1;
    }
    
    // is there enough data to copy the next row over to the display or do we need to loop around?
    if( displaySelector >= dataPresent )
    {
      Serial.println("Display selector reset, insufficient data");
      displaySelector = DISPLAY_LINES-1;
    }
    
    copyData(displaySelector, DISPLAY_LINES-1);
    
    ++displaySelector;
    scroll = false;//always reset the scroll variable once we've visted this function
    
  }
  
  writeDisplayUpdate();
  
}

//set the default no data available message
void setDataNotAvailableMessage()
{
  static char lineA[17]; // 17 characters for final null byte
  strcpy(lineA, "Bustime/GAR v");
  strcpy(lineA+strlen(lineA), VERSION);
  char lineB[17] = "Waiting for data";
  strcpy(displayText[0], lineA);
  strcpy(displayText[1], lineB);
}

//write the prepared message to the display
void writeDisplayUpdate()
{
  // always display the first line regardless
  lcd.clear();
  
  if(debugMode > 0)
  {
    // in debug mode
    // show signal strength of update (line 1)
    lcd.setCursor(0, 0);
    lcd.print(receivedRSSI, DEC);
    lcd.print(", +[84-0]-"); // 84 (-7 dB ) to 0 (-91 dB) 
    // show seconds since last update (line 2)
    lcd.setCursor(0, 1);
    lcd.print("lstx: ");
    long elapse = millis() - lastReceivedTimeDebug;
    lcd.print(elapse/1000, DEC);
    
    Serial.print(receivedRSSI);
    Serial.print(", +[84-0]-  lstx: ");
    Serial.println(elapse/1000);
  }
  else
  {
    int i=0;
    while(i < DISPLAY_LINES)
    {
      lcd.setCursor(0, i);
      lcd.print(displayText[i]);
      
      Serial.print(i);
      Serial.print(" < line printing to LCD. text: ");
      Serial.println(displayText[i]);
      ++i;
    }
  }
  
}

// this method copies data from a source row (from received data) to a destination row on the display (the datastructure that feeds what the display shows)
void copyData(int fromDataRow, int toDisplayRow)
{
  char rowidAndSpace[4];
  sprintf(rowidAndSpace, "%d ", fromDataRow+1);
  
  strcpy(displayText[toDisplayRow], rowidAndSpace);
  strcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), displayRoute[fromDataRow]); // appends a final null character that is erased by the next copy
  
  char distminValue[10];
  sprintf(distminValue, " %d", displayDistance[fromDataRow]);
  
  if(displayType[fromDataRow] == 0){ // 0 is minutes
    if(displayDistance[fromDataRow] < 1)
    {
      // in the case of 0 or fewer minutes remaining, write DUE instead of time / min)
      memcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), " DUE", 4);
    }else{
      // in the case of 1 or more minutes remaining, just write that number followed by min (for minutes)
      strcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), distminValue); // copy the number to the main output string
      if(minutesCountedDown > 0)
      {
        // when the system removed a minute, place a * at the end to denote it's estimated
        memcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), "m*", 3);
      }else{
        // if the received data is current, don't display a *
        memcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), "m", 2);
      }
    }
  }else if(displayType[fromDataRow] == 1){ // 1 is stops
    // write out to the main display string how many stops remain
    strcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), distminValue); // copy the number to the main output string
    memcpy(displayText[toDisplayRow]+strlen(displayText[toDisplayRow]), "s", 2);
  }
}  

// implement countdown by looking at how long has elapsed, if the number would go below 0, remove it from the display list
boolean countDown()
{
  
  if(lastReceivedTime == 0)
  {
    return false;
  }
  
  boolean lupdated = false;
  unsigned int deltaSeconds = (millis() - lastReceivedTime) / 1000; // delta is seconds since last data received
  
  if( (deltaSeconds / 60) > minutesCountedDown)
  {
    
    ++minutesCountedDown;//increment the countdown flag
    int i=0;
    for(i=0; i<(DISPLAY_LINES+SCROLL_LINES); i++)
    {
      // TODO to make stops have a * next to them, change the display type of 1 to 2...
      //0 is the only display type that counts down, so only count down types of 0
      if(displayType[i] == 0)
      {
        // if this number goes to 0 or negative minutes, we will just display "due" in its place, the timeout mechanism will handle this for now
        // NOTE: in the future, we may want to add an else to remove negative values from the display cache all together instead of relying on the timeout mechanism
        displayDistance[i] = displayDistance[i] - 1;
        lupdated = true; //something was updated, trigger a true response from this function
        
        
        Serial.print("Counting down on row ");
        Serial.print(i);
        Serial.print(" new dist ");
        Serial.println(displayDistance[i]);
        
      }
    }
  }
  
  return lupdated;
}

// clears the text that is prepared for displaying
void whiteoutDisplayText()
{
  int i=0;
  int j=0;
  int x_max = DISPLAY_LINES;
  int y_max = DISPLAY_COLUMNS + 1;
  for(i=0; i<x_max; i++){
    for(j=0; j<y_max; j++){
      displayText[i][j] = ' ';
    }
    displayText[i][j-1] = '\0'; // final character is a null character for displaying text cleanly
  }
}

// clears the processed arrivals data that has been received
void clearReceivedData()
{
  int i=0;
  int j=0;
  int x_max = SCROLL_LINES + DISPLAY_LINES - 1;
  int y_max = DISPLAY_COLUMNS;
  for(i=0; i<x_max; i++){
    displayDistance[i] = 0;
    displayType[i] = 9; // 9 is not a valid type
    for(j=0; j<y_max; j++){
      displayRoute[i][j] = '\0';
    }
  }
  
  // reset the minute countdown flag to begin a new countdown
  minutesCountedDown = 0L;
  lastReceivedTime = 0L;
}

// clears the latest Public Service Announcement
void clearReceivedPSA()
{
  int i=0;
  for(i=0;i<32;i++){
    psa[i] = '\0';
  }
  psa_timeout = 0;
}

/*
// if needed --> Append Char Function 
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


