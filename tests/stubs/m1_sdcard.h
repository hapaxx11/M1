/* Stub m1_sdcard.h — provides only the S_M1_SDCard_Access_Status enum for
 * host-side unit tests. */

#ifndef M1_SDCARD_H_
#define M1_SDCARD_H_

typedef enum
{
    SD_access_OK = 0,
    SD_access_NotOK,
    SD_access_NotReady,
    SD_access_UnMounted,
    SD_access_NoFS,
    SD_access_EndOfStatus
} S_M1_SDCard_Access_Status;

#endif /* M1_SDCARD_H_ */
