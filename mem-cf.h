/*
IBEX UK LTD http://www.ibexuk.com
Electronic Product Design Specialists
RELEASED SOFTWARE

The MIT License (MIT)

Copyright (c) 2013, IBEX UK Ltd, http://ibexuk.com

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//Project Name:		COMPACT FLASH MEMORY CARD FAT16 & FAT 32 DRIVER
//CF CARD DRIVER C CODE HEADER FILE






//*****************************
//*****************************
//********** DEFINES **********
//*****************************
//*****************************
#ifndef CF_C_INIT		//Do only once the first time this file is used
#define	CF_C_INIT



//----------------------------------------------
//----- DEFINE TARGET COMPILER & PROCESSOR -----
//----------------------------------------------
//(ONLY 1 SHOULD BE INCLUDED, COMMENT OUT OTHERS - ALSO SET IN THE OTHER DRIVER .h FILE)
#define	FFS_USING_MICROCHIP_C18_COMPILER
//<< add other compiler types here



//----------------------
//----- IO DEFINES -----									//<<<<< CHECK FOR A NEW APPLICATION <<<<<
//----------------------

#ifdef FFS_USING_MICROCHIP_C18_COMPILER

//PORTS:-
#define FFS_DATA_BUS_IP				PORTD			//CF D7:0 data bus read register
#define FFS_DATA_BUS_OP				LATD			//CF D7:0 data bus write register (same as read register if microcontroller / processor doesn't have separate registers for input and output)
#define	FFS_DATA_BUS_TO_INPUTS		TRISD = 0xff	//CF D7:0 data bus input / output register (bit state 0 = output, 1 = input)
#define	FFS_DATA_BUS_TO_OUTPUTS		TRISD = 0x00

//CONTROL PINS:-
#define	FFS_CE						LATCbits.LATC2
#define	FFS_WE						LATCbits.LATC0
#define	FFS_OE						LATCbits.LATC1
#define	FFS_REG						LATBbits.LATB5		
#define	FFS_RDY						PORTBbits.RB4
#define	FFS_WAIT					PORTBbits.RB3

//ADDRESS PINS:-
#define	FFS_ADDRESS_REGISTER		LATE
#define	FFS_ADDRESS_BIT_0			0x01				//(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
#define	FFS_ADDRESS_BIT_1			0x02
#define	FFS_ADDRESS_BIT_2			0x04
//#define	FFS_ADDRESS_FUNCTION						//Optional function to call to output the FFS_ADDRESS_REGISTER.  Comment out if not requried

//RESET PIN (Defined like this so the pin can be connected to an output latch IC instead of directly to the processor if desired)
#define	FFS_RESET_PIN_REGISTER		LATA
#define	FFS_RESET_PIN_BIT			0x08				//(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
//#define	FFS_RESET_PIN_FUNCTION						//Optional function to call to output the FFS_RESET_PIN_REGISTER.  Comment out if not requried

//CARD DETECT PIN (Defined like this so the pin can be connected to an input latch IC instead of directly to the processor if desired)
#define	FFS_CD_PIN_REGISTER			PORTC
#define	FFS_CD_PIN_BIT				0x20				//(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02 or 0x01)
//#define	FFS_CD_PIN_FUNCTION							//Optional function to call to get the FFS_RESET_PIN_REGISTER.  Comment out if not requried



#define	FFS_DRIVER_GEN_512_BYTE_BUFFER	ffs_general_buffer		//This general buffer is used by routines and may be the same as the buffer that
																//the application uses to read and write data from and to the card if ram is limited
#endif		//#ifdef FFS_USING_MICROCHIP_C18_COMPILER






//----------------------------------------
//----- CF CARD SIGNAL DELAY DEFINES -----					//<<<<< CHECK FOR A NEW APPLICATION <<<<<
//----------------------------------------
//Depending on the speed of your processor set as many null operation instructions as are required based on the processor instruction execution time.
//When getting a new PCB to work it may be useful to add additional delays to the FFS_DELAY_FOR_WAIT_SIGNAL define as overkill and then once everything
//is working reduce the delay, to avoid problems of PCB designs that need more time for signals to stabilise causing the driver to not work.

#ifdef FFS_USING_MICROCHIP_C18_COMPILER

#define FFS_DELAY_FOR_WAIT_SIGNAL()		Nop(); Nop();						//Needs to be minimum 35nS (also can be used to slow down read and write access if necessary for PCB)
#define FFS_DELAY_FOR_RDY_SIGNAL()		Nop(); Nop(); Nop(); Nop();			//Needs to be minimum 400nS

#endif		//#ifdef FFS_USING_MICROCHIP_C18_COMPILER



//PROCESS CF CARD STATE MACHINE STATES
typedef enum _FFS_PROCESS_STATE
{
    FFS_PROCESS_NO_CARD,
    FFS_PROCESS_WAIT_FOR_CARD_FULLY_INSERTED,
    FFS_PROCESS_COMPLETE_RESET,
    FFS_PROCESS_WAIT_FOR_CARD_RESET,
    FFS_PROCESS_CARD_INITIALSIED
} FFS_PROCESS_STATE;


#endif




//*******************************
//*******************************
//********** FUNCTIONS **********
//*******************************
//*******************************
#ifdef CF_C
//-----------------------------------
//----- INTERNAL ONLY FUNCTIONS -----
//-----------------------------------



//-----------------------------------------
//----- INTERNAL & EXTERNAL FUNCTIONS -----
//-----------------------------------------
//(Also defined below as extern)
void ffs_process (void);
BYTE ffs_is_card_present (void);
void ffs_card_reset_pin (BYTE pin_state);
void ffs_read_sector_to_buffer (DWORD sector_lba);
void ffs_write_sector_from_buffer (DWORD sector_lba);
void ffs_set_address (BYTE address);
BYTE ffs_write_byte (BYTE data);
WORD ffs_read_word (void);
BYTE ffs_read_byte (void);




#else
//------------------------------
//----- EXTERNAL FUNCTIONS -----
//------------------------------
extern void ffs_process (void);
extern BYTE ffs_is_card_present (void);
extern void ffs_card_reset_pin (BYTE pin_state);
extern void ffs_read_sector_to_buffer (DWORD sector_lba);
extern void ffs_write_sector_from_buffer (DWORD sector_lba);
extern void ffs_set_address (BYTE address);
extern BYTE ffs_write_byte (BYTE data);
extern WORD ffs_read_word (void);
extern BYTE ffs_read_byte (void);



#endif




//****************************
//****************************
//********** MEMORY **********
//****************************
//****************************
#ifdef CF_C
//--------------------------------------------
//----- INTERNAL ONLY MEMORY DEFINITIONS -----
//--------------------------------------------
BYTE sm_ffs_process = FFS_PROCESS_NO_CARD;
WORD file_system_information_sector;
BYTE ffs_no_of_heads;
BYTE ffs_no_of_sectors_per_track;
DWORD ffs_no_of_partition_sectors;






//--------------------------------------------------
//----- INTERNAL & EXTERNAL MEMORY DEFINITIONS -----
//--------------------------------------------------
//(Also defined below as extern)
WORD number_of_root_directory_sectors;				//Only used by FAT16, 0 for FAT32
BYTE ffs_buffer_needs_writing_to_card;
DWORD ffs_buffer_contains_lba = 0xffffffff;
DWORD fat1_start_sector;
DWORD root_directory_start_sector_cluster;			//Start sector for FAT16, start clustor for FAT32
DWORD data_area_start_sector;
BYTE disk_is_fat_32;
BYTE sectors_per_cluster;
DWORD last_found_free_cluster;
DWORD sectors_per_fat;
BYTE active_fat_table_flags;
DWORD read_write_directory_last_lba;
WORD read_write_directory_last_entry;


#else
//---------------------------------------
//----- EXTERNAL MEMORY DEFINITIONS -----
//---------------------------------------
extern WORD number_of_root_directory_sectors;				//Only used by FAT16, 0 for FAT32
extern BYTE ffs_buffer_needs_writing_to_card;
extern DWORD ffs_buffer_contains_lba;
extern DWORD fat1_start_sector;
extern DWORD root_directory_start_sector_cluster;
extern DWORD data_area_start_sector;
extern BYTE disk_is_fat_32;
extern BYTE sectors_per_cluster;
extern DWORD last_found_free_cluster;
extern DWORD sectors_per_fat;
extern BYTE active_fat_table_flags;
extern DWORD read_write_directory_last_lba;
extern WORD read_write_directory_last_entry;



#endif


