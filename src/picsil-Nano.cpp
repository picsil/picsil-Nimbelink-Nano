//---------------------------------------------------------------------------------------------
//
// Library for Nimbelink Skywire Nano cellular modules.
//
// Copyright 2020 picsil LLC
//
// Copyright 2016-2018, M2M Solutions AB
// Written by Jonny Bergdahl, 2016-11-18
//
// Licensed under the MIT license, see the LICENSE.txt file.
//
////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <HardwareSerial.h>
#include "picsil-Nano.h"

NanoCellular::NanoCellular(int8_t powerPin, int8_t statusPin)
{
     _powerPin = powerPin;
    _statusPin = statusPin;

    if (_powerPin != NOT_A_PIN)
    {
        pinMode(_powerPin, OUTPUT);
        digitalWrite(powerPin, HIGH);
    }
    if (statusPin != NOT_A_PIN)
    {
        pinMode(_statusPin, INPUT);
    }   
}

bool NanoCellular::begin(HardwareSerial* uart)
{
    _uart = uart;		
    _uart->begin(115200);

    PN_DEBUG("Powering off module");
    setPower(false);
    PN_DEBUG("Powering on module");
    setPower(true);

    PN_DEBUG("Checking SIM card");
    if (!getSimPresent())
    {
        PN_ERROR("No SIM card detected");
        return false;
    }

    PN_DEBUG("Waiting for module initialization");
    uint32_t timeout = 5000;
    while (timeout > 0)            
    {
        if (readReply(500, 1) &&
            strstr(_buffer, "READY"))
        {
            PN_DEBUG("Module initialized");
            break;
        }
        callWatchdog();
        delay(500);
        timeout -= 500;
    }
    if (timeout <= 0)
    {
        // critical error
		PN_DEBUG("Failed to get READY from module");
        return false;
    }

    // Wait for network registration
    PN_DEBUG("Waiting for network registration");
    timeout = 60000;
 
    while (timeout > 0)            
    {
        if (readReply(500, 1) &&
            strstr(_buffer, "CONNECTED"))
        {
            PN_DEBUG("Network connected");
            break;
        }
        callWatchdog();
        delay(500);
        timeout -= 500;
    }
    if (timeout <= 0)
    {
        // critical error
		PN_DEBUG("Failed to connect to network (module still initialized)");
    }
 
    NetworkRegistrationState state;
    while (timeout > 0)
    {
        state = getNetworkRegistration();
        switch (state)
        {
            case NetworkRegistrationState::NotRegistered:
                PN_DEBUG("Not registered");
                break;
            case NetworkRegistrationState::Registered:
                PN_DEBUG("Registered");
                break;
            case NetworkRegistrationState::Searching:
                PN_DEBUG("Searching");
                break;
            case NetworkRegistrationState::Denied:
                PN_DEBUG("Denied");
                break;
            case NetworkRegistrationState::Unknown:
                PN_DEBUG("Unknown");
                break;
            case NetworkRegistrationState::Roaming:
                PN_DEBUG("Roaming");
                break;
        }
        callWatchdog();
        delay(500);
        timeout -= 500;
        if (state == NetworkRegistrationState::Registered ||
           state == NetworkRegistrationState::Roaming)
        {
            break;
        }
    }
    if (timeout <= 0)
    {
        PN_ERROR("Network registration failed");
        return false;
    }

    // Disable echo
    sendAndCheckReply("ATE0", _OK, 1000);
    // Set verbose error messages
    sendAndCheckReply("AT+CMEE=2", _OK, 1000);

    callWatchdog();
    return true;
}

uint8_t NanoCellular::getIMEI(char* buffer)
{
    if (sendAndWaitForReply("AT+CGSN=1"))
    {
        char buf[28];
        strncpy(buf, _buffer, 26);
        buf[26] = 0;
        strncpy(buffer, strchr(buf, '\"'), 15);
        return strlen(buffer);
    }
    return 0;
}

bool NanoCellular::getStatus()
{
    if (_statusPin == NOT_A_PIN)
    {
        return true;
    }
    return digitalRead(_statusPin) == HIGH;
}

void NanoCellular::setWatchdogCallback(WATCHDOG_CALLBACK_SIGNATURE)
{
    this->watchdogcallback = watchdogcallback;
}

void NanoCellular::flush()
{
    while (_uart->available())
    {
        _uart->read();
    }
}

const char* NanoCellular::getFirmwareVersion()
{
	return _firmwareVersion;
}

uint8_t NanoCellular::getOperatorId(char* buffer)
{
    // Reply is:
    // +COPS: 0,2,"311480",7
    // OK   
    if (sendAndWaitForReply("AT+COPS?", 1000, 3))
    {
        const char delimiter[] = ",";
        char* token = strtok(_buffer, delimiter);
        if (token)
        {
            token = strtok(nullptr, delimiter);
            token = strtok(nullptr, delimiter);
            if (token)
            {
                // Strip out the " characters                
                uint8_t len = strlen(token);
                strncpy(buffer, token + 1, len - 2);
                buffer[len - 2] = 0;        
                return strlen(buffer);
            }
        }
    }
    return 0;
}

uint8_t NanoCellular::getRSSI()
{
    // Reply is:
    // +CESQ: 99,99,255,255,16,47
    // OK
    if (sendAndWaitForReply("AT+CESQ", 1000, 3))
    {        
        char * token = strtok(_buffer, " ");
        if (token)
        {
            token = strtok(nullptr, ",");
            if (token)
            {
                for (int x = 0; x < 6; x++) {
                    if (token)
                    {
                        token = strtok(nullptr, ",");
                    }
                    else
                    {
                        return 0;
                    }
                }

                char* ptr;                
                return strtol(token, &ptr, 10);
            }
        }
    }
    return 0;
}

uint8_t NanoCellular::getSIMICCID(char* buffer)
{
    char delim[] = " ";
    // #ICCID: 898600220909A0206023
    // OK
    if (sendAndWaitForReply("AT#ICCID", 1000, 3))
    {
        char * token = strtok(_buffer, delim);
        if (token)
        {
            token = strtok(nullptr, delim);
            uint8_t len = strlen(token);
            strncpy(buffer, token, len + 1);
            return len;                        
        }
    }
    return 0;    
}

uint8_t NanoCellular::getSIMIMSI(char* buffer)
{
    char delim[] = "\n";
    // 240080007440698
    // OK
    if (sendAndWaitForReply("AT+CIMI", 1000, 3))
    {
        char * lf = strstr(_buffer, delim);
        if (lf)
        {
            uint8_t len = lf - _buffer;
            strncpy(buffer, _buffer, len);
            return len;                        
        }
    }
    return 0;    
}

double NanoCellular::getVoltage()
{
    // Reply is:
    // %XVBAT: 5059
    // OK
    if (sendAndWaitForReply("AT%XVBAT", 1000, 3))
    {
        const char delimiter[] = " ";
        char * token = strtok(_buffer, delimiter);        
        if (token)
        {
            token = strtok(nullptr, delimiter);
            if (token)
            {
                char* ptr;
                uint16_t milliVolts = strtol(token, &ptr, 10);
                return milliVolts / 1000.0;
            }
        }
    }
    return 0;
}

bool NanoCellular::disconnectNetwork()
{
    // PDP context 0 (default connection) can't be deactivated
    // Set to airplane mode instead
    if (!sendAndCheckReply("AT+CFUN=4", _OK, 30000))
    {
        PN_ERROR("Failed to set airplane mode.");
        return false;
    }
    return true;
}

bool NanoCellular::connectNetwork()
{
    if (!sendAndCheckReply("AT+CFUN=1", _OK, 30000))
    {
        PN_ERROR("Failed disable airplane mode.");
        return false;
    }
    return true;
}

bool NanoCellular::connectNetwork(const char* apn, const char* userId, const char* password)
{
    // First set up PDP context
    sprintf(_buffer, "AT+CGDCONT=0,\"IPV4V6\",\"%s\"", apn);
    if (!sendAndCheckReply(_buffer, _OK, 1000))
    {
        PN_ERROR("Failed to setup PDP context");
        return false;
    }
    callWatchdog();
    if (!sendAndCheckReply("AT+CFUN=1", _OK, 30000))
    {
        PN_ERROR("Failed disable airplane mode.");
        return false;
    }

    return true;
}

int NanoCellular::read()
{
    uint8_t buffer[2];
    read(buffer, 1);
    return buffer[0];
}

int NanoCellular::peek()
{
    // Not supported
    return 0;
}

int NanoCellular::read(uint8_t *buf, size_t size)
{
    if ((size == 0) || (_socket == 0))
    {
        return 0;
    }

    sprintf(_buffer, "AT#XTCPRECV=%i,%i,%i", _socket, size, SOCKET_TIMEOUT);
    if (sendAndWaitForReply(_buffer, 1000, 1) &&
        strstr(_buffer, "+QIRD:"))
    {        
        // +QIRD: <len>
        // <data>
        //
        // OK
        char* token = strtok(_buffer, " ");
        token = strtok(nullptr, "\n");
        char* ptr;
        uint16_t length = strtol(token, &ptr, 10);
        QT_COM_TRACE("Data len: %i", length);

        _uart->readBytes(buf, length);
        buf[length] = '\0';
        QT_COM_TRACE_START(" <- ");
        QT_COM_TRACE_ASCII(_buffer, size);
        QT_COM_TRACE_END("");
        return length;       
    }
    return 0;
}

int -NanoCellular::available()
{
    if (useEncryption())
    {
        if (sslLength > 0)
        {
            return sslLength;
        }
        sprintf(_buffer, "AT+QSSLRECV=1,%i", sizeof(_buffer) - 36);
        if (sendAndWaitForReply(_buffer, 1000, 3))
        {
            char* recvToken = strstr(_buffer, "+QSSLRECV: ");
			recvToken += 11;
			
            /* Get length of data */
            char* lfToken = strstr(recvToken, "\n");
			uint32_t llen = lfToken - recvToken;
			
			char numberStr[llen];
			strncpy(numberStr, recvToken, llen);
			numberStr[llen] = '\0';
			
			sslLength = atoi(numberStr);     

			if (sslLength > 0)
			{
                //set start of data to after the last new line
                char* data = lfToken + 1;

                //The check below is because sometimes a URC-message gets through and ruins everything because it includes 2 more lines that we don't expect
                //First cut off the message so we only check the first 30 characters
                
                //get first 30 characters
                char subbuff[30];
                memcpy( subbuff, _buffer, 30 );
                subbuff[30] = '\0';

                //A bad message can at a maximum look like this: +QSSLURC: "recv",1<LF><LF>+QSSLRECV: [number]<LF>
                //but sometimes only part of it is included, example: v",1<LF><LF>+ QSSLRECV:[number]<LF>
                //i check for 2 new lines before +QSSLRECV because if there only is 1 then the data will still be read
                if(strstr(subbuff, "\n\n+QSSLRE") != nullptr) {
                    if(readReply(1000, 2)) {
                        data = _buffer;
                    }
                    else {
                        QT_ERROR("Could not get data after URC-interrupt");

                        sslLength = 0;
                    }
                }

				memcpy(_readBuffer, data, sslLength);
			}
			QT_TRACE("available sslLength: %i", sslLength);
			return sslLength;
        }
    }
    else
    {
        sprintf(_buffer, "AT+QIRD=1,0");
        if (sendAndWaitForReply(_buffer, 1000, 3))
        {
            const char delimiter[] = ",";
            char * token = strtok(_buffer, delimiter);              
            if (token)
            {
                token = strtok(nullptr, delimiter);
                if (token)
                {
                    token = strtok(nullptr, delimiter);
                    if (token)
                    {
                        char* ptr;
                        uint16_t unread = strtol(token, &ptr, 10);
                        QT_COM_TRACE("Available: %i", unread);
                        return unread;
                    }
                }
            }        
        }
    }
    QT_COM_ERROR("Failed to read response");
    return 0;
}

//
// Private
//

void NanoCellular::callWatchdog()
{
    if (watchdogcallback != nullptr)
    {
        (watchdogcallback)();
    }
}

bool NanoCellular::setPower(bool state)
{
    uint32_t timeout;
	PN_DEBUG("setPower: %i", state);
    if (state == true)
    {
        if (_powerPin != NOT_A_PIN)
        {
            digitalWrite(_powerPin, LOW);
            delay(300);
            digitalWrite(_powerPin, HIGH);
        }

        PN_TRACE_START("Waiting for module");
        while (!getStatus())
        {
            PN_TRACE_PART(".");
            callWatchdog();
            delay(500);
        }
        PN_TRACE_END("");

        PN_DEBUG("Open communications");
        int32_t timeout = 7000;
        while (timeout > 0) 
        {
            flush();
            if (sendAndCheckReply(_AT, "AT", 1000))
            {
                PN_COM_TRACE("GOT AT");
                break;
            }
            callWatchdog();
            delay(500);
            timeout -= 500;
        }

        if (timeout < 0)
        {
            PN_ERROR("Failed to initialize cellular module");
            return false;
        }
    }
    else
    {
        if (!getStatus())
        {
            PN_COM_TRACE("Module already off");
            return true;
        }
        PN_DEBUG("Powering down module");        
        // First check if already awake
        timeout = 5000;
        while (timeout > 0) 
        {
            flush();
            if (sendAndCheckReply(_AT, "OK", 1000))
            {   
                PN_COM_TRACE("GOT AT");
                break;
            }
            if (sendAndCheckReply(_AT, "OK", 1000))
            {   
                PN_COM_TRACE("GOT OK");
                break;
            }            
            callWatchdog();
            delay(500);
            timeout -= 500;
        }
        sendAndCheckReply("ATE0", _OK, 1000);
		
        //if (!sendAndCheckReply("AT+QCFG=\"urc/port\",1,\"uart1\"", _OK))
		if (!sendAndCheckReply("AT#URC=\"LWM2M\",1", _OK))
		{
			PN_ERROR("Could not start LWM2M urc messages");
			return false;
		}
		if (!sendAndCheckReply("AT#URC=\"SOCK\",1", _OK))
		{
			PN_ERROR("Could not start SOCK urc messages");
			return false;
		}

        if (!sendAndCheckReply("AT#SHUTDOWN", _OK, 10000))
        {
            return false;
        }
        timeout = millis() + 60000;  // max 60 seconds for a shutdown
        while (timeout > millis())
        {
            if (readReply(1000, 1))
            {
                if (strstr(_buffer, "+SHUTDOWN"))
                {
                    if (_powerPin != NOT_A_PIN)
                    {
                        digitalWrite(_powerPin, LOW);
                    }
                    PN_DEBUG("Module powered down");

                    break;
                }
            }
            callWatchdog();
        }
        return false;
    }
    return true;
}

bool NanoCellular::getSimPresent()
{
    // state is 0 if UICC not initialized or 1 if UICC initiailized
    // Reply is:
    // %XSIM: <state>
    // OK
    if (sendAndWaitForReply("AT%XSIM?"))
    {
        const char delimiter[] = " ";
        char* token = strtok(_buffer, delimiter);
        token = strtok(nullptr, delimiter);
        if (token)
        {
            return token[0] == 0x31;
        }        
    }
    return false;
}

NetworkRegistrationState NanoCellular::getNetworkRegistration()
{
    if (sendAndWaitForReply("AT+CEREG?", 1000, 3))   
    {
        const char delimiter[] = ",";
        char * token = strtok(_buffer, delimiter);
        if (token)
        {
            token = strtok(nullptr, delimiter);
            if (token)
            {
                return (NetworkRegistrationState)(token[0] - 0x30);
            }
        }
    }
    return NetworkRegistrationState::Unknown;  
}

bool NanoCellular::sendAndWaitForMultilineReply(const char* command, uint8_t lines, uint16_t timeout)
{
	return sendAndWaitForReply(command, timeout, lines);
}

bool NanoCellular::sendAndWaitForReply(const char* command, uint16_t timeout, uint8_t lines)
{
    flush();
	PN_COM_TRACE(" -> %s", command);
    _uart->println(command);
    return readReply(timeout, lines);
}

bool NanoCellular::sendAndWaitFor(const char* command, const char* reply, uint16_t timeout)
{
    uint16_t index = 0;

    flush();
	PN_COM_TRACE(" -> %s", command);
    _uart->println(command);
    while (timeout--)
    {
        if (index > 254)
        {
            break;
        }
        while (_uart->available())
        {
            char c = _uart->read();
            if (c == '\r')
            {
                continue;
            }
            if (c == '\n' && index == 0)
            {
                // Ignore first \n.
                continue;
            }
            _buffer[index++] = c;
        }

        if (strstr(_buffer, reply))
        {
            PN_COM_TRACE("Match found");
            break;
        }
        if (timeout <= 0)
        {
            PN_COM_TRACE_START(" <- (Timeout) ");
            PN_COM_TRACE_ASCII(_buffer, index);
            PN_COM_TRACE_END("");
            return false;
        }
        callWatchdog();
        delay(1);
    }
    _buffer[index] = 0;
    PN_COM_TRACE_START(" <- ");
    PN_COM_TRACE_ASCII(_buffer, index);
    PN_COM_TRACE_END("");
    return true;
}

bool NanoCellular::sendAndCheckReply(const char* command, const char* reply, uint16_t timeout)
{
    sendAndWaitForReply(command, timeout);
    return (strstr(_buffer, reply) != nullptr);
}

bool NanoCellular::readReply(uint16_t timeout, uint8_t lines)
{
    uint16_t index = 0;
    uint16_t linesFound = 0;

    while (timeout--)
    {
        if (index > 254)
        {
            break;
        }
        while (_uart->available())
        {
            char c = _uart->read();
            if (c == '\r')
            {
                continue;
            }
            if (c == '\n' && index == 0)
            {
                // Ignore first \n.
                continue;
            }
            _buffer[index++] = c;
            if (c == '\n')
            {
				linesFound++;
            }
    		if (linesFound >= lines)
	    	{
    			break;
	    	}            
        }

   		if (linesFound >=lines)
    	{
   			break;
    	}            

        if (timeout <= 0)
        {
            PN_COM_TRACE_START(" <- (Timeout) ");
            PN_COM_TRACE_ASCII(_buffer, index);
            PN_COM_TRACE_END("");
            return false;
        }
        callWatchdog();
        delay(1);
    }
    _buffer[index] = 0;
    PN_COM_TRACE_START(" <- ");
    PN_COM_TRACE_ASCII(_buffer, index);
    PN_COM_TRACE_END("");
    return true;
}

bool NanoCellular::checkResult()
{   
    // CheckResult returns one of these:
    // true    OK
    // false   Unknown result
    // false  CME Error
    //
    char* token = strstr(_buffer, _OK);
    if (token)
    {
        //PN_TRACE("*OK - %s", _buffer);
        _lastError = 0;
        return true;
    }
    token = strstr(_buffer, _CME_ERROR);
    if (!token)
    {
        //PN_TRACE("*NO CME ERROR: %s", _buffer);
        _lastError = -1;
        return false;
    }
    //PN_TRACE("*CME ERROR: %s", _buffer);
    token = strtok(_buffer, _CME_ERROR);
    _lastError = atoi(token);
    return false;
}