
//      Flash.cpp
//
//      Copyright (c) 2016, 2017 by William A. Thomason.      http://arduinoalternatorregulator.blogspot.com/
//
//
//
//              This program is free software: you can redistribute it and/or modify
//              it under the terms of the GNU General Public License as published by
//              the Free Software Foundation, either version 3 of the License, or
//              (at your option) any later version.
//      
//              This program is distributed in the hope that it will be useful,
//              but WITHOUT ANY WARRANTY; without even the implied warranty of
//              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//              GNU General Public License for more details.
//      
//              You should have received a copy of the GNU General Public License
//              along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//


#include "Flash.h"



#ifdef EEPROM_SIM  
 void eeprom_read_block  (void *__dst, const void *__src, size_t __n);
 void eeprom_write_block (const void *__src, void *__dst, size_t __n);          // Prototypes for STM32F07x Flash simulation.
 #endif







//------------------------------------------------------------------------------------------------------
// CRC-32
//
//      This function will calculate a CRC-32 for the passed array and return it as an unsigned LONG value
//      They are used in the EEPROM reads and writes
//      
// 
//------------------------------------------------------------------------------------------------------



unsigned long crc_update(unsigned long crc, uint8_t data)                               // Helper function, used to read CRC value out of PROGMEN.
{
    static const PROGMEM uint32_t crc_table[16] = {                                     // Table to simplify calculations of CRCs
      0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
      0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
      0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
      0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
      };
 
    uint8_t tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
    }



unsigned long calc_crc (uint8_t*d, int sizeD) {

 unsigned long crc = ~0L;
  int i;

  for (i = 0; i < sizeD; i++)
    crc = crc_update(crc, *d++);
  crc = ~crc;
  return crc;
  }








//------------------------------------------------------------------------------------------------------
// Read SCS EEPROM
//
//      This function will see if there is are valid structures for the  sytemConfig saved EEPROM. 
//      If so, it will copy those from the EEPROM into the passed buffer and return TRUE.
//
//
//
//------------------------------------------------------------------------------------------------------

bool read_SCS_EEPROM(SCS *scsPtr) {
  
   SCS buff;
   EKEY  key;                                                                    // Structure used to see validate the presence of saved data.




   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON, sizeof(EKEY));         // Fetch the EKEY structure from EEPROM to update appropriate portions


   if  ((key.SCS_ID1 == SCS_ID1_K) && (key.SCS_ID2 == SCS_ID2_K)) {                     // We may have a valid systemConfig structure . . 
        eeprom_read_block((void*)&buff, (void *)SCS_FLASH_LOCAITON, sizeof(SCS));     
                                                                                        //  . .  let's fetch it from EEPROM and see if the CRCs check out..
        if (calc_crc ((uint8_t*)&buff, sizeof(SCS)) == key.SCS_CRC32) {
            *scsPtr = buff;                                                             //  Looks valid, copy the working buffer into RAM
             return(true);
             }
        }

 
    return(false);                                                                      // Did not make it through all the checks...
}




//------------------------------------------------------------------------------------------------------
// Write SCS EEPROM
//
//      If a pointer is passed into this function it will write the current systemConfig and ChargeParms tables into the EEPROM and update the
//      appropriate EEPROM Key variables.   If NULL is passed in for the data pointer, the saved SCS will be invalidated in the EEPROM
//
//
//
//------------------------------------------------------------------------------------------------------

void write_SCS_EEPROM(SCS *scsPtr) {

   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.

 
   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON  , sizeof(EKEY));       // Fetch the EKEY structure from EEPROM to update appropriate portions

   
  if (scsPtr != NULL) {
        key.SCS_ID1 = SCS_ID1_K;                                                        // User wants to save the current systemConfig structure to EEPROM
        key.SCS_ID2 = SCS_ID2_K;                                                        // Put in validation tokens
        key.SCS_CRC32 = calc_crc ((uint8_t*)scsPtr, sizeof(SCS));

        
        #if !defined DEBUG && !defined SIMULATION
           if ((systemConfig.BT_CONFIG_CHANGED == false) ||                             // Wait a minute: Before we do any actual changes. . if the Bluetooth 
               (systemConfig.CONFIG_LOCKOUT   != 0))                                    // has been made a bit more secure, or if we are locked out --> prevent any update.
              return;
              #endif                                                                    // (But do this check only if not in 'testing' mode!


        eeprom_write_block((void*)scsPtr, (void *)SCS_FLASH_LOCAITON, sizeof(SCS));
        }                                                                               // And write out the current structure
        
  else  {
        key.SCS_ID1 = 0;                                                                // User wants to invalidate the EEPROM saved info.  
        key.SCS_ID2 = 0;                                                                // So just zero out the validation tokens
        key.SCS_CRC32 = 0;                                                              // And the CRC-32 to make dbl sure.
        }
        
  eeprom_write_block((void *)&key, (void *)EKEY_FLASH_LOCAITON  , sizeof(EKEY));        // Save back the updated EKEY structure

}





//------------------------------------------------------------------------------------------------------
// Read CPS EEPROM
//
//      This function will see if there is are valid structures saved in EEPROM for the entry 'index' of the chargeParm table.
//      If so, it will copy that from the EEPROM into the passed CPS structure and TRUE is returned.
//
//      index is CPS entry from the EEPROM to be copied into the working CPE structure 
//
//
//------------------------------------------------------------------------------------------------------

bool read_CPS_EEPROM(uint8_t index, CPS *cpsPtr) {
  
   CPS   buff;
   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.


   eeprom_read_block((void *)&key, (const void *)EKEY_FLASH_LOCAITON  , sizeof(EKEY));  // Fetch the EKEY structure from EEPROM

   if  ((key.CPS_ID1[index] == CPS_ID1_K) && (key.CPS_ID2[index] == CPS_ID2_K)) {       // How about the Charge Profile tables? . . 

        eeprom_read_block((void*)&buff, (void *)CPS_FLASH_LOCAITON, sizeof(CPS));
                                                                                        //  . .  let's fetch it from EEPROM and see if the CRCs check out..
        
        if (calc_crc ((uint8_t*)&buff, sizeof(CPS)) == key.CPS_CRC32[index]) {
                *cpsPtr = buff;                                                         //  Looks valid, copy the working buffer into RAM
                return(true);
                }
        }

   return(false);                                                                       // Did not make it through all the checks...
  
}



//------------------------------------------------------------------------------------------------------
// Write CPS EEPROM
//
//      Will write the passed CPS structure into the saved entry 'N' of the EEPROM.  If NULL is passed in for the data pointer, 
//      Entry 'N' will be invalidated in the EEPROM
//
//
//
//------------------------------------------------------------------------------------------------------



void write_CPS_EEPROM(uint8_t index, CPS *cpsPtr) {                                     // Save/flush entry 'index'

   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.

 
   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON  , sizeof(EKEY));       // Fetch the EKEY structure from EEPROM to update appropriate portions

   
  if (cpsPtr != NULL) {
        key.CPS_ID1[index] = CPS_ID1_K;                                                 // User wants to save the current systemConfig structure to EEPROM
        key.CPS_ID2[index] = CPS_ID2_K;                                                 // Put in validation tokens
        key.CPS_CRC32[index] = calc_crc ((uint8_t*)cpsPtr, sizeof(CPS));

        #if !defined DEBUG && !defined SIMULATION
           if ((systemConfig.BT_CONFIG_CHANGED == false) ||                             // Wait a minute: Before we do any actual changes. . if the Bluetooth 
               (systemConfig.CONFIG_LOCKOUT   != 0))                                    // has not been made a bit more secure, or if we are locked out, prevent any update.
              return;
              #endif                                                                    // (But do this check only if not in 'testing' mode!

        eeprom_write_block((void*)cpsPtr, (void *)CPS_FLASH_LOCAITON, sizeof(CPS));
        }                                                                               // And write out the indexed CPE entry
        
  else  {
        key.CPS_ID1[index]   = 0;                                                       // User wants to invalidate the EEPROM saved info.  
        key.CPS_ID2[index]   = 0;                                                       // So just zero out the validation tokens
        key.CPS_CRC32[index] = 0;                                                       // And the CRC-32 to make dbl sure.
        }

     eeprom_write_block((void *)&key, (void *) EKEY_FLASH_LOCAITON  , sizeof(EKEY));    // Save back the updated EKEY structure

}








//------------------------------------------------------------------------------------------------------
// Transfer Default CPS from FLASH
//
//      This function copy the default entry from the PROGMEM CPS structure in FLASH to the passed buffer.
//
//      index is CPS entry from the PROGMEM to be copied into the passed CPE structure 
//
//
//------------------------------------------------------------------------------------------------------

void transfer_default_CPS(uint8_t index, CPS  *cpsPtr) {


   uint8_t*wp = (uint8_t*) cpsPtr;
   uint8_t*ep = (uint8_t*) &defaultCPS[index];                                               //----- Copy the appropriate default Charge Parameters entry from FLASH into the working structure   

   for (unsigned int u=0; u < sizeof(CPS); u++)                                 
           *wp++ = (uint8_t) pgm_read_byte_near(ep++);

}





//------------------------------------------------------------------------------------------------------
// Read CAL EEPROM
//
//      This function will see if there is are valid structures for the  sytemConfig saved EEPROM. 
//      If so, it will copy those from the EEPROM into the passed buffer and return TRUE.
//
//
//
//------------------------------------------------------------------------------------------------------

bool read_CAL_EEPROM(CAL *calPtr) {
  
   CAL buff;
   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.




   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON, sizeof(EKEY));         // Fetch the EKEY structure from EEPROM to update appropriate portions

  
   if  ((key.CAL_ID1 == CAL_ID1_K) && (key.CAL_ID2 == CAL_ID2_K)) {                     // We may have a valid systemConfig structure . . 

        eeprom_read_block((void*)&buff, (void *)CAL_FLASH_LOCAITON, sizeof(CAL));     
                                                                                        //  . .  lets fetch it from EEPROM and see if the CRCs check out..
        if (calc_crc ((uint8_t*)&buff, sizeof(CAL)) == key.CAL_CRC32) {
            *calPtr = buff;                                                             //  Looks valid, copy the working buffer into RAM
             return(true);
             }
        }

    return(false);                                                                      // Did not make it through all the checks...
}




//------------------------------------------------------------------------------------------------------
// Write CAL EEPROM
//
//      If a pointer is passed into this function it will write the current ADCCal and ChargeParms tables into the EEPROM and update the
//      appropriate EEPROM Key variables.   If NULL is passed in for the data pointer, the saved CAL structure will be invalidated in the EEPROM
//
//
//
//------------------------------------------------------------------------------------------------------

void write_CAL_EEPROM(CAL *calPtr) {

   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.

 
   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON  , sizeof(EKEY));       // Fetch the EKEY structure from EEPROM to update appropriate portions

   
  if (calPtr != NULL) {
        key.CAL_ID1 = CAL_ID1_K;                                                        // User wants to save the current systemConfig structure to EEPROM
        key.CAL_ID2 = CAL_ID2_K;                                                        // Put in validation tokens
        key.CAL_CRC32 = calc_crc ((uint8_t*)calPtr, sizeof(CAL));

        
          eeprom_write_block((void*)calPtr, (void *)CAL_FLASH_LOCAITON, sizeof(CAL));
        }                                                                               // And write out the current structure
        
  else  {
        key.CAL_ID1 = 0;                                                                // User wants to invalidate the EEPROM saved info.  
        key.CAL_ID2 = 0;                                                                // So just zero out the validation tokens
        key.CAL_CRC32 = 0;                                                              // And the CRC-32 to make dbl sure.
        }
        
  eeprom_write_block((void *)&key, (void *)EKEY_FLASH_LOCAITON  , sizeof(EKEY));        // Save back the updated EKEY structure

}







#ifdef SYSTEMCAN  

//------------------------------------------------------------------------------------------------------
// Read CCS EEPROM
//
//      This function will see if there is are valid structures for the CanConfig saved EEPROM. 
//      If so, it will copy those from the EEPROM into the passed buffer and return TRUE.
//
//
//
//------------------------------------------------------------------------------------------------------

bool read_CCS_EEPROM(CCS *ccsPtr) {

   CCS buff;
   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.


   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON, sizeof(EKEY));         // Fetch the EKEY structure from EEPROM to update appropriate portions

  
   if  ((key.CCS_ID1 == CCS_ID1_K) && (key.CCS_ID2 == CCS_ID2_K)) {                     // We may have a valid systemConfig structure . . 

        eeprom_read_block((void*)&buff, (void *)CCS_FLASH_LOCAITON, sizeof(CCS));     
                                                                                        //  . .  lets fetch it from EEPROM and see if the CRCs check out..
        if (calc_crc ((uint8_t*)&buff, sizeof(CCS)) == key.CCS_CRC32) {
            *ccsPtr = buff;                                                             //  Looks valid, copy the working buffer into RAM
             return(true);
             }
        }

    return(false);                                                                      // Did not make it through all the checks...
    
}




//------------------------------------------------------------------------------------------------------
// Write CCS EEPROM
//
//      If TRUE is passed into these functions they will write the current CanConfig table into the EEPROM and update the
//      appropriate EEPROM Key variables.   If NULL is passed in for the data pointer, the saved CCS will be invalidated in the EEPROM
//
//
//
//------------------------------------------------------------------------------------------------------

void write_CCS_EEPROM(CCS *ccsPtr) {

   EKEY  key;                                                                           // Structure used to see validate the presence of saved data.

 
   eeprom_read_block((void *)&key, (void *) EKEY_FLASH_LOCAITON  , sizeof(EKEY));       // Fetch the EKEY structure from EEPROM to update appropriate portions

   
  if (ccsPtr != NULL) {
        key.CCS_ID1 = CCS_ID1_K;                                                        // User wants to save the current systemConfig structure to EEPROM
        key.CCS_ID2 = CCS_ID2_K;                                                        // Put in validation tokens
        key.CCS_CRC32 = calc_crc ((uint8_t*)ccsPtr, sizeof(CCS));

        
        #if !defined DEBUG && !defined SIMULATION
           if ((systemConfig.BT_CONFIG_CHANGED == false) ||                             // Wait a minute: Before we do any actual changes. . if the Bluetooth 
               (systemConfig.CONFIG_LOCKOUT   != 0))                                    // has not been made a bit more secure, or if we are locked out, prevent any update.
              return;
              #endif                                                                    // (But do this check only if not in 'testing' mode!


        eeprom_write_block((void*)ccsPtr, (void *)CCS_FLASH_LOCAITON, sizeof(CCS));
        }                                                                               // And write out the current structure
        
  else  {
        key.CCS_ID1 = 0;                                                                // User wants to invalidate the EEPROM saved info.  
        key.CCS_ID2 = 0;                                                                // So just zero out the validation tokens
        key.CCS_CRC32 = 0;                                                              // And the CRC-32 to make dbl sure.
        }
        
  eeprom_write_block((void *)&key, (void *)EKEY_FLASH_LOCAITON  , sizeof(EKEY));        // Save back the updated EKEY structure

}


#endif      // SYSTEMCAN








//------------------------------------------------------------------------------------------------------
// Restore All
//
//      This function will restore all EEPROM based configuration values (system and all the Charge profile tables
//      to their default (as compiled) values.  It does this by erasing clearing out each table entry.
//      Note that this function then will reboot the machine, so it will not return...
// 
//
//------------------------------------------------------------------------------------------------------



void    restore_all(){

     uint8_t b;

     write_SCS_EEPROM(NULL);                                                            // Erase any saved systemConfig structure in the EEPROM
     if (ADCCal.Locked != true)   write_CAL_EEPROM(NULL);                               // If not factory locked, reset the CAL structure.

     for (b=0; b<MAX_CPES; b++) 
        write_CPS_EEPROM(b, NULL);                                                      // Erase any saved charge profiles structures in the EEPROM
    
     #ifdef SYSTEMCAN
       write_CCS_EEPROM(NULL);                                                          // CANConfig structure as well
       #endif


     
     reboot();                                                                          // And FORCE the system to reset - we will never come back from here!
   }












//------------------------------------------------------------------------------------------------------
// Commit EEPROM
//
//      This function will commit changes to the EEPROM.  It is primary used on CPUs which have EEPROM simulated
//      using FLASH.  For other CPUs (those with EEPROM), it has no purpose as the actual EEPROM writes have already
//      been done with each individual save.
//
//------------------------------------------------------------------------------------------------------

void commit_EEPROM(void) {
  
#ifdef EEPROM_SIM

    //!! SOME CODE GOES HERE!!!!



#endif
}







//------------------------------------------------------------------------------------------------------
// EEPROM Simulation block read/write
//
//      SYSTEM CPUs (ATmega64M1 and STM32F07) do not have EEPROM, so it is simulated using a portion of the FLASH memory.
//      These functions provide linkage between a common simulation lib and the original AVR libs.
//
//
//
//------------------------------------------------------------------------------------------------------
#ifdef EEPROM_SIM


void eeprom_read_block  (void *__dst, const void *__src, size_t __n) {
  //!!  SOME CODE GOES HERE
  }
  
void eeprom_write_block (const void *__src, void *__dst, size_t __n) {
  //!!  SOME CODE GOES HERE
  };

#endif      // EEPROM_SIM











