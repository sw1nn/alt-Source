
//      AltReg_CAN.H
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


#ifndef _ALTREG_CAN_H_
#define _ALTREG_CAN_H_


#define SUPPORT_NMEA2000                                    // Should we include the NMEA2000 PGNs?
#define SUPPORT_RVC                                         // What about the RV-C?  
                                                            //  Note that RV-C is needed for coordination of charging sources, and remote instrumentation.
                                                            //  However, if you only want the NMEA-2000, and some other NMEA-2000 devices are unable to handle the standard RV-C messages, 
                                                            //  you may disable them.  (or place the Regulator on an dedicated CAN network and use a bridge)


#include "Config.h"

#ifdef SYSTEMCAN                                            // To allow common source code for old and new designs; nothing happens if we are not compiling for the new CAN board.    
#include <Arduino.h>
#include <NMEA2000.h>                                       // https://github.com/ttlappalainen
#include <N2kMessages.h>                                    // https://github.com/ttlappalainen
#include <RVCMessages.h>                                    // https://github.com/thomasonw
#include "AltReg_Serial.h"
    
    

typedef struct CCS {                                            // CAN Configuration Structure
        uint8_t     BI_OVERRIDE;                                // RV_C 'Battery' number of the DC system - override for DIP switch (0 = use DIP switch value)
        uint8_t     DEVICE_INSTANCE;                            // RV_C 'Charger' instance
        uint8_t     DEVICE_PRIORITY;                            // RV_C 'device ranking' relative to other DC sources.  Used to arbitrate who will be MASTER of mesh.
        bool        CONSIDER_MASTER;                            // Should we ever try to be the RBS master?  (Allows regulator to have a high priority for charging prioritization,
                                                                // but not be allowed to be the master.  Example, a Solar Controller which does not have its remote battery sensing wires connected.)
        bool        SHUNT_AT_BAT;                               // Has the user told us the local current shunt is attached to THE battery shunt?
                                                                // This flag is used to decide if we are able to report the true Battery Amps if we are acting as the RBM.
        tRVCBatType BATTERY_TYPE;                               // User configured (Informing us).  What is the Battery Type (See RV-C for definition)
        bool        ENABLE_NMEA2000;                            // Should the NMEA_2000 CAN protocol be sent out?
        bool        ENABLE_OSE;                                 // Should the RV-C protocol be sent out?  (These two configuration flags allow for silencing in case where conflicts cause issues)
        bool        ENABLE_NMEA2000_RAT;                        // Configured the Alternator Regulator to look to a NMEA-2000 node sending out NMEA-2000 battery status message (PGN:127506) with 
                                                                // battery current and temperature.  If a one is and the battery instance is the same, this information will be used to remotely instrument
                                                                // battery amperage and temperature.  While this feature is set, the Alternator Regulator will suspend its sending of PGN:127506, to reduce confusion.
        uint8_t     LAST_CAN_ID;                                // Last CAN-ID/address we were assigned via the ISO-AddressClaim() process.  Used for best-practice of trying to position oneself at the same address
                                                                // as last used - to retain a consistency in the system between power-on cycles,  (At least until someone adds another device)
    } CCS;
    
   
    
                                                          
  
#define REMOTE_CAN_MASTER_STABILITY 10000UL                             // We need to see 10 seconds of messaged from any potential 'remote' device wishing us to lock onto it.
#define REMOTE_CAN_MASTER_TIMEOUT     750UL                             // And if we do not hear SOMETHING from it for over 750mS second, we figure it is dead and need to start over.
                                                                        // (Should be broadcasting every 500mS max per spec)

#define REMOTE_CAN_RAT2000_TIMEOUT   2000UL                             // An optional NMEA2000 based Remote Amp and Temp device SHOULD talk every 666mS, lets give them 2 seconds.

#define REMOTE_CAN_LPCS_TIMEOUT       750UL                             //  Likewise, if we do not hear from any Lower Priority Charging Sources . . figure WE are it.
#define REMOTE_CAN_HPUUCS_TIMEOUT    7500UL                             // Charger Status 1 (which has utilization %) comes every 5000mS..

#define RBM_REMASTER_IDLE_PERIOD    2 * REMOTE_CAN_MASTER_STABILITY     // When looking to establish a new RMB, wait at least 2x the stability timeout period.  To give some time for network errors to clear
#define RBM_REMASTER_IDLE_MULTI     50UL                                // Also, hold off a little longer based on your 'priority' level.  Let 'smarter' devices try 1st for king...


#define MAX_SUPPORTED_SYSTEM_AMPS      2000                             // Upper limit of Amps we expect the battery to take in. 
#define MAX_SUPPORTED_SYSTEM_VOFFSET  1.5                               // If the Voltage Offset is greater then 1.5v, something is wring on the wiring.
                                                                        // (Used to indicate a crazy remote CAN battery master who we will ignore) 


#define FLAG_DC1  0x01 << 0                                             // Used by validate_CAN() to track which RV-C DC status/command message we have received from the RBS master
#define FLAG_DC2  0x01 << 1                                             // Tracked in --> uint8_t CAN_RBM_messages
#define FLAG_DC3  0x01 << 2
#define FLAG_DC4  0x01 << 3
#define FLAG_DC5  0x01 << 4



//--- Prototypes and extern statments 
bool initialize_CAN(void);
void send_CAN(void); 
void check_CAN(void);
void decide_if_CAN_RBM(void);
void handle_CAN_Messages(const tN2kMsg &N2kMsg);
bool handle_CAN_Requests(unsigned long RequestedPGN, unsigned char Requester, int DeviceIndex);

void CAN_ASCII_write(char *stng);
int  CAN_ASCII_read(void);
int  CAN_ASCII_available(void);

void invalidate_RBM(void);
uint8_t fetch_CAN_localID(void);





 
    
extern CCS              canConfig;
extern float            CAN_RBM_amps;      
extern unsigned long    CAN_RBM_ampsRefreshed;
extern float            CAN_RBM_volts;
extern float            CAN_RBM_voltsOffset; 
extern int              CAN_RBM_voPWMvalue; 
extern uint16_t         CAN_RBM_dVdT;               
extern unsigned long    CAN_RBM_voltsRefreshed;
extern int              CAN_RBM_temp;  
extern unsigned long    CAN_RBM_tempRefreshed; 
extern bool             ignoringRBM;

extern unsigned long    CAN_RAT2000_lastReceived; 
extern float            CAN_RAT2000_amps;
extern int              CAN_RAT2000_temp; 

extern uint8_t          batteryInstance;
extern float            CAN_RBM_desiredVolts;                                
extern float            CAN_RBM_desiredAmps;
extern tRVCBatChrgMode  CAN_RBM_desiredChargeState;
extern uint8_t          CAN_RBM_sourceID; 
extern unsigned long    CAN_RBM_lastReceived;
extern uint8_t          CAN_RBM_messages; 


extern bool             CAN_weAreRBM; 
extern unsigned long    CAN_LPCS_lastReceived;    
extern unsigned long    CAN_EPCS_lastReceived;    
extern unsigned long    CAN_HPUUCS_lastReceived;
extern int              average_EPC_utilization;

extern uint8_t          CAN_ASCII_source;



#endif   // SYSTEMCAN
#endif   // _ALTREG_CAN_H_             




