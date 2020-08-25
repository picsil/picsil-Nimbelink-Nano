#ifndef __picsil_Nano_h__
#define __picsil_Nano_h__
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <HardwareSerial.h>
#include <M2M_Logger.h>

#define NOT_A_PIN   -1
#define FLASHSTR	__FlashStringHelper*
#define PICSIL_NANO_DEBUG
#define PICSIL_NANO_COM_DEBUG

#ifdef PICSIL_NANO_DEBUG
#define PN_ERROR(...) if (_logger != nullptr) _logger->error(__VA_ARGS__)
#define PN_INFO(...) if (_logger != nullptr) _logger->info(__VA_ARGS__)
#define PN_DEBUG(...) if (_logger != nullptr) _logger->debug(__VA_ARGS__)
#define PN_TRACE(...) if (_logger != nullptr) _logger->trace(__VA_ARGS__)
#define PN_TRACE_START(...) if (_logger != nullptr) _logger->traceStart(__VA_ARGS__)
#define PN_TRACE_PART(...) if (_logger != nullptr) _logger->tracePart(__VA_ARGS__)
#define PN_TRACE_END(...) if (_logger != nullptr) _logger->traceEnd(__VA_ARGS__)
#else
#define PN_ERROR(...)
#define PN_INFO(...)
#define PN_DEBUG(...)
#define PN_TRACE(...)
#define PN_TRACE_START(...)
#define PN_TRACE_PART(...)
#define PN_TRACE_END(...)
#endif
#ifdef PICSIL_NANO_COM_DEBUG
#define PN_COM_ERROR(...) if (_logger != nullptr) _logger->error(__VA_ARGS__)
#define PN_COM_INFO(...) if (_logger != nullptr) _logger->info(__VA_ARGS__)
#define PN_COM_DEBUG(...) if (_logger != nullptr) _logger->debug(__VA_ARGS__)
#define PN_COM_TRACE(...) if (_logger != nullptr) _logger->trace(__VA_ARGS__)
#define PN_COM_TRACE_START(...) if (_logger != nullptr) _logger->traceStart(__VA_ARGS__)
#define PN_COM_TRACE_PART(...) if (_logger != nullptr) _logger->tracePart(__VA_ARGS__)
#define PN_COM_TRACE_END(...) if (_logger != nullptr) _logger->traceEnd(__VA_ARGS__)
#define PN_COM_TRACE_BUFFER(buffer, size) if (_logger != nullptr) _logger->tracePartHexDump(buffer, size)
#define PN_COM_TRACE_ASCII(buffer, size) if (_logger != nullptr) _logger->tracePartAsciiDump(buffer, size)
#else
#define PN_COM_ERROR(...)
#define PN_COM_INFO(...)
#define PN_COM_DEBUG(...)
#define PN_COM_TRACE(...)
#define PN_COM_TRACE_START(...)
#define PN_COM_TRACE_PART(...)
#define PN_COM_TRACE_END(...)
#define PN_COM_TRACE_BUFFER(buffer, size)
#define PN_COM_TRACE_ASCII(buffer, size)
#endif

enum class NetworkRegistrationState : uint8_t
{
    NotRegistered = 0,
    Registered,
    Searching,
    Denied,
    Unknown,
    Roaming
};

enum class TlsEncryption : uint8_t
{
    None = 0,
    Ssl30,
    Tls10,
    Tls11,
    Tls12,
    All
};

#define FILE_HANDLE         uint32_t
#define NOT_A_FILE_HANDLE   -1
#define SOCKET_TIMEOUT      1

#define WATCHDOG_CALLBACK_SIGNATURE void (*watchdogcallback)()

class NanoCellular : public Client
{
public:
    NanoCellular(int8_t powerPin = NOT_A_PIN, int8_t statusPin = NOT_A_PIN);

    bool begin(HardwareSerial* uart);

	// Logging
	void setLogger(Logger* logger);

	bool setPower(bool state);
    bool getStatus();    

    //SSL
    bool setEncryption(TlsEncryption enc);

    int8_t getLastError();

    bool getSimPresent();
	const char* getFirmwareVersion();
    uint8_t getIMEI(char* buffer);
    uint8_t getOperatorId(char* buffer);
    NetworkRegistrationState getNetworkRegistration();
    uint8_t getRSSI();
    uint8_t getSIMICCID(char* buffer);
    uint8_t getSIMIMSI(char* buffer);
    double getVoltage();

    bool connectNetwork(const char* apn, const char* userid, const char* password);
    bool connectNetwork();
    bool disconnectNetwork();

    // HTTP client interface
//   bool httpGet(const char* url, const char* fileName);

    // TCP Client interface
//    int connect(IPAddress ip, uint16_t port);
//    int connect(const char *host, uint16_t port);
//    int connect(IPAddress ip, uint16_t port, TlsEncryption encryption);
//    int connect(const char *host, uint16_t port, TlsEncryption encryption);
    size_t write(uint8_t);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    int read();
    int read(uint8_t *buf, size_t size);
    int peek();
    void flush();
//    void stop();
//    uint8_t connected();
//    operator bool()
//    {
//        return connected();
//    }

    // File client interface
//    FILE_HANDLE openFile(const char* fileName, bool overWrite = false);
//    bool readFile(FILE_HANDLE fileHandle, uint8_t* buffer, uint32_t length);
//    bool writeFile(FILE_HANDLE fileHandle, const uint8_t* buffer, uint32_t length);
//    bool seekFile(FILE_HANDLE fileHandle, uint32_t length);
//    uint32_t getFilePosition(FILE_HANDLE fileHandle);
//    bool truncateFile(FILE_HANDLE fileHandle);
//    bool closeFile(FILE_HANDLE fileHandle);

//    bool uploadFile(const char* fileName, const uint8_t* buffer, uint32_t length);
//    bool downloadFile(const char* fileName, uint8_t* buffer, uint32_t length);
//    uint32_t getFileSize(const char* fileName);
//    bool deleteFile(const char* fileName);

    // Callbacks
    void setWatchdogCallback(WATCHDOG_CALLBACK_SIGNATURE);

private:
//    bool activateSsl();
//    bool useEncryption();
	bool sendAndWaitForReply(const char* command, uint16_t timeout = 1000, uint8_t lines = 1);
	bool sendAndWaitForMultilineReply(const char* command, uint8_t lines, uint16_t timeout = 1000);
    bool sendAndWaitFor(const char* command, const char* reply, uint16_t timeout);   
	bool sendAndCheckReply(const char* command, const char* reply, uint16_t timeout = 1000);
    bool readReply(uint16_t timeout = 1000, uint8_t lines = 1);
    bool checkResult();
    void callWatchdog();

    int8_t _powerPin;
    int8_t _statusPin;
    int8_t _lastError = 0;
    uint32_t sslLength;
    HardwareSerial* _uart;
    Logger* _logger;
    uint16_t _socket = 0;
    char _buffer[255];
    char _readBuffer[255];
    char _command[32];
	char _firmwareVersion[20];
    WATCHDOG_CALLBACK_SIGNATURE;
    TlsEncryption _encryption;

    boolean httpsredirect;
    const char* _useragent = "PP";
	const char* _AT = "AT";
    const char* _OK = "OK";
    const char* _ERROR = "ERROR";
    const char* _CONNECT = "CONNECT";
    const char* _CME_ERROR = "CME ERROR: ";
    const char* _INET_PREFIX = "I";
    const char* _SSL_PREFIX = "SSL";
};

#endif