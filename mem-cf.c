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
//CF CARD DRIVER C CODE FILE




#include "main.h"					//Global data type definitions (see https://github.com/ibexuk/C_Generic_Header_File )
#define CF_C
#include "mem-cf.h"

#include "mem-ffs.h"






//************************************************************
//************************************************************
//********** DO BACKGROUND COMPACT FLASH CARD TASKS **********
//************************************************************
//************************************************************
//This function needs to be called reguarly to detect a new card being inserted so that it can be initialised ready for access.
void ffs_process (void)
{
	BYTE head;
	WORD cylinder_no;
	BYTE sector;

	BYTE b_temp;
	WORD w_temp;
	DWORD dw_temp;
	DWORD lba;
	DWORD main_partition_start_sector;
	WORD number_of_reserved_sectors;
	BYTE number_of_copies_of_fat;
	BYTE *buffer_pointer;



	switch (sm_ffs_process)
	{
	case FFS_PROCESS_NO_CARD:
		//-------------------------------
		//----- NO CARD IS INSERTED -----
		//-------------------------------
		FFS_CE = 1;
		FFS_WE = 1;
		FFS_OE = 1;
		
		ffs_card_ok = 0;					//Flag that card not OK

		//Reset all file handlers
		for (b_temp = 0; b_temp < FFS_FOPEN_MAX; b_temp++)
			ffs_file[b_temp].flags.bits.file_is_open = 0;

		//Has a card has been inserted?
		if (ffs_is_card_present() == 0)
			return;

		//A card has been inserted
		//Pause for 500mS seconds
		ffs_10ms_timer = 50;
		sm_ffs_process = FFS_PROCESS_WAIT_FOR_CARD_FULLY_INSERTED;

		return;


    case FFS_PROCESS_WAIT_FOR_CARD_FULLY_INSERTED:
		//------------------------------------------------------------
		//----- CARD INSERTED - WAIT FOR IT TO BE FULLY INSERTED -----
		//------------------------------------------------------------
		//(To allow for users that don't insert the card in one nice quick movement)

		//Ensure card is still inserted
		if (ffs_is_card_present() == 0)
		{
			sm_ffs_process = FFS_PROCESS_NO_CARD;
			return;
		}

		//Wait for timer to expire
		if (ffs_10ms_timer)
			return;
		
		//Card has been inserted for 500ms - reset the card
		ffs_card_reset_pin(1);
		FFS_REG = 1;						//Set reg pin high

		ffs_10ms_timer = 2;				//(Actually need about 4mS, but set to 2 to ensure that at least 10mS passes from this timer)
		sm_ffs_process = FFS_PROCESS_COMPLETE_RESET;

		return;


	case FFS_PROCESS_COMPLETE_RESET:
		//------------------------------
		//----- COMPLETE THE RESET -----
		//------------------------------

		//Wait for timer to expire
		if (ffs_10ms_timer)
			return;

		//Bring the card out of reset
		ffs_card_reset_pin(0);

		//Pause 500mS for card to be ready
		ffs_10ms_timer = 50;
		sm_ffs_process = FFS_PROCESS_WAIT_FOR_CARD_RESET;
		return;


    case FFS_PROCESS_WAIT_FOR_CARD_RESET:
		//------------------------------------------------------
		//----- CARD INSERTED - WAIT FOR RESET TO COMPLETE -----
		//------------------------------------------------------

		//Wait for timer to expire
		if (ffs_10ms_timer)
			return;

		//Ensure card is still inserted
		if (ffs_is_card_present() == 0)
		{
			sm_ffs_process = FFS_PROCESS_NO_CARD;
			return;
		}

		//Initialise the card
		sm_ffs_process = FFS_PROCESS_CARD_INITIALSIED;

		//Actually exit this switch statement to run the card initialise procedure below (this is the only state that doesn't return)
		break;

    case FFS_PROCESS_CARD_INITIALSIED:
		//-----------------------------------------
		//----- CARD INSERTED AND INITIALSIED -----
		//-----------------------------------------

		//If card is still inserted then exit
		if (ffs_is_card_present())
			return;

		//CARD HAS BEEN REMOVED
		ffs_card_ok = 0;
		
		sm_ffs_process = FFS_PROCESS_NO_CARD;

		return;


	default:
		sm_ffs_process = FFS_PROCESS_NO_CARD;
		return;
	}

	//---------------------------------------------
	//---------------------------------------------
	//----- INITIALISE NEW COMPACT FLASH CARD -----
	//---------------------------------------------
	//---------------------------------------------
	//(The only state that exits the switch statement above is FFS_PROCESS_WAIT_FOR_CARD_RESET when it completes)

	ffs_card_ok = 0;					//Default to card not OK


	//---------------------------------------------------
	//----- READ CARD SETUP (CF Identify Drive Cmd) -----
	//---------------------------------------------------
	FFS_CE = 0;								//Select the card

	//Write the 'Select Card/Head' register [LBA27:24 - bits3:0]
	ffs_set_address(0x06);
	if (ffs_write_byte(0x00) == 0)
		goto init_new_ffs_card_fail;

	//Select the command register
	ffs_set_address(0x07);
	
	//Send 'Identify Drive' instruction
	if (ffs_write_byte(0xec) == 0)
		goto init_new_ffs_card_fail;	

	//Read from the data register
	ffs_set_address(0x00);

	//CHECK GENERAL CONFIGURATION WORD (0X848A = CF CARD)
	w_temp = ffs_read_word();

	if (w_temp != 0x848a)
		goto init_new_ffs_card_fail;

	//Dump the next 4 words
	ffs_read_word();
	ffs_read_word();
	ffs_read_word();
	ffs_read_word();
	
	//Get number of unformatted bytes per sector (word 5)
	//(Dumped as this doesn't necessarily match the value in the boot record - we use the boot record value)
	ffs_read_word();

	//Dump the next word
	ffs_read_word();

	//Get number of sectors on the card (= the first invalid LBA address) (words 7&8) - not currently useful as we don't format a card
	ffs_read_word();
	ffs_read_word();

	//Dump the next 12 words
	for (b_temp = 0; b_temp < 12; b_temp++)
		ffs_read_word();

	//Get the buffer size (x512 bytes (NOT WORDS)) (word 21)
	ffs_read_word();

	//Dump the next 33 words
	for (b_temp = 0; b_temp < 33; b_temp++)
		ffs_read_word();

	//Get the number of heads (word 55)
	w_temp = ffs_read_word();
	if (w_temp > 0xff)						//Can't be more than 255 heads
		goto init_new_ffs_card_fail;
	ffs_no_of_heads = (BYTE)w_temp;

	//Get the number of sectors per track (word 56)
	w_temp = ffs_read_word();
	if (w_temp > 0xff)						//Can't be more than 64 sectors per track
		goto init_new_ffs_card_fail;
	ffs_no_of_sectors_per_track = (BYTE)w_temp;

	//Set the number of bytes per sector before we move on to general access
	ffs_bytes_per_sector = 512;

	//---------------------------------------
	//----- READ THE MASTER BOOT RECORD -----
	//---------------------------------------
	// read sector 1 (LBA 0x00)
	ffs_set_address(0x06);					//Write the 'Select Card/Head' register [LBA27:24 - bits3:0]
	ffs_write_byte(0xe0);					//(Use Logic Block Addressing - not Cylinder,Head,Sector)

	ffs_set_address(0x05);					//Write the 'Cylinder High' register [LBA23:16]
	ffs_write_byte(0x00);

	ffs_set_address(0x04);					//Write the 'Cylinder Low' register [LBA15:8]
	ffs_write_byte(0x00);

	ffs_set_address(0x03);					//Write the 'Sector No' register [LBA7:0]
	ffs_write_byte(0x00);

	ffs_set_address(0x02);					//Write the 'Sector Count' register (no of sectors to be transfered to complete the operation)
	ffs_write_byte(0x01);

	ffs_set_address(0x07);					//Write the 'Command' register
	ffs_write_byte(0x20);					//Read sector(s) command

	ffs_set_address(0x00);					//Read the data register

	//Read and dump the first 223 words (446 bytes of boot up executable code))
	for (b_temp = 0; b_temp < 223; b_temp++)
		ffs_read_word();

	//Now at start of the partition table [0x000001be]

	//Check for Partition 1 active (0x00 = inactive, 0x80 = active) [1]
	//(We allow a value of 0x00 for partition 1 as this partition must be present and on some disks a value of 0x00 has been found)
	b_temp = ffs_read_byte();
	//if (b_temp != 0x80)
	//	goto init_new_ffs_card_fail

	//Get 'Beginning of Partition - Head' [0x000001bf]
	head = ffs_read_byte();


	//Get 'Beginning of Partition - Cylinder + Sector' [0x000001c0]
	w_temp = ffs_read_word();

	cylinder_no = (w_temp >> 8);				//Yes this is correct - strange bit layout in this register!
	if (w_temp & 0x0040)
		cylinder_no += 0x0100;
	if (w_temp & 0x0080)
		cylinder_no += 0x0200;

	sector = (BYTE)(w_temp & 0x003f);

	//----- GET START ADDRESS OF PARTITION 1 -----
	//(Sectors per track x no of heads)
	w_temp = ((WORD)ffs_no_of_sectors_per_track * (WORD)ffs_no_of_heads);

	//Result x cylinder value just read
	main_partition_start_sector = ((DWORD)w_temp * (DWORD)cylinder_no);

	//Add (sectors per track x head value just read)
	w_temp = ((WORD)ffs_no_of_sectors_per_track * (WORD)head);
	main_partition_start_sector += (DWORD)w_temp;
	
	//Add sector value -1 (as sectors are numbered 1-#)
	main_partition_start_sector += (DWORD)(sector - 1);

	//WE NOW HAVE THE START ADDRESS OF THE FIRST PARTITION (THE ONLY PARTITION WE LOOK AT)


	//Read the 'Type Of Partition' [0x000001c2]
	//(We accept FAT16 or FAT32)
	b_temp = ffs_read_byte();

	if (b_temp == 0x04)						//FAT16 (smaller than 32MB)
		disk_is_fat_32 = 0;
	else if (b_temp == 0x06)				//FAT16 (larger than 32MB)
		disk_is_fat_32 = 0;
	else if (b_temp == 0x0b)				//FAT32 (Partition Up to 2048GB)
		disk_is_fat_32 = 1;
	else if (b_temp == 0x0c)				//FAT32 (Partition Up to 2048GB - uses 13h extensions)
		disk_is_fat_32 = 1;
	else if (b_temp == 0x0e)				//FAT16 (partition larger than 32MB, uses 13h extensions)
		disk_is_fat_32 = 0;
	else
		goto init_new_ffs_card_fail;

	//Get end of partition - head [0x000001c3]
	ffs_read_byte();

	//Get end of partition - Cylinder & Sector [0x000001c4]
	ffs_read_word();

	//Get no of sectors between MBR and the first sector in the partition [0x000001c6]
	ffs_read_word();
	ffs_read_word();

	//Get no of sectors in the partition - could be useful when we do writing of files [0x000001ca]
	ffs_no_of_partition_sectors = (DWORD)(ffs_read_word());
	ffs_no_of_partition_sectors += (((DWORD)ffs_read_word() << 16));



	//------------------------------------------
	//---- READ THE PARTITION 1 BOOT RECORD ----
	//------------------------------------------
	//Setup for finding the FAT1 table start address root directory start address and data area start address
	lba = main_partition_start_sector;

	ffs_read_sector_to_buffer(lba);
	buffer_pointer = &FFS_DRIVER_GEN_512_BYTE_BUFFER[0];

	//Dump jump code & OEM name (11 bytes)
	buffer_pointer += 11;

	//Get 'Bytes Per Sector' [# + 0x000b]
	//Check value matches value read from 'Identify Drive' - Changed - use this value as ID value can be different (it = no of Unfotmatted bytes per sector)
	//Value is usually 512, but can be 256 on some older CF cards.  Ensure <= 512 (this is the size of the sector read buffer we provide and it should not be bigger than this)
	ffs_bytes_per_sector = (WORD)*buffer_pointer++;
	ffs_bytes_per_sector |= (WORD)(*buffer_pointer++) << 8;
	if (ffs_bytes_per_sector > 512)
		goto init_new_ffs_card_fail;

	//Get 'Sectors Per Cluster' [# + 0x000d]
	//(Restricted to powers of 2 (1, 2, 4, 8, 16, 32…))
	sectors_per_cluster = *buffer_pointer++;
	
	b_temp = 0;											//Check its power of 2 (other functions rely on this check)
	for (w_temp = 0x01; w_temp < 0x0100; w_temp <<= 1)
	{
		if (sectors_per_cluster & (BYTE)w_temp)
			b_temp++;
	}
	if (b_temp != 1)
		goto init_new_ffs_card_fail;


	//Get '# of reserved sectors' [# + 0x000e]
	//Adjust the start addresses acordingly
	number_of_reserved_sectors = (WORD)*buffer_pointer++;
	number_of_reserved_sectors |= (WORD)(*buffer_pointer++) << 8;

	//Get 'no of copies of FAT' [# + 0x0010]
	//(any number >= 1 is permitted, but no value other than 2 is recomended)
	number_of_copies_of_fat = *buffer_pointer++;
	if ((number_of_copies_of_fat > 4) || (number_of_copies_of_fat == 0))		//We set a limit on there being a maximum of 4 copies of fat
		goto init_new_ffs_card_fail;

	//Get 'max root directory entries' [# + 0x0011]
	//(Used by FAT16, but not for FAT32)
	dw_temp = (DWORD)*buffer_pointer++;
	dw_temp |= (DWORD)(*buffer_pointer++) << 8;
	number_of_root_directory_sectors = ((dw_temp * 32) + (DWORD)(ffs_bytes_per_sector - 1)) / ffs_bytes_per_sector;		//Multiply no of entries by 32 (no of bytes per entry)
																														//This calculation rounds up
	//Get 'number of sectors in partition < 32MB' [# + 0x0013]
	//(Dump)
	buffer_pointer++;
	buffer_pointer++;

	//Get 'media descriptor' [# + 0x0015]
	//(Should be 0xF8 for hard disk)
	if (*buffer_pointer++ != 0xf8)
		goto init_new_ffs_card_fail;

	//Get 'sectors per fat'  [# + 0x0016]
	//(Used by FAT16, but not for FAT32 - for FAT32 the value is a double word and located later on in this table - variable will be overwritten)
	sectors_per_fat = (DWORD)*buffer_pointer++;
	sectors_per_fat |= (DWORD)(*buffer_pointer++) << 8;


	if(disk_is_fat_32 == 0)
	{
		//---------------------------------------------------------------------------------------
		//----- PARTITION IS FAT 16 - COMPLETE BOOT RECORD & INITAILISATION FOR THIS SYSTEM -----
		//---------------------------------------------------------------------------------------

		//CALCULATE THE PARTITION AREAS START ADDRESSES
		fat1_start_sector = main_partition_start_sector + (DWORD)number_of_reserved_sectors;
		root_directory_start_sector_cluster = main_partition_start_sector + (DWORD)number_of_reserved_sectors + (sectors_per_fat * number_of_copies_of_fat);
		data_area_start_sector = main_partition_start_sector + (DWORD)number_of_reserved_sectors + (sectors_per_fat * number_of_copies_of_fat) + number_of_root_directory_sectors;

		//SET THE ACTIVE FAT TABLE FLAGS
		active_fat_table_flags = 0;						// #|#|#|#|USE_FAT_TABLE_3|USE_FAT_TABLE_2|USE_FAT_TABLE_1|USE_FAT_TABLE_0
		for (b_temp = 0; b_temp < number_of_copies_of_fat; b_temp++)
		{
			active_fat_table_flags <<= 1;
			active_fat_table_flags++;
		}
		
		//SET UNUSED REGISTERS (used for FAT32)
		file_system_information_sector = 0xffff;

	}
	else
	{
		//---------------------------------------------------------------------------------------
		//----- PARTITION IS FAT 32 - COMPLETE BOOT RECORD & INITAILISATION FOR THIS SYSTEM -----
		//---------------------------------------------------------------------------------------

		//Dump sectors per track, # of heads, # of hidden sectors in partition (12 bytes)
		buffer_pointer += 12;

		//Get 'sectors per fat'  [# + 0x0024]
		sectors_per_fat = (DWORD)*buffer_pointer++;
		sectors_per_fat |= (DWORD)(*buffer_pointer++) << 8;
		sectors_per_fat |= (DWORD)(*buffer_pointer++) << 16;
		sectors_per_fat |= (DWORD)(*buffer_pointer++) << 24;

		//Get 'Flags' [# + 0x0028]
		//(Bits 0-4 Indicate Active FAT Copy)
		//(Bit 7 Indicates whether FAT Mirroring is Enabled or Disabled <Clear is Enabled>) (If FAT Mirroringis Disabled, the FAT Information is
		//only written to the copy indicated by bits 0-4)
		w_temp = (DWORD)*buffer_pointer++;
		w_temp |= (DWORD)(*buffer_pointer++) << 8;
		if (w_temp & 0x0080)
		{
			//BIT7 = 1, FAT MIRRORING IS DISABLED
			//Bits 3:0 set which FAT table is active
			if ((w_temp & 0x000f) > number_of_copies_of_fat)
				goto init_new_ffs_card_fail;
			
			switch (w_temp & 0x000f)
			{
			case 0:
				active_fat_table_flags = 0x01;						// #|#|#|#|USE_FAT_TABLE_3|USE_FAT_TABLE_2|USE_FAT_TABLE_1|USE_FAT_TABLE_0
				break;
			case 1:
				active_fat_table_flags = 0x02;
				break;
			case 2:
				active_fat_table_flags = 0x04;
				break;
			case 3:
				active_fat_table_flags = 0x08;
				break;
			}
		}
		else
		{
			//BIT7 = 0, FAT MIRRORING IS ENABLED INTO ALL FATS
			active_fat_table_flags = 0;						// #|#|#|#|USE_FAT_TABLE_3|USE_FAT_TABLE_2|USE_FAT_TABLE_1|USE_FAT_TABLE_0
			for (b_temp = 0; b_temp < number_of_copies_of_fat; b_temp++)
			{
				active_fat_table_flags <<= 1;
				active_fat_table_flags++;
			}
		}

		//Get 'Version of FAT32 Drive' [# + 0x002A]
		//(High Byte = Major Version, Low Byte = Minor Version)
		buffer_pointer++;
		buffer_pointer++;

		//Get 'Cluster Number of the Start of the Root Directory' [# + 0x2C]
		//(Usually 2, but not requried to be 2)
		root_directory_start_sector_cluster = (DWORD)*buffer_pointer++;
		root_directory_start_sector_cluster |= (DWORD)(*buffer_pointer++) << 8;
		root_directory_start_sector_cluster |= (DWORD)(*buffer_pointer++) << 16;
		root_directory_start_sector_cluster |= (DWORD)(*buffer_pointer++) << 24;

		//Get 'Sector Number of the File System Information Sector' [# + 0x0030]
		//(Referenced from the Start of the Partition. Usually 1, but not requried to be 1)
		file_system_information_sector = (DWORD)*buffer_pointer++;
		file_system_information_sector |= (DWORD)(*buffer_pointer++) << 8;


		//CALCULATE THE PARTITION AREAS START ADDRESSES
		fat1_start_sector = main_partition_start_sector + (DWORD)number_of_reserved_sectors;			//THE FAT START ADDRESS IS NOW GOOD (has correct offset)
		//root_directory_start_sector_cluster already done above
		data_area_start_sector = main_partition_start_sector + (DWORD)number_of_reserved_sectors + (sectors_per_fat * number_of_copies_of_fat);

		//SET UNUSED REGISTERS (used for FAT16)
		number_of_root_directory_sectors = 0;
	}

	//------------------------------------------------------------------------
	//----- BOOT RECORD IS DONE - ALL REQUIRED DISK PARAMETERS ARE KNOWN -----
	//------------------------------------------------------------------------


	//-----------------------------
	//----- CARD IS OK TO USE -----
	//-----------------------------
	FFS_CE = 1;						//Deselect the card
	ffs_card_ok = 1;				//Flag that the card is OK

	//Do CF Driver specific initialisations
	last_found_free_cluster = 0;		//When we next look for a free cluster, start from the beginning


	return;


//----------------------------------
//----- CARD IS NOT COMPATIBLE -----
//----------------------------------
init_new_ffs_card_fail:
	FFS_CE = 1;						//Deselect the card
	ffs_card_ok = 0;				//Flag that the card is not OK
	return;
}









//*********************************************************************************************
//*********************************************************************************************
//*********************************************************************************************
//*********************************************************************************************
//****************************** DRIVER SUB FUNCTIONS BELOW HERE ******************************
//*********************************************************************************************
//*********************************************************************************************
//*********************************************************************************************
//*********************************************************************************************







//***************************************
//***************************************
//********** IS CARD PRESENT ? **********
//***************************************
//***************************************
//Returns
//	1 if present, 0 if not
BYTE ffs_is_card_present (void)
{

	#ifdef	FFS_CD_PIN_FUNCTION
		FFS_CD_PIN_FUNCTION();
	#endif


	//IF CD pin is low then card is present
	if (FFS_CD_PIN_REGISTER & FFS_CD_PIN_BIT)
		return(0);
	else
		return(1);
}






//***************************************
//***************************************
//********** DO CARD RESET PIN **********
//***************************************
//***************************************
void ffs_card_reset_pin (BYTE pin_state)
{

	if (pin_state)
		FFS_RESET_PIN_REGISTER  |= FFS_RESET_PIN_BIT;
	else
		FFS_RESET_PIN_REGISTER  &= ~FFS_RESET_PIN_BIT;
	
	#ifdef	FFS_RESET_PIN_FUNCTION
		FFS_RESET_PIN_FUNCTION();
	#endif
}







//*******************************************
//*******************************************
//********** READ SECTOR TO BUFFER **********
//*******************************************
//*******************************************
//lba = start sector address
//The card must be deselected after all of the data has been read using:
//	FFS_CE = 1;					//Deselect the card
void ffs_read_sector_to_buffer (DWORD sector_lba)
{
	WORD count;
	BYTE *buffer_pointer;


	//----- IF LBA MATCHES THE LAST LBA DON'T BOTHER RE-READING AS THE DATA IS STILL IN THE BUFFER -----
	if (ffs_buffer_contains_lba == sector_lba)
	{
		return;
	}

	FFS_CE = 0;										//Select the card


	//----- IF THE BUFFER CONTAINS DATA THAT IS WAITING TO BE WRITTEN THEN WRITE IT FIRST -----
	if (ffs_buffer_needs_writing_to_card)
	{
		if (ffs_buffer_contains_lba != 0xffffffff)			//This should not be possible but check is made just in case!
			ffs_write_sector_from_buffer(ffs_buffer_contains_lba);

		FFS_CE = 0;										//Select the card again

		ffs_buffer_needs_writing_to_card = 0;
	}



	//----- NEW LBA TO BE LOADED -----
	ffs_set_address(0x06);							//0x06 - Write the 'Select Card/Head' register [LBA27:24 - bits3:0]
	ffs_write_byte((BYTE)(0b11100000 | (sector_lba >> 24)));	//(Use Logic Block Addressing - not Cylinder,Head,Sector)

	ffs_set_address(0x05);							//0x05 - Write the 'Cylinder High' register [LBA23:16]
	ffs_write_byte((BYTE)((sector_lba & 0x00ff0000) >> 16));

	ffs_set_address(0x04);							//0x04 - Write the 'Cylinder Low' register [LBA15:8]
	ffs_write_byte((BYTE)((sector_lba & 0x0000ff00) >> 8));

	ffs_set_address(0x03);							//0x03 - Write the 'Sector No' register [LBA7:0]
	ffs_write_byte((BYTE)(sector_lba & 0x000000ff));

	ffs_set_address(0x02);							//0x02 - Write the 'Sector Count' register (no of sectors to be transfered to complete the operation)
	ffs_write_byte(1);

	ffs_set_address(0x07);							//0x07 - Write the 'Command' register
	ffs_write_byte(0x20);							//Read sector(s) command

	ffs_set_address(0x00);							//0x00 - Read from the data register

	ffs_buffer_contains_lba = 0xffffffff;			//Flag that buffer does not currently contain any lba


	//----- READ THE SECTOR INTO THE BUFFER -----
	buffer_pointer = &FFS_DRIVER_GEN_512_BYTE_BUFFER[0];

	for (count = 0; count < ffs_bytes_per_sector; count++)
	{
		*buffer_pointer++ = ffs_read_byte();
	}

	ffs_buffer_contains_lba = sector_lba;				//Flag that the data buffer currently contains data for this LBA (logged to avoid re-loading the buffer again if its not necessary)

	FFS_CE = 1;											//De-select the card

}







//**********************************************
//**********************************************
//********** WRITE SECTOR FROM BUFFER **********
//**********************************************
//**********************************************
void ffs_write_sector_from_buffer (DWORD sector_lba)
{
	WORD count;
	BYTE *buffer_pointer;
	
	buffer_pointer = &FFS_DRIVER_GEN_512_BYTE_BUFFER[0];

	ffs_buffer_needs_writing_to_card = 0;			//Flag that buffer is no longer waiting to write to card (must be at top as this function
													//calls other functions that check this flag and would call the function back)

	
	//----- SETUP TO WRITE THE SECTOR -----
	FFS_CE = 0;										//Select the card

	ffs_set_address(0x06);							//0x06 - Write the 'Select Card/Head' register [LBA27:24 - bits3:0]
	ffs_write_byte((BYTE)(0b11100000 | (sector_lba >> 24)));	//(Use Logic Block Addressing - not Cylinder,Head,Sector)

	ffs_set_address(0x05);							//0x05 - Write the 'Cylinder High' register [LBA23:16]
	ffs_write_byte((BYTE)((sector_lba & 0x00ff0000) >> 16));

	ffs_set_address(0x04);							//0x04 - Write the 'Cylinder Low' register [LBA15:8]
	ffs_write_byte((BYTE)((sector_lba & 0x0000ff00) >> 8));

	ffs_set_address(0x03);							//0x03 - Write the 'Sector No' register [LBA7:0]
	ffs_write_byte((BYTE)(sector_lba & 0x000000ff));

	ffs_set_address(0x02);							//0x02 - Write the 'Sector Count' register (no of sectors to be transfered to complete the operation)
	ffs_write_byte(1);

	ffs_set_address(0x07);							//0x07 - Write the 'Command' register
	ffs_write_byte(0x30);							//Write sector(s) command

	ffs_set_address(0x00);							//0x00 - Write to the data register


	//----- WRITE THE BUFFER TO THE CARD SECTOR -----
	for (count = 0; count < ffs_bytes_per_sector; count++)
	{
		ffs_write_byte(*buffer_pointer++);
	}
	

	FFS_CE = 1;						//Deselect the card
}




//*********************************
//*********************************
//********** SET ADDRESS **********
//*********************************
//*********************************
void ffs_set_address (BYTE address)
{
	//----- IF THE BUFFER CONTAINS DATA THAT IS WAITING TO BE WRITTEN THEN WRITE IT FIRST -----
	//(As we are now doing some other operation)
	if (ffs_buffer_needs_writing_to_card)
	{
		if (ffs_buffer_contains_lba != 0xffffffff)			//This should not be possible but check is made just in case!
			ffs_write_sector_from_buffer(ffs_buffer_contains_lba);

		FFS_CE = 0;										//Select the card again

		ffs_buffer_needs_writing_to_card = 0;
	}

	ffs_buffer_contains_lba = 0xffffffff;			//Flag that buffer does not currently contain any lba (done here as something new is happening with the card so don't rely on the data buffer having the same data in it after whatever is happening)

	//----- SET THE ADDRESS -----
	if (address & 0x01)
		FFS_ADDRESS_REGISTER |= FFS_ADDRESS_BIT_0;
	else
		FFS_ADDRESS_REGISTER &= ~FFS_ADDRESS_BIT_0;

	if (address & 0x02)
		FFS_ADDRESS_REGISTER |= FFS_ADDRESS_BIT_1;
	else
		FFS_ADDRESS_REGISTER &= ~FFS_ADDRESS_BIT_1;

	if (address & 0x04)
		FFS_ADDRESS_REGISTER |= FFS_ADDRESS_BIT_2;
	else
		FFS_ADDRESS_REGISTER &= ~FFS_ADDRESS_BIT_2;

	#ifdef FFS_ADDRESS_FUNCTION
		FFS_ADDRESS_FUNCTION();
	#endif
}






//****************************************
//****************************************
//********** WRITE BYTE TO CARD **********
//****************************************
//****************************************
BYTE ffs_write_byte (BYTE data)
{
	//Bus to outputs
	FFS_DATA_BUS_TO_OUTPUTS;

	//Load the data to latch
	FFS_DATA_BUS_OP = data;

	//Set timeout to 100mS (added timeout to stop crash sometimes when card is inserted)
	ffs_10ms_timer = 10;

	while (ffs_10ms_timer)
	{
		if(FFS_RDY)				//Ensure card is ready
			goto ffs_write_byte_1;
	}
	return (0);					//Error

ffs_write_byte_1:
	FFS_WE = 0;

	FFS_DELAY_FOR_WAIT_SIGNAL();
	while(FFS_WAIT == 0);		//Check to see if card is inserting a wait state

	FFS_WE = 1;
	
	return (1);
}






//*****************************************
//*****************************************
//********** READ WORD FROM CARD **********
//*****************************************
//*****************************************
WORD ffs_read_word (void)
{
	WORD data;

	while(FFS_RDY == 0);				//Ensure card is ready

	//Bus to inputs
	FFS_DATA_BUS_TO_INPUTS;

	FFS_OE = 0;

	FFS_DELAY_FOR_WAIT_SIGNAL();
	while(FFS_WAIT == 0);		//Check to see if card is inserting a wait state

	data = (WORD)FFS_DATA_BUS_IP;

	FFS_OE = 1;

	FFS_DELAY_FOR_RDY_SIGNAL();
	while(FFS_RDY == 0);				//Ensure card is ready

	FFS_OE = 0;

	FFS_DELAY_FOR_WAIT_SIGNAL();
	while(FFS_WAIT == 0);		//Check to see if card is inserting a wait state

	data += ((WORD)FFS_DATA_BUS_IP << 8);

	FFS_OE = 1;

	//Bus to outputs
	FFS_DATA_BUS_TO_OUTPUTS;

	return (data);
}






//*****************************************
//*****************************************
//********** READ BYTE FROM CARD **********
//*****************************************
//*****************************************
BYTE ffs_read_byte (void)
{
	BYTE data;

	while(FFS_RDY == 0);				//Ensure card is ready

	//Bus to inputs
	FFS_DATA_BUS_TO_INPUTS;

	FFS_OE = 0;

	FFS_DELAY_FOR_WAIT_SIGNAL();
	while(FFS_WAIT == 0);		//Check to see if card is inserting a wait state

	data = FFS_DATA_BUS_IP;

	FFS_OE = 1;

	//Bus to outputs
	FFS_DATA_BUS_TO_OUTPUTS;

	return (data);
}








