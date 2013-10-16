/* Yanjun Luo, EEPROM operation for V400. */
//#include "v300config.h"
//#include "v300ctrldef.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#include "SUSI.h"

#define	EEPROM_I2C_ADDR	    0xAE

#define EEPROM_MAXSIZE      255

#define	VEEX_TEST_APP	1

int eeprom_open()
{
	int result;

//	eeprom_close();

        result = SusiDllInit();
        if (result == FALSE) {
                printf("SusiDllInit() failed\n");
                return (-1);
        }

        result = SusiSMBusAvailable();
        if (result == 0) {
                printf("SusiSMBusAvailable() failed\n");
                SusiDllUnInit();
                return (-1);
        }

	return 0;
}

int eeprom_close()
{
        int result;

        result = SusiDllUnInit();
        if (result == FALSE) {
                printf("SusiDllUnInit() failed\n");
                return (-1);
        }

	return 0;
}

int eeprom_read_byte(unsigned int addr, unsigned char* byte)
{
	int result;

        result = SusiSMBusReadByte(EEPROM_I2C_ADDR, addr, byte );
        if (result == FALSE) {
                printf("SusiSMBusReadByte() failed\n");
                return (-1);
        }

	return 0;
}

int eeprom_write_byte(unsigned int addr, unsigned char data)
{
        int result;

        result = SusiSMBusWriteByte(EEPROM_I2C_ADDR, addr, data);
        if (result == FALSE) {
                printf("SusiSMBusWriteByte() failed\n");
                return (-1);
        }

	usleep(10000);

	return 0;
}

int eeprom_read_buf(unsigned int start, unsigned int len, unsigned char* buf)
{
        int result;

	 if(len > EEPROM_MAXSIZE)
        {
        	len = EEPROM_MAXSIZE;
        }

	if((0 == start) && (EEPROM_MAXSIZE == len))
	{
	        result = SusiSMBusReadByteMulti(EEPROM_I2C_ADDR, start, buf, len);
	        if (result == FALSE) {
	                printf("SusiSMBusReadByteMulti() failed\n");
	                return (-1);
	        }
	}
	else
	{
		 unsigned char pBuf[EEPROM_MAXSIZE] = {'\0'};
	        result = SusiSMBusReadByteMulti(EEPROM_I2C_ADDR, 0, pBuf, EEPROM_MAXSIZE);
	        if (result == FALSE) {
	                printf("SusiSMBusReadByteMulti() failed\n");
	                return (-1);
	        }
		memcpy(buf,pBuf + start,len);
	}
	
	return 0;
}

 int eeprom_write_buf(unsigned int start, unsigned int len, unsigned char * buf)
{
        int result;

        if(len > EEPROM_MAXSIZE)
        {
        	len = EEPROM_MAXSIZE;
        }

	if((0 == start) && (EEPROM_MAXSIZE == len))
	{
	       result = SusiSMBusWriteByteMulti(EEPROM_I2C_ADDR, start, buf, len);
	}
	else
	{
		unsigned char pBuf[EEPROM_MAXSIZE] = {'\0'};
		result = SusiSMBusReadByteMulti(EEPROM_I2C_ADDR, 0, pBuf, EEPROM_MAXSIZE);
		if (result == FALSE) 
		{
	                printf("eeprom_write_buf: eeprom_read_buf() failed\n");
	                return (-1);
	        }	
		memcpy(pBuf + start,buf,len);
	       result = SusiSMBusWriteByteMulti(EEPROM_I2C_ADDR, 0, pBuf, EEPROM_MAXSIZE);
	}
		
        if (result == FALSE) 
	{
                printf("eeprom_write_buf: SusiSMBusWriteByteMulti() failed\n");
                return (-1);
        }

	return 0;	
}


