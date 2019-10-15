/*
 * Copyright 2019 Arcus Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Stripped down header file of library support we actually use,
//  underlying code carries this copyright...

/**
  ******************************************************************************
  * @file    lib_ConfigManager.h 
  * @author  MMY Application Team
  * @version V4.0.0
  * @date    02/06/2014
  * @brief   This file provides set of firmware functions to manages device modes. 
  ******************************************************************************
  * @copyright
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  */ 

/* Define to prevent recursive inclusion --------------------------------------------*/
#ifndef _LIB_ST95HF_H
#define _LIB_ST95HF_H



/* Flags for PICC/PCD tracking  ----------------------------------------------------------*/
#define	TRACK_NOTHING			0x00
#define	TRACK_NFCTYPE1  		0x01 /* 0000 0001 */
#define	TRACK_NFCTYPE2 			0x02 /* 0000 0010 */
#define	TRACK_NFCTYPE3 			0x04 /* 0000 0100 */
#define	TRACK_NFCTYPE4A 		0x08 /* 0000 1000 */
#define	TRACK_NFCTYPE4B 		0x10 /* 0001 0000 */
#define	TRACK_NFCTYPE5 			0x20 /* 0010 0000 */
#define TRACK_ALL 			0xFF /* 1111 1111 */

#define RESULTOK 				0x00 
#define ERRORCODE_GENERIC 		1

typedef enum {UNDEFINED_MODE=0,PICC,PCD}DeviceMode_t;
extern DeviceMode_t devicemode;
extern uint8_t NDEF_Buffer [];

typedef struct {
    char protocol[80];
    char URI_Message[400];
    char Information[400];
}sURI_Info;

typedef enum 
{
    UNKNOWN_TYPE = 0,
    VCARD_TYPE,
    WELL_KNOWN_ABRIDGED_URI_TYPE,
    URI_SMS_TYPE,
    URI_GEO_TYPE,
    URI_EMAIL_TYPE,
    SMARTPOSTER_TYPE,
    URL_TYPE,
    TEXT_TYPE,
    BT_TYPE,
    /* list of "external type" known by this demo, other external type will be addressed as UNKNWON_TYPE */
    M24SR_DISCOVERY_APP_TYPE 
} NDEF_TypeDef;

#define SP_MAX_RECORD				4

typedef struct sRecordInfo sRecordInfo;
struct sRecordInfo
{
    uint8_t RecordFlags;
    uint8_t TypeLength;
    uint8_t PayloadLength3;
    uint8_t PayloadLength2;
    uint8_t PayloadLength1;
    uint8_t PayloadLength0;
    uint8_t IDLength;
    uint8_t Type[0xFF];
    uint8_t ID[0xFF];
    uint16_t PayloadOffset;
    uint32_t PayloadBufferAdd;    /* add where payload content has been stored */
    NDEF_TypeDef NDEF_Type;  /* to store identification ID for application */
    sRecordInfo *SPRecordStructAdd[SP_MAX_RECORD]; /*in case of smart poster array to store add of other sRecordInfo struct */
    uint8_t NbOfRecordInSPPayload;
};      

/* public function	 ----------------------------------------------------------------*/

void ConfigManager_HWInit (void);
uint8_t ConfigManager_TagHunting ( uint8_t tagsToFind );
void ConfigManager_Stop(void);

uint16_t NDEF_ReadURI(sRecordInfo *pRecordStruct, sURI_Info *pURI);
uint16_t NDEF_WriteURI(sURI_Info *pURI);
uint16_t NDEF_IdentifyNDEF ( sRecordInfo *pRecordStruct, uint8_t* pNDEF );
uint16_t NDEF_IdentifyBuffer ( sRecordInfo *pRecordStruct, uint8_t* pNDEF );

uint8_t PCDNFCT1_ReadNDEF( uint8_t *data );
uint8_t PCDNFCT2_ReadNDEF( uint8_t *data );
uint8_t PCDNFCT3_ReadNDEF( uint8_t *data );
uint8_t PCDNFCT4_ReadNDEF( uint8_t *data );
uint8_t PCDNFCT5_ReadNDEF( uint8_t *data );

int8_t ISO15693_ReadSingleBlock ( const uint8_t Flags, const uint8_t *UID, const uint16_t BlockNumber, uint8_t *pResponse );
int8_t ISO15693_WriteSingleBlock ( const uint8_t Flags, const uint8_t *UIDin, const uint16_t BlockNumber, const uint8_t *DataToWrite,uint8_t *pResponse );
int8_t ISO15693_GetUID (uint8_t *UIDout);

#endif

/******************* (C) COPYRIGHT 2014 STMicroelectronics *****END OF FILE****/
