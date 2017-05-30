#include "slave_globals.h"
#include "Arduino.h"
#include "HardwareSerial.h"
#include "hasp_types.h"
#include "goat_funcs.h"
#include "goat_slave_funcs.h"

void setupSlaveSerials( void )
{
   //Serial to Master
   Serial.begin( 300 );
   while( !Serial );
}

void setupSlaveGlobals( void )
{
    memset( &reading.time[0], ' ', sizeof( reading ) - 4 );
    memset( &receive_buffer[0], 0, MAX_BUF );
    buffer_index = 0;

    assignEntry( reading.time, C_TIME(), sizeof( reading.time ) );
    assignEntry( reading.bank, "2", sizeof( reading.bank ) );
    assignEntry( reading.extt_reading, "DNA", sizeof( reading.extt_reading ) );
    assignEntry( reading.ext_humidity_reading, "DNA", sizeof( reading.ext_humidity_reading ) );
    assignEntry( reading.pump_status, "DNA", sizeof( reading.pump_status ) );
    assignEntry( reading.sd_status, "DNA", sizeof( reading.sd_status ) );
    assignEntry( reading.reading_status, "FIRST", sizeof( reading.reading_status ) );
}

void setupSlaveSensors( void )
{
    if( !bme.begin() )
        assignEntry( reading.temp_reading, "BFAIL", sizeof( reading.temp_reading ) );
    else
        assignEntry( reading.temp_reading, "BGOOD", sizeof( reading.temp_reading ) );
}
