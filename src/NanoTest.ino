#include "picsil-Nano.h"
#include "M2M_Logger.h"

#define NOT_A_PIN -1

NanoCellular cell(2, NOT_A_PIN);
Logger logger;
char buf[255];

void setup()
{
    cell.begin(&Serial1);
    cell.getIMEI(buf);
    Serial.println(buf);
}

void loop()
{
    
}