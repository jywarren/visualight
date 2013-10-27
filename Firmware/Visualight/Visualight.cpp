/*
* VISUALIGHT ARDUINO LIBRARY
*
*
*
*/
#if (ARDUINO >= 100)
    #include "Arduino.h"
#else
    #include <avr/io.h>
    #include "WProgram.h"
#endif

#include "Visualight.h"

Visualight::Visualight(){
	_red = 255;
	_green = 255;
	_blue = 255;
  _white = 255;
	_blinkMe = 0;

	connectTime = 0;
  lastHeartbeat = 0;
	_debug = false;
	resetButtonState = 1;

	isServer = true;
  reconnect = false;
  reconnectCount = 0;

  for(int i=0; i<sizeof(password); i+=4){
    password[i] = '.';
    password[i+1] = '$';
    password[i+2] = '*';
    password[i+3] = '#';
  }
}

void Visualight::setVerbose(boolean set){
	if(set) _debug = true;
	else _debug = false;
  delay(50);
}

void Visualight::update(){

	if(isServer){
    	processServer();
  	}
  	else{
    	processClient();
  	}
  	processButton(); // test to see if this is getting blocked somewhere...
}


void Visualight::setup(uint8_t _MODEL, char* _URL, uint16_t _PORT){
  MODEL = _MODEL;
  URL = _URL;
  PORT = _PORT;
	//colorLED(255,255,255);
	fadeOn();
	pinMode(resetButton, INPUT);
	pinMode(resetPin,OUTPUT);
	digitalWrite(resetButton, HIGH);
	digitalWrite(resetPin, HIGH);  
	//attachInterrupt(4, processButton, CHANGE);
	if(_debug) Serial.begin(9600);
		if(_debug){
		while(!Serial){
		;
		}
	}
	if(_debug) Serial.println(F("Starting"));
	if(_debug) Serial.print(F("Free memory: "));
	if(_debug) Serial.println(wifly.getFreeMemory(),DEC);
	Serial1.begin(9600);
	if (!wifly.begin(&Serial1,&Serial)) {
		if(_debug) Serial.println(F("Failed to start wifly"));
		//RESET WIFI MODULE -- Toggle reset pin
	}

	wifly.getDeviceID(devID,sizeof(devID));
	if(strcmp(devID, "Visualight")==0){
		if(_debug) Serial.println(F("SAME"));
	}
	else{
		if(_debug) Serial.println(F("DIFF"));
		configureWifi();
	}
	//wifly.terminal();
	isServer = EEPROM.read(0);

	wifly.getMAC(MAC, sizeof(MAC));
	if(_debug) Serial.print(F("MAC: "));
	if(_debug) Serial.println(MAC);
	if(_debug) Serial.print(F("IP: "));
	if(_debug) Serial.println(wifly.getIP(buf, sizeof(buf)));

	if (wifly.getPort() != 80) {
		wifly.setPort(80);
		/* local port does not take effect until the WiFly has rebooted (2.32) */
		wifly.save();
		if(_debug) Serial.println(F("Set port to 80"));
			//wifly.reboot();
	}
	if(_debug) Serial.println(F("Ready"));

	if(isServer){
		/* Create AP*/
		if(_debug) Serial.println(F("Creating AP"));
    if (MODEL > 0) colorLED(0,0,255,255);
		else colorLED(0,0,255); // set led to blue for setup
		if(_debug) Serial.println(F("Create server"));
		wifly.setSoftAP();
		EEPROM.write(0, 1);
	}
	else{
		if(_debug) Serial.println(F("Already Configured"));
		//isServer = false;
		if(!connectToServer()){
      reconnectCount++;
    }
	}
}

void Visualight::configureWifi(){
  if(_debug) Serial.println(F("From Config"));
  wifly.factoryRestore();
  delay(2000);
  wifly.setBroadcastInterval(0);  // Turn off UPD broadcast
  wifly.setDeviceID("Visualight");
  wifly.setProtocol(WIFLY_PROTOCOL_TCP);
  wifly.enableDHCP();
  wifly.setChannel("0");
  wifly.save();
  wifly.reboot();
}

void Visualight::joinWifi(){
  /* Setup the WiFly to connect to a wifi network */
  if(_debug) Serial.println(F("From joinWifi"));
  if(_debug) Serial.println(sizeof(password));
  wifly.reboot();
  wifly.setSSID(network);

  //int wifiType = 0; 
  for(int i=0; i<sizeof(password); i++){
    if(strcmp(password, ".$*#.$*#") > 0){
      wifly.setPassphrase(password);
      break;
    } 
  }
  
  wifly.setJoin(WIFLY_WLAN_JOIN_AUTO);
  //wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);
  wifly.save();
  //wifly.finishCommand();
  wifly.reboot();

  if(!wifly.isAssociated()){
    if(_debug) Serial.println(F("Joining network"));
    if (wifly.join()) {
      if(_debug) Serial.println(F("Joined wifi network"));
      if(!connectToServer()){
        reconnectCount++;
      }
    } 
    else {
      if(_debug) Serial.println(F("Failed to join wifi network"));
      delay(1000);
      joinWifi();
      //TODO: Reboot count and reset to AP after count expires
    }
  }
  else{
    //Serial.println(F("Already Joined!"));
    if(MODEL>0)colorLED(255,255,255,255);
    else colorLED(255,255,255);
    if(isServer){
      if(_debug) Serial.println(F("Switch to Client Mode"));
      EEPROM.write(0, 0); 
      isServer = false;
    }
    if(!connectToServer()){
      reconnectCount++;
    }
  }
}

boolean Visualight::connectToServer(){
  if(reconnectCount > 4){
    if(_debug) Serial.println(F("rebooting wifly..."));
    wifly.reboot();
    delay(1000);
    reconnectCount = 0;
  }
  wifly.flushRx();
  if(!wifly.isAssociated()) { //
      if(_debug) Serial.println(F("Joining"));
      if(!wifly.join()){
        if(_debug) Serial.println(F("Join Failed"));
        if(_debug) Serial.println(wifly.isAssociated());
        return false;
      }
    }
    else{
      if(_debug) Serial.println(F("Already Assoc"));
    }
  if(_debug) Serial.println(F("Connecting"));

  if (wifly.open(URL, PORT)) {
    if(reconnect){
      if(MODEL>0)colorLED(_red,_green,_blue,_white); // RGBW
      else colorLED(_red,_green,_blue); // RGB
    }else{
      if(MODEL>0)colorLED(255,255,255,255); // white is connected
      else colorLED(255,255,255); // white is connected
    }
    if(_debug) Serial.println(F("Connected"));

    //wifly.write(MAC); //how did this ever work?
    wifly.print("{\"mac\":\"");
    wifly.print(MAC);
    wifly.println("\"}");

    reconnect = false;
    connectTime = millis();
    lastHeartbeat = millis();
    return true;
  } 
  else {
    if(_debug) Serial.println(F("Failed to open"));
    return false;
  }
}

void Visualight::sendHeartbeat(){
  if(_debug) Serial.println(F("sending heartBeat"));
  wifly.print("{\"mac\":\"");
  wifly.print(MAC);
  wifly.println("\",\"h\":\"h\"}");
  lastHeartbeat = millis();
  if(_debug) Serial.print(F("Free memory: "));
  if(_debug) Serial.println(wifly.getFreeMemory(),DEC);
}


void Visualight::wifiReset(){
  wifly.close();
  if(_debug) Serial.println(F("Wifi RESET"));
  if(MODEL>0) colorLED(0,0,255,255);
  else colorLED(0,0,255);
  isServer = true;
  EEPROM.write(0, 1);
  wifly.reboot();
  wifly.setSoftAP();
}


void Visualight::processClient(){
  int available;

  if(millis()-lastHeartbeat > heartBeatInterval){
    sendHeartbeat();
  }

  if(millis()-connectTime > connectServerInterval){
    if(_debug) Serial.println(F("reconnect from timeout"));
    reconnect = true;
    //connectTime = millis();
    if(!connectToServer()){
      reconnectCount++;
    }
  }
  else {
    available = wifly.available();

    if (available < 0) {
      if(_debug)Serial.println(F("reconnect from available()"));
      if(!connectToServer()){
        reconnectCount++;
      }
    }
    else if(available > 0){
      connectTime = millis();
      char thisChar;
      thisChar = wifly.read();
      if( thisChar == 97){
        int numBytes = wifly.readBytesUntil('x', serBuf, 16);
        sscanf(serBuf,"%i,%i,%i,%i",&_red,&_green,&_blue,&_white);
        if(MODEL > 0) colorLED(_red,_green,_blue, _white);
        else colorLED(_red,_green,_blue);
        if(_debug){
          Serial.print("numBytes: ");
          Serial.println(numBytes);
          Serial.print("buf: ");
          Serial.println(serBuf);
          delay(5);
        }
        memset(serBuf,0,sizeof(serBuf));
      } else {
        if(_debug)Serial.println(thisChar);
        delay(5);
      }
    }
  }
}


/**********************************************************************************/
//------------------------------ BUTTON & LED METHODS ----------------------------//
/**********************************************************************************/

void Visualight::processButton(){
  resetButtonState = digitalRead(resetButton);
  if(resetButtonState == LOW){
    if(_debug) Serial.println(F("RESET WIFI"));
    wifiReset();
  }
}

void Visualight::colorLED(int red, int green, int blue){
  // if(sink){ /* keep just in case for future */
  //   red = 255-red;
  //   green = 255-green;
  //   blue = 255-blue;
  //   white = 255-white;
  // }
  analogWrite(redLED, red);
  analogWrite(greenLED, green);
  analogWrite(blueLED, blue);
}

void Visualight::colorLED(int red, int green, int blue, int white){
  analogWrite(redLED, red);
  analogWrite(greenLED, green);
  analogWrite(blueLED, blue);
  analogWrite(whiteLED, white);
}

void Visualight::fadeOn(){
    for(int fadeValue = 0 ; fadeValue <= 255; fadeValue +=5) { 
    // sets the value (range from 0 to 255):
    if(MODEL > 0) {
      colorLED(fadeValue, fadeValue, fadeValue, fadeValue);
    } else {
      colorLED(fadeValue,fadeValue,fadeValue);
    }
    delay(10);                            
  }
}

/****************************************************************************/
//------------------------ VISUALIGHT-AS-SERVER METHODS --------------------//
/****************************************************************************/

void Visualight::processServer() {
  if (wifly.available() > 0) {
    if (wifly.gets(buf, sizeof(buf))) { /* See if there is a request */
      if (strstr_P(buf, PSTR("GET /mac")) > 0) { /* GET request */
        if(_debug) Serial.println(F("Got GET MAC request"));
        while (wifly.gets(buf, sizeof(buf)) > 0) { /* Skip rest of request */
        }
        //int numNetworks = wifly.getNumNetworks();
        sendMac();
        wifly.flushRx();    // discard rest of input
        if(_debug) Serial.println(F("Sent Mac address"));
        wifly.close();
      } 
      else if (strstr_P(buf, PSTR("GET / ")) > 0) { // GET request
        if(_debug) Serial.println(F("Got GET request"));
        while (wifly.gets(buf, sizeof(buf)) > 0) { /* Skip rest of request */
        }
        //int numNetworks = wifly.getNumNetworks();
        wifly.flushRx();    // discard rest of input
        sendIndex();
        if(_debug) Serial.println(F("Sent index page"));
        wifly.close();
      } 
      
      else if (strstr_P(buf, PSTR("POST")) > 0) { /* Form POST */
        
        if(_debug) Serial.println(F("Got POST"));

        if (wifly.match(F("network="))) { /* Get posted field value */
          wifly.getsTerm(network, sizeof(network),'&');
          replaceAll(network,"+","$");
          if (wifly.match(F("password="))) {
            wifly.gets(password, sizeof(password));
            replaceAll(password,"+","$");
          }
          wifly.flushRx();    // discard rest of input
          if(_debug) Serial.print(F("Sent greeting page - Network: "));
          sendGreeting(network); //send greeting back
          delay(500);
          sendGreeting(network); //send a second time *just in case*
          delay(500);
          wifly.flushRx();
          joinWifi();
        }
      } 
      else { /* Unexpected request */
        delay(100);
        wifly.flushRx();    // discard rest of input
        if(_debug) Serial.println(F("Sending 404"));
        send404();
      }
    }
  }
}

void Visualight::sendMac() {
  /* Send the header direclty with print */
  wifly.println(F("HTTP/1.1 200 OK"));
  wifly.println(F("Connection: close"));
  wifly.println(F("Content-Type: application/json"));
  wifly.println(F("Transfer-Encoding: chunked"));
  wifly.println(F("Access-Control-Allow-Origin: *"));
  wifly.println();

  /* Send the body using the chunked protocol so the client knows when
   * the message is finished.
   * Note: we're not simply doing a close() because in version 2.32
   * firmware the close() does not work for client TCP streams.
   */
  wifly.sendChunk(F("{\"mac\":\""));
  wifly.sendChunk(MAC);
  wifly.sendChunk(F("\"}"));
  wifly.sendChunkln();
  wifly.sendChunkln();
  // enctype=\"text/plain\"
}
/** Send an index HTML page with an input box for a network name and password */
void Visualight::sendIndex() {
  /* Send the header direclty with print */
  wifly.println(F("HTTP/1.1 200 OK"));
  wifly.println(F("Connection: close"));
  wifly.println(F("Content-Type: text/html"));
  wifly.println(F("Transfer-Encoding: chunked"));
  wifly.println(F("Access-Control-Allow-Origin: *"));
  wifly.println();

  /* Send the body using the chunked protocol so the client knows when
   * the message is finished.
   * Note: we're not simply doing a close() because in version 2.32
   * firmware the close() does not work for client TCP streams.
   */
    wifly.sendChunkln(F("<html>"));
    wifly.sendChunkln(F("<head><title>Visualight Setup</title>"));
    wifly.sendChunkln(F("<style>body{width:100%;margin:0;padding:0;background:#999;font-family:sans-serif;}"));
    wifly.sendChunkln(F("h1,h3{margin:2% auto;width:80%;}div{margin: 10% auto 0;width:60%;padding:4%;background:#f9f9f9;border-radius:15px;}"));
    wifly.sendChunkln(F("input{display:block;width:80%;margin:4% auto;font-size:1.2em;padding:1%;background:#fff;}"));
    wifly.sendChunkln(F("select{display:block;width:80%;margin:4% auto;height:40px;font-size:1em;background:#fff;}"));
    wifly.sendChunkln(F("input[type=\"submit\"]{width:30%;padding:1%;background:#222;color:#ddd;border-radius:15px;}form{width:90%;margin:0 auto;}"));
    wifly.sendChunkln(F("</style></head>"));
    wifly.sendChunkln(F("<body><div>"));
    wifly.sendChunkln(F("<form name=\"input\" action=\"/\" method=\"post\">"));
    wifly.sendChunkln(F("<h1>Visualight</h1>"));
    wifly.sendChunkln(F("<h3>We&rsquo;ll need a little data to get started</h3>"));
    wifly.sendChunkln(F("<input type=\"text\" name=\"network\" placeholder=\"YOUR WIFI SSID\" />"));
    wifly.sendChunkln(F("<input type=\"text\" name=\"password\" placeholder=\"YOUR WIFI PASS\" />"));
    wifly.sendChunkln(F("<input type=\"submit\" value=\"Submit\" />"));
    wifly.sendChunkln(F("</form></div>")); 
    wifly.sendChunkln(F("</body></html>"));
    wifly.sendChunkln();
    wifly.sendChunkln();
  // enctype=\"text/plain\"
}

/** Send a greeting HTML page with the user's name and an analog reading */
void Visualight::sendGreeting(char *name) {
  /* Send the header directly with print */
  wifly.println(F("HTTP/1.1 200 OK"));
  wifly.println(F("Connection: close"));
  wifly.println(F("Content-Type: text/html"));
  wifly.println(F("Transfer-Encoding: chunked"));
  wifly.println(F("Access-Control-Allow-Origin: *"));
  wifly.println();

  /* Send the body using the chunked protocol so the client knows when
   * the message is finished.
   */
  wifly.sendChunkln(F("<html>"));
  wifly.sendChunkln(F("<title>Visualight Setup</title>"));
  /* No newlines on the next parts */
  wifly.sendChunk(F("<h1><p>Hello "));
  wifly.sendChunk(name);
  /* Finish the paragraph and heading */
  wifly.sendChunkln(F("</p></h1>"));
  wifly.sendChunkln(F("</html>"));
  wifly.sendChunkln();
  wifly.sendChunkln();
}

void Visualight::send404() { /** Send a 404 error */
  wifly.println(F("HTTP/1.1 404 Not Found"));
  wifly.println(F("Connection: close"));
  wifly.println(F("Content-Type: text/html"));
  wifly.println(F("Transfer-Encoding: chunked"));
  wifly.println(F("Access-Control-Allow-Origin: *"));
  wifly.println();
  wifly.sendChunkln(F("<html><head>"));
  wifly.sendChunkln(F("<title>404 Not Found</title>"));
  wifly.sendChunkln(F("</head><body>"));
  wifly.sendChunkln(F("<h1>Not Found</h1>"));
  wifly.sendChunkln(F("<hr>"));
  wifly.sendChunkln(F("</body></html>"));
  wifly.sendChunkln();
  wifly.sendChunkln();
}

void Visualight::replaceAll(char *buf_,const char *find_,const char *replace_) {
  char *pos;
  int replen,findlen;

  findlen=strlen(find_);
  replen=strlen(replace_);

  while((pos=strstr(buf_,find_))) {
    strncpy(pos,replace_,replen);
    strcpy(pos+replen,pos+findlen);
  }
}