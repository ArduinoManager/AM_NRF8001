/*
 *
 * AMController libraries, example sketches (“The Software”) and the related documentation (“The Documentation”) are supplied to you 
 * by the Author in consideration of your agreement to the following terms, and your use or installation of The Software and the use of The Documentation 
 * constitutes acceptance of these terms.  
 * If you do not agree with these terms, please do not use or install The Software.
 * The Author grants you a personal, non-exclusive license, under author's copyrights in this original software, to use The Software. 
 * Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by the Author, including but not limited to any 
 * patent rights that may be infringed by your derivative works or by other works in which The Software may be incorporated.
 * The Software and the Documentation are provided by the Author on an "AS IS" basis.  THE AUTHOR MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT 
 * LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE SOFTWARE OR ITS USE AND OPERATION 
 * ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 * REPRODUCTION AND MODIFICATION OF THE SOFTWARE AND OR OF THE DOCUMENTATION, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), 
 * STRICT LIABILITY OR OTHERWISE, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Author: Fabrizio Boco - fabboco@gmail.com
 *
 * All rights reserved
 *
 */

#include "AM_NRF8001.h"
#include <Adafruit_BLE_UART.h>

#define ADAFRUITBLE_REQ 10
#define ADAFRUITBLE_RDY 2     // This should be an interrupt pin, on Uno is #2 or #3
#define ADAFRUITBLE_RST 9

void eventCallback(aci_evt_opcode_t event);
void rxCallback(uint8_t *buffer, uint8_t len);

AMController *myGlobal;

Adafruit_BLE_UART BTLEserial = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

AMController::AMController( 
                             void (*doWork)(void), 
                             void (*doSync)(void),
                             void (*processIncomingMessages)(char *variable, char *value),
                             void (*processOutgoingMessages)(void),
                             void (*deviceConnected)(void),
							 							 void (*deviceDisconnected)(void)
                             ) {
    _doWork = doWork;
    _doSync = doSync;
    _processIncomingMessages = processIncomingMessages;
    _processOutgoingMessages = processOutgoingMessages;
    _deviceConnected = deviceConnected;
    _deviceDisconnected = deviceDisconnected;
    
    _remainBuffer[0] = '\0';

	_connected = false;  
	_initialized = false;
	
	BTLEserial.setACIcallback(&eventCallback);
	BTLEserial.setRXcallback(rxCallback);
	BTLEserial.setDeviceName("AMMgr"); /* 7 characters max! */
	
	myGlobal = this;
}                             

void AMController::loop() {
  	if (!_initialized) {
  		BTLEserial.begin();  	
  		_initialized = true;
	}  	  	
	
	BTLEserial.pollACI();
	
 	_doWork();
 	
 	BTLEserial.pollACI();
 	     	  	
   // Write outgoing messages
   if (_connected)
	   _processOutgoingMessages();   
}

void AMController::writeMessage(const char*variable, int value) {
	char buffer[VARIABLELEN+VALUELEN+3];
	snprintf(buffer,VARIABLELEN+VALUELEN+3, "%s=%d#", variable, value);     
  BTLEserial.write((uint8_t*)buffer, strlen(buffer)*sizeof(char));  
  BTLEserial.pollACI(); 
}

void AMController::writeMessage(const char*variable, float value) {
    char buffer[VARIABLELEN+VALUELEN+3];
    char vbuffer[VALUELEN];
    
    if (!_connected)
    	return;
            
    dtostrf(value, 0, 3, vbuffer);    
    snprintf(buffer,VARIABLELEN+VALUELEN+3, "%s=%s#", variable, vbuffer); 
    
    BTLEserial.write((uint8_t*)buffer, strlen(buffer)*sizeof(char));
    
    BTLEserial.pollACI(); 
}

void AMController::writeTripleMessage(const char*variable, float vX, float vY, float vZ) {

    char buffer[VARIABLELEN+VALUELEN+3];
    char vbufferAx[VALUELEN];
    char vbufferAy[VALUELEN];
    char vbufferAz[VALUELEN];
    
    if (!_connected)
    	return;

    dtostrf(vX, 0, 2, vbufferAx); 
    dtostrf(vY, 0, 2, vbufferAy); 
    dtostrf(vZ, 0, 2, vbufferAz);    
    snprintf(buffer,VARIABLELEN+VALUELEN+3, "%s=%s:%s:%s#", variable, vbufferAx,vbufferAy,vbufferAz); 
        
    BTLEserial.write((uint8_t*)buffer, strlen(buffer)*sizeof(char));
    
    BTLEserial.pollACI(); 
}

void AMController::writeTxtMessage(const char*variable, const char*value) {
    char buffer[128];    
        
    if (!_connected)
    	return;
    	    
    snprintf(buffer,128, "%s=%s#", variable, value);
    
    BTLEserial.write((uint8_t*)buffer, strlen(buffer)*sizeof(char));
    BTLEserial.pollACI(); 
}

void AMController::log(const char*msg) {
	this->writeTxtMessage("$D$",msg);
}

void AMController::log(int msg) {
	char buffer[11];
	snprintf(buffer,10, "%d", msg);
	
	this->writeTxtMessage("$D$",buffer);
}


void AMController::logLn(const char*msg) {
	this->writeTxtMessage("$DLN$",msg);
}

void AMController::logLn(int msg) {
	char buffer[11];
	snprintf(buffer,10, "%d", msg);
	
	this->writeTxtMessage("$DLN$",buffer);
}

void AMController::logLn(long msg) {
	char buffer[11];
	snprintf(buffer,10, "%l", msg);
	
	this->writeTxtMessage("$DLN$",buffer);	
}

void AMController::logLn(unsigned long msg) {
	char buffer[11];
	snprintf(buffer,10, "%l", msg);
	
	this->writeTxtMessage("$DLN$",buffer);
}

void AMController::temporaryDigitalWrite(uint8_t pin, uint8_t value, unsigned long ms) {

	boolean previousValue = digitalRead(pin);

	digitalWrite(pin, value);
	delay(ms);
	digitalWrite(pin, previousValue);
}

void AMController::connected() {

	_connected = true;
	
	if (_deviceConnected!=NULL)
		_deviceConnected();
}

void AMController::disconnected() {

	_connected = false;
	
	if (_deviceDisconnected!=NULL)
		_deviceDisconnected();
}

void AMController::processIncomingData(char *data, int len) {

	char		_variable[VARIABLELEN+1];
	char 	   	_value[VALUELEN+1];
	bool	   	_var = true;
	uint8_t		_idx = 0;
	uint8_t 	lastPound;

	_variable[0]='\0'; 
	_value[0]='\0';

	strncat(_remainBuffer,data,len);
	uint8_t l = strlen(_remainBuffer);
	
	Serial.print(">");
	Serial.print(_remainBuffer);
	Serial.println("<");
	
	BTLEserial.pollACI(); 

	for(uint8_t i=0; i<l; i++) {
	
		BTLEserial.pollACI(); 
		
		if (_var) {
		
			if (_remainBuffer[i] != '=') {
				_variable[_idx++] = _remainBuffer[i];
			} else {
				_variable[_idx] = '\0';
				_var = false;
				_idx = 0;
			}
		}
		else {
		
			if (_remainBuffer[i] != '#') {
				_value[_idx++] = _remainBuffer[i];
			}
			else {
			
				lastPound = i;
				_value[_idx] = '\0';
				_var = true;
				_idx = 0;
			
				if (strlen(_value)>0 && strcmp(_variable,"Sync") == 0) {                                
					// Process sync messages for the variable _value
					BTLEserial.pollACI(); 
					_doSync();
					BTLEserial.pollACI(); 
	      }
    	  else {
            
        	if (strlen(_variable) > 0 && strlen(_value) > 0) {                
						// Process incoming messages
						BTLEserial.pollACI(); 
						_processIncomingMessages(_variable,_value);
						BTLEserial.pollACI(); 
          } 
      	}
			}
		}
	}

	char tmp[128];
	if (lastPound==l-1){
		_remainBuffer[0]='\0';
	}
	else {
		strcpy(tmp,&_remainBuffer[lastPound+1]);
		strcpy(_remainBuffer,tmp);
	}
	
	_remainBuffer[0]='\0';
}
	
void eventCallback(aci_evt_opcode_t event) {
  
    if (event == ACI_EVT_DEVICE_STARTED) {
        Serial.println(F("* Advertising started!"));
    }
    
    if (event == ACI_EVT_CONNECTED) {
			myGlobal->connected();
    }
    
    if (event == ACI_EVT_DISCONNECTED) {
    	myGlobal->disconnected();
    	delay(150);
    }  
}
	
void rxCallback(uint8_t *buffer, uint8_t len) {
	myGlobal->processIncomingData((char *)buffer,len);
	BTLEserial.pollACI();
}