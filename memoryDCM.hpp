//
//  memoryDCM.hpp
//  
//
//  Created by Rede Dor on 05/09/18.
//

#ifndef memoryDCM_hpp
#define memoryDCM_hpp

#include <stdio.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <time.h>
#include "print.h"
#include "nii_dicom_batch.h"
#include <math.h>
#include <float.h>

using namespace std;

struct TDCMdim { //DimensionIndexValues
    uint32_t dimIdx[MAX_NUMBER_OF_DIMENSIONS];
    uint32_t diskPos;
    float triggerDelayTime, TE, intenScale, intenIntercept, intenScalePhilips, RWVScale, RWVIntercept;
    float V[4];
    bool isPhase;
    bool isReal;
    bool isImaginary;
};

struct TVolumeDiffusion {
    struct TDICOMdata* pdd;  // The multivolume
    struct TDTI4D* pdti4D;   // permanent records.

    uint8_t manufacturer;            // kMANUFACTURER_UNKNOWN, kMANUFACTURER_SIEMENS, etc.

    //void set_manufacturer(const uint8_t m) {manufacturer = m; update();}  // unnecessary

    // Everything after this in the structure would be private if it were a C++
    // class, but it has been rewritten as a struct for C compatibility.  I am
    // using _ as a hint of that, although _ for privacy is not really a
    // universal convention in C.  Privacy is desired because immediately
    // any of these are updated _update_tvd() should be called.

    bool _isAtFirstPatientPosition;   // Limit b vals and vecs to 1 per volume.

    //float bVal0018_9087;      // kDiffusion_b_value, always present in Philips/Siemens.
    //float bVal2001_1003;        // kDiffusionBFactor
    // float dirRL2005_10b0;        // kDiffusionDirectionRL
    // float dirAP2005_10b1;        // kDiffusionDirectionAP
    // float dirFH2005_10b2;        // kDiffusionDirectionFH

    // Philips diffusion scans tend to have a "trace" (average of the diffusion
    // weighted volumes) volume tacked on, usually but not always at the end,
    // so b is > 0, but the direction is meaningless.  Most software versions
    // explicitly set the direction to 0, but version 3 does not, making (0x18,
    // 0x9075) necessary.
    bool _isPhilipsNonDirectional;

    //char _directionality0018_9075[16];   // DiffusionDirectionality, not in Philips 2.6.
    // float _orientation0018_9089[3];      // kDiffusionOrientation, always
    //                                      // present in Philips/Siemens for
    //                                      // volumes with a direction.
    //char _seq0018_9117[64];              // MRDiffusionSequence, not in Philips 2.6.

    float _dtiV[4];
    double _symBMatrix[6];
    //uint16_t numDti;
};

int dcmInt (int lByteLength, unsigned char lBuffer[], bool littleEndian);
int dcmStrInt (const int lByteLength, const unsigned char lBuffer[]);
void dcmStr(int lLength, unsigned char lBuffer[], char* lOut, bool isStrLarge = false);
float dcmFloat(int lByteLength, unsigned char lBuffer[], bool littleEndian);
double dcmFloatDouble(const size_t lByteLength, const unsigned char lBuffer[], const bool littleEndian);
int dcmStrManufacturer (int lByteLength, unsigned char lBuffer[]);
float dcmStrFloat (const int lByteLength, const unsigned char lBuffer[]);
void dcmMultiFloat (int lByteLength, char lBuffer[], int lnFloats, float *lFloats);
bool isFloatDiff (float a, float b);
void dcmMultiShorts (int lByteLength, unsigned char lBuffer[], int lnShorts, uint16_t *lShorts, bool littleEndian);
int readCSAImageHeader(unsigned char *buff, int lLength, struct TCSAdata *CSA, int isVerbose, bool is3DAcq);
void dcmStrDigitsOnly(char* lStr);
void dcmStrDigitsOnlyKey(char key, char* lStr);
void dcmStrDigitsDotOnlyKey(char key, char* lStr);
struct TVolumeDiffusion initTVolumeDiffusion(struct TDICOMdata* ptdd, struct TDTI4D* dti4D);
void dcmMultiLongs (int lByteLength, unsigned char lBuffer[], int lnLongs, uint32_t *lLongs, bool littleEndian);
int compareTDCMdim(void const *item1, void const *item2);
int isSQ(uint32_t groupElement);
void dcmMultiFloatDouble (size_t lByteLength, unsigned char lBuffer[], size_t lnFloats, float *lFloats, bool isLittleEndian);
void set_directionality0018_9075(struct TVolumeDiffusion* ptvd, unsigned char* inbuf);
void set_bValGE(struct TVolumeDiffusion* ptvd, int lLength, unsigned char* inbuf);
void set_diffusion_directionGE(struct TVolumeDiffusion* ptvd, int lLength, unsigned char* inbuf, const int axis);
void set_bVal(struct TVolumeDiffusion* ptvd, float b);
void set_orientation0018_9089(struct TVolumeDiffusion* ptvd, int lLength, unsigned char* inbuf, bool isLittleEndian);
void set_isAtFirstPatientPosition_tvd(struct TVolumeDiffusion* ptvd, const bool iafpp);
uint32_t mz_crc32(unsigned char *ptr, uint32_t buf_len);
uint32_t mz_crc32X(unsigned char *ptr, unsigned long);
struct TDICOMdata readDICOMv(stringstream &filemem, int isVerbose, int compressFlag, struct TDTI4D *dti4D);
int headerDcm2Nii(struct TDICOMdata d, struct nifti_1_header *h, bool isComputeSForm);
int nii_saveNII3D(char * niiFilename, struct nifti_1_header hdr, unsigned char* im, struct TDCMopts opts);
int nii_saveNII(char * niiFilename, struct nifti_1_header hdr, unsigned char* im, struct TDCMopts opts, struct TDICOMdata d);

void set_diffusion_directionPhilips(struct TVolumeDiffusion* ptvd, float vec, const int axis);
void set_bMatrix(TVolumeDiffusion* ptvd, double b, int component);
void _update_tvd(struct TVolumeDiffusion* ptvd);

#endif /* memoryDCM_hpp */


