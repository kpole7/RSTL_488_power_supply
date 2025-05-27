/*
 * FreeModbus Libary: Win32 Demo Application
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

 /**********************************************************
 *	Linux TCP support.
 *	Based on Walter's project.
 *	Modified by Steven Guo <gotop167@163.com>
 ***********************************************************/

// The file has been modified by K.O.

#include "modbusTcpSlave.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#include "mb.h"
#include "mbport.h"

// ----------------------- Global variables ---------------------------------

// This is a table of Modbus registers containing the description lengths of each power supply unit.
// These registers occupy addresses from TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS
// These registers are initialized once, at program startup, and remain constant thereafter
// View comments on ChannelDescriptionTextsPtr
uint16_t ChannelDescriptionLength[MAX_NUMBER_OF_SERIAL_PORTS];

// This is a table of pointers to texts that are the description of channels
// View comments on ChannelDescriptionTextsPtr
char* ChannelDescriptionTextsPtr[MAX_NUMBER_OF_SERIAL_PORTS];

// ChannelDescriptionPlainTextsPtr is a pointer to the allocated memory area used for the description of channels.
// Example
// An excerpt from the configuration file:
//
//id=13		port='/dev/ttyS0' 	opis='Magnes 1'
//id=0x66	port='/dev/ttyS1' 	opis='Magnes 2'
//id=51		port='/dev/ttyS2'	opis='Magnes 3'
//id=52		port='/dev/ttyS3'	opis='Magnes  4'
//id=102	port='/dev/ttyUSB0' opis='Magnes   5'
//id=53		port='/dev/ttyS4'	opis='Magnes 6'
//
// ChannelDescriptionLength[] = 10, 10, 10, 10, 12, 10, 0, 0, ...
// ChannelDescriptionPlainTextsPtr[] =
//	4D 61 67 6E 65 73 20 31 00 00
//	4D 61 67 6E 65 73 20 32 00 00
//	4D 61 67 6E 65 73 20 33 00 00
//	4D 61 67 6E 65 73 20 20 34 00
//	4D 61 67 6E 65 73 20 20 20 35 00 00
//	4D 61 67 6E 65 73 20 36 00 00
// For instance, in order to get description of the 3rd PSU (that is 'Magnes 3'),
// TCP client should send a request for 5 registers (that is ChannelDescriptionLength[2]/2)
// from address TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS + 3 * TCP_SERVER_DESCRIPTION_ADDRESS_STEP
char* ChannelDescriptionPlainTextsPtr;

// ----------------------- Constants ----------------------------------------

// The TCP server identification label includes the date and time of git last commit
// to ensure that both the TCP server and client are on the same version;
// for instance:
// const char TcpSlaveIdentifier[40] = "ID: git commit time 2025-05-13 15:25:52";
//                                      1234567890123456789012345678901234567890
#include "git_revision.cpp"

// ----------------------- Static variables ---------------------------------
static pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
static enum ThreadState
{
    STOPPED,
    RUNNING,
    SHUTDOWN
} ePollThreadState;

// ----------------------- Static functions ---------------------------------
static enum ThreadState eGetPollingThreadState( void );
static void eSetPollingThreadState( enum ThreadState eNewState );

// @brief This function is run as an additional thread (K.O. comment)
static void* pvPollingThread( void *pvParameter );

// ----------------------- Start implementation -----------------------------

// This function initializes TCP socket and starts additional thread for the Modbus TCP slave
char initializeModbusTcpSlave( void ){
    BOOL            bResult;
	pthread_t       xThread;
	uint16_t		J;

	assert( sizeof(TcpSlaveIdentifier)+2*sizeof(uint16_t) == MODBUS_TCP_SECTOR_SIZE * sizeof(uint16_t) );

	// The registers:
	// TableOfSharedDataForTcpServer[0][TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL]
	// TableOfSharedDataForTcpServer[0][TCP_SERVER_ADDRESS_NUMBER_OF_CHANNELS]
	// are set in the function configurationFileParsing()

	for (J = TCP_SERVER_ADDRESS_IDENTIFICATION_LABEL; J < MODBUS_TCP_SECTOR_SIZE; J++){
		TableOfSharedDataForTcpServer[0][J] = (TcpSlaveIdentifier[2*(J-TCP_SERVER_ADDRESS_IDENTIFICATION_LABEL)] << 8)
				+ TcpSlaveIdentifier[2*(J-TCP_SERVER_ADDRESS_IDENTIFICATION_LABEL)+1];
	}

    if( eMBTCPInit( TcpPortNumber ) != MB_ENOERR ){
        fprintf( stderr, "can't initialize modbus stack!\r\n" );
        return FALSE;
    }

    if( eGetPollingThreadState(  ) == STOPPED ){
        if( pthread_create( &xThread, NULL, pvPollingThread, NULL ) != 0 ){
            // Can't create the polling thread.
            bResult = FALSE;
        }
        else{
            bResult = TRUE;
        }
    }
    else{
        bResult = FALSE;
    }

    return bResult;
}

// @brief This function is run as an additional thread (K.O. comment)
static void* pvPollingThread( void *pvParameter )
{
    eSetPollingThreadState( RUNNING );

    if( eMBEnable(  ) == MB_ENOERR )
    {
        do
        {
            if( eMBPoll(  ) != MB_ENOERR )
                break;
        }
        while( eGetPollingThreadState(  ) != SHUTDOWN );
    }

    ( void )eMBDisable(  );

    eSetPollingThreadState( STOPPED );

    return 0;
}

static enum ThreadState eGetPollingThreadState(){
    enum ThreadState eCurState;

    ( void )pthread_mutex_lock( &xLock );
    eCurState = ePollThreadState;
    ( void )pthread_mutex_unlock( &xLock );

    return eCurState;
}

static void eSetPollingThreadState( enum ThreadState eNewState )
{
    ( void )pthread_mutex_lock( &xLock );
    ePollThreadState = eNewState;
    ( void )pthread_mutex_unlock( &xLock );
}

eMBErrorCode
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode )
{
    int             iRegIndex;
    int				Sector, Offset;

    usAddress--;

    if (usAddress < TCP_SERVER_START_ADDRESS){
    	// usAddress is below the address space range
    	return MB_ENOREG;
    }
    if (usAddress < TCP_SERVER_START_ADDRESS
    		+ TableOfSharedDataForTcpServer[0][TCP_SERVER_ADDRESS_NUMBER_OF_CHANNELS] * TCP_SERVER_SECTOR_ADDRESS_STEP
			+ MODBUS_TCP_SECTOR_SIZE )
    {
    	// usAddress is located in the address space range, which contains (NumberOfChannels+1) sectors

        usAddress -= TCP_SERVER_START_ADDRESS;
        Sector = usAddress / TCP_SERVER_SECTOR_ADDRESS_STEP;
        Offset = usAddress - Sector * TCP_SERVER_SECTOR_ADDRESS_STEP;

        if (Offset >= MODBUS_TCP_SECTOR_SIZE){
        	return MB_ENOREG;
        }
        if (usNRegs > Offset + MODBUS_TCP_SECTOR_SIZE){
        	return MB_ENOREG;
        }

        if (MB_REG_READ == eMode){
        	// This Modbus command is a valid request to read data from TableOfSharedDataForTcpServer

        	iRegIndex = 0;
    		while( usNRegs > 0 ){
    			*pucRegBuffer = ( UCHAR ) ( TableOfSharedDataForTcpServer[Sector][Offset+iRegIndex] >> 8 );
    			pucRegBuffer++;
    			*pucRegBuffer = ( UCHAR ) ( TableOfSharedDataForTcpServer[Sector][Offset+iRegIndex] & 0xFF );
    			pucRegBuffer++;
    			iRegIndex++;
    			usNRegs--;
    		}
    		return MB_ENOERR;
        }
        if (MB_REG_WRITE == eMode){

#if 0
			printf(" eMBRegHoldingCB; sect=%u; offs=%u; N= %u; D=%02X %02X\n", (unsigned)Sector, (unsigned)Offset,
					(unsigned)usNRegs, (unsigned)pucRegBuffer[0], (unsigned)pucRegBuffer[1] );
#endif

        	if (1 != usNRegs){
        		// the only write command supported is the single register write command;
        		// this should never happen
                return MB_ENOREG;
        	}
        	if (0 == Sector){
                return MB_ENOREG;
        	}
        	if ((MODBUS_TCP_ADDRESS_ORDER_CODE != Offset) && (MODBUS_TCP_ADDRESS_ORDER_VALUE != Offset)){
        		// There are only two read/write registers for each channel
                return MB_ENOREG;
        	}
        	// This Modbus command is a valid request to write data to TableOfSharedDataForTcpServer
   			TableOfSharedDataForTcpServer[Sector][Offset] = *pucRegBuffer << 8;
   			pucRegBuffer++;
   			TableOfSharedDataForTcpServer[Sector][Offset] |= *pucRegBuffer;

   			if (MODBUS_TCP_ADDRESS_ORDER_VALUE == Offset){
   				// Modbus TCP command to write the set-point value register is treated as an order for the lower layer
   				// to set the set-point
   				TableOfSharedDataForTcpServer[Sector][MODBUS_TCP_ADDRESS_ORDER_CODE] = RTU_ORDER_SET_VALUE;
   			}
    		return MB_ENOERR;
        }
        return MB_ENOREG;
    }
    else{
    	// usAddress is in a range where there can only be data structures related to power supply
    	// units descriptions;  this is read-only data

        if (MB_REG_READ != eMode){
        	return MB_ENOREG;
        }
        if (usAddress < TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS){
        	return MB_ENOREG;
    	}
    	if (usAddress+usNRegs <= TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS + MAX_NUMBER_OF_SERIAL_PORTS){
           	// This Modbus command is a valid request to read data from ChannelDescriptionLength
            usAddress -= TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS;

        	iRegIndex = 0;
    		while( usNRegs > 0 ){
    			*pucRegBuffer = ( UCHAR ) ( ChannelDescriptionLength[iRegIndex] >> 8 );
    			pucRegBuffer++;
    			*pucRegBuffer = ( UCHAR ) ( ChannelDescriptionLength[iRegIndex] & 0xFF );
    			pucRegBuffer++;
    			iRegIndex++;
    			usNRegs--;
    		}

    		return MB_ENOERR;
    	}
    	else{
    		// usAddress is in a range where there can only be texts of specific lengths

            if (usAddress < TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS + TCP_SERVER_DESCRIPTION_ADDRESS_STEP){
            	return MB_ENOREG;
        	}
            if (usAddress > TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS + TCP_SERVER_DESCRIPTION_ADDRESS_STEP * (MAX_NUMBER_OF_SERIAL_PORTS + 1)){
            	// rough check
            	return MB_ENOREG;
        	}
            usAddress -= TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS + TCP_SERVER_DESCRIPTION_ADDRESS_STEP;
            Sector = usAddress / TCP_SERVER_DESCRIPTION_ADDRESS_STEP;
            Offset = usAddress - Sector * TCP_SERVER_DESCRIPTION_ADDRESS_STEP;
            if (Sector >= MAX_NUMBER_OF_SERIAL_PORTS){
            	return MB_ENOREG;
            }
            if ( 2*(Offset + usNRegs) > ChannelDescriptionLength[Sector] ){
            	return MB_ENOREG;
            }
            if (NULL == ChannelDescriptionTextsPtr[Sector]){
            	// assertion; it should never happen
            	return MB_ENOREG;
            }
            // This Modbus command is a valid request to read a text
        	iRegIndex = 0;
    		while( usNRegs > 0 ){
    			*pucRegBuffer = ( UCHAR ) ((ChannelDescriptionTextsPtr[Sector])[2*iRegIndex]  );
    			pucRegBuffer++;
    			*pucRegBuffer = ( UCHAR ) ((ChannelDescriptionTextsPtr[Sector])[2*iRegIndex+1]);
    			pucRegBuffer++;
    			iRegIndex++;
    			usNRegs--;
    		}

    		return MB_ENOERR;
    	}
    }
    return MB_ENOREG;
}

// This function closes the open socket
void closeModbusTcpSlave( void ){
	( void )eMBClose();
}
