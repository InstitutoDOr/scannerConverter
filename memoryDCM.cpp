//
//  memoryDCM.cpp
//  
//
//  Created by Rede Dor on 05/09/18.
//

#include "memoryDCM.hpp"

int isDICOMfile(stringstream &filemem) { //0=NotDICOM, 1=DICOM, 2=Maybe(not Part 10 compliant)
    filemem.seekg(0, filemem.end);
    int fileLen=filemem.tellg(); //Get file length
    if (fileLen < 256) {
        printMessage("File Size = %d\n", fileLen);
        printMessage( "File too small to be a DICOM image \n");
        return 0;
    }

    filemem.seekg(0, filemem.beg);
    char buffer[256];
    filemem.read(buffer, 256);

    if ((buffer[128] == 'D') && (buffer[129] == 'I')  && (buffer[130] == 'C') && (buffer[131] == 'M'))
        return 1; //valid DICOM
    if ((buffer[0] == 8) && (buffer[1] == 0)  && (buffer[3] == 0))
        return 2; //not valid Part 10 file, perhaps DICOM object
    return 0;
} //isDICOMfile()

struct TDICOMdata readDICOMv(stringstream &filemem, int isVerbose, int compressFlag, struct TDTI4D *dti4D) {
    struct TDICOMdata d = clear_dicom_data();
    d.imageNum = 0; //not set
    strcpy(d.protocolName, ""); //erase dummy with empty
    strcpy(d.protocolName, ""); //erase dummy with empty
    strcpy(d.seriesDescription, ""); //erase dummy with empty
    strcpy(d.sequenceName, ""); //erase dummy with empty
    //do not read folders - code specific to GCC (LLVM/Clang seems to recognize a small file size)
    dti4D->sliceOrder[0] = -1;
    struct TVolumeDiffusion volDiffusion = initTVolumeDiffusion(&d, dti4D);

    bool isPart10prefix = true;
    int isOK = isDICOMfile(filemem);
    if (isOK == 0) return d;
    if (isOK == 2) {
        d.isExplicitVR = false;
        isPart10prefix = false;
    }

    filemem.seekg(0, filemem.end);
    long fileLen=filemem.tellg(); //Get file length
    if (fileLen < 256) {
        printMessage( "File too small to be a DICOM image \n");
        return d;
    }
    //Since size of DICOM header is unknown, we will load it in 1mb segments
    //This uses less RAM and makes is faster for computers with slow disk access
    //Benefit is largest for 4D images.
    //To disable caching and load entire file to RAM, compile with "-dmyLoadWholeFileToReadHeader"
    //To implement the segments, we define these variables:
    // fileLen = size of file in bytes
    // MaxBufferSz = maximum size of buffer in bytes
    // Buffer = array with n elements, where n is smaller of fileLen or MaxBufferSz
    // lPos = position in Buffer (indexed from 0), 0..(n-1)
    // lFileOffset = offset of Buffer in file: true file position is lOffset+lPos (initially 0)
    #ifdef myLoadWholeFileToReadHeader
    size_t MaxBufferSz = fileLen;
    #else
    size_t MaxBufferSz = 1000000; //ideally size of DICOM header, but this varies from 2D to 4D files
    #endif
    if (MaxBufferSz > (size_t)fileLen)
        MaxBufferSz = fileLen;
    //printf("%d -> %d\n", MaxBufferSz, fileLen);
    long lFileOffset = 0;
    filemem.seekg(0, filemem.beg);
    //Allocate memory
    unsigned char *buffer=(unsigned char *)malloc(MaxBufferSz+1);
    if (!buffer) {
        printError( "Memory exhausted!");
        return d;
    }
    //Read file contents into buffer
    filemem.read((char *)buffer, MaxBufferSz);
    if (!filemem) {
        printError("Error reading stream \n");
        return d;
    }
    //DEFINE DICOM TAGS
#define  kUnused 0x0001+(0x0001 << 16 )
#define  kStart 0x0002+(0x0000 << 16 )
#define  kTransferSyntax 0x0002+(0x0010 << 16)
//#define  kImplementationVersionName 0x0002+(0x0013 << 16)
#define  kSourceApplicationEntityTitle 0x0002+(0x0016 << 16 )
//#define  kSpecificCharacterSet 0x0008+(0x0005 << 16 ) //someday we should handle foreign characters...
#define  kImageTypeTag 0x0008+(0x0008 << 16 )
#define  kStudyDate 0x0008+(0x0020 << 16 )
#define  kAcquisitionDate 0x0008+(0x0022 << 16 )
#define  kAcquisitionDateTime 0x0008+(0x002A << 16 )
#define  kStudyTime 0x0008+(0x0030 << 16 )
#define  kAcquisitionTime 0x0008+(0x0032 << 16 ) //TM
#define  kContentTime 0x0008+(0x0033 << 16 ) //TM
#define  kModality 0x0008+(0x0060 << 16 ) //CS
#define  kManufacturer 0x0008+(0x0070 << 16 )
#define  kInstitutionName 0x0008+(0x0080 << 16 )
#define  kInstitutionAddress 0x0008+(0x0081 << 16 )
#define  kReferringPhysicianName 0x0008+(0x0090 << 16 )
#define  kStationName 0x0008+(0x1010 << 16 )
#define  kSeriesDescription 0x0008+(0x103E << 16 ) // '0008' '103E' 'LO' 'SeriesDescription'
#define  kInstitutionalDepartmentName  0x0008+(0x1040 << 16 )
#define  kManufacturersModelName 0x0008+(0x1090 << 16 )
#define  kDerivationDescription 0x0008+(0x2111 << 16 )
#define  kComplexImageComponent (uint32_t) 0x0008+(0x9208 << 16 )//'0008' '9208' 'CS' 'ComplexImageComponent'
#define  kPatientName 0x0010+(0x0010 << 16 )
#define  kPatientID 0x0010+(0x0020 << 16 )
#define  kPatientBirthDate 0x0010+(0x0030 << 16 )
#define  kPatientSex 0x0010+(0x0040 << 16 )
#define  kPatientAge 0x0010+(0x1010 << 16 )
#define  kPatientWeight 0x0010+(0x1030 << 16 )
#define  kAnatomicalOrientationType 0x0010+(0x2210 << 16 )
#define  kBodyPartExamined 0x0018+(0x0015 << 16)
#define  kScanningSequence 0x0018+(0x0020 << 16)
#define  kSequenceVariant 0x0018+(0x0021 << 16)
#define  kScanOptions 0x0018+(0x0022 << 16)
#define  kMRAcquisitionType 0x0018+(0x0023 << 16)
#define  kSequenceName 0x0018+(0x0024 << 16)
#define  kZThick  0x0018+(0x0050 << 16 )
#define  kTR  0x0018+(0x0080 << 16 )
#define  kTE  0x0018+(0x0081 << 16 )
//#define  kEffectiveTE  0x0018+(0x9082 << 16 )
const uint32_t kEffectiveTE  = 0x0018+ (0x9082 << 16);
#define  kTI  0x0018+(0x0082 << 16) // Inversion time
#define  kEchoNum  0x0018+(0x0086 << 16 ) //IS
#define  kMagneticFieldStrength  0x0018+(0x0087 << 16 ) //DS
#define  kZSpacing  0x0018+(0x0088 << 16 ) //'DS' 'SpacingBetweenSlices'
#define  kPhaseEncodingSteps  0x0018+(0x0089 << 16 ) //'IS'
#define  kEchoTrainLength  0x0018+(0x0091 << 16 ) //IS
#define  kPhaseFieldofView  0x0018+(0x0094 << 16 ) //'DS'
#define  kPixelBandwidth  0x0018+(0x0095 << 16 ) //'DS' 'PixelBandwidth'
#define  kDeviceSerialNumber  0x0018+(0x1000 << 16 ) //LO
#define  kSoftwareVersions  0x0018+(0x1020 << 16 ) //LO
#define  kProtocolName  0x0018+(0x1030<< 16 )
#define  kRadionuclideTotalDose  0x0018+(0x1074<< 16 )
#define  kRadionuclideHalfLife  0x0018+(0x1075<< 16 )
#define  kRadionuclidePositronFraction  0x0018+(0x1076<< 16 )
#define  kGantryTilt  0x0018+(0x1120  << 16 )
#define  kXRayExposure  0x0018+(0x1152  << 16 )
#define  kAcquisitionMatrix  0x0018+(0x1310  << 16 ) //US
#define  kFlipAngle  0x0018+(0x1314  << 16 )
#define  kInPlanePhaseEncodingDirection  0x0018+(0x1312<< 16 ) //CS
#define  kSAR  0x0018+(0x1316 << 16 ) //'DS' 'SAR'
#define  kPatientOrient  0x0018+(0x5100<< 16 )    //0018,5100. patient orientation - 'HFS'
#define  kDiffusionDirectionality  0x0018+uint32_t(0x9075<< 16 )   // NONE, ISOTROPIC, or DIRECTIONAL
//#define  kDiffusionBFactorSiemens  0x0019+(0x100C<< 16 ) //   0019;000C;SIEMENS MR HEADER  ;B_value
#define  kDiffusion_bValue  0x0018+uint32_t(0x9087<< 16 ) // FD
#define  kDiffusionOrientation  0x0018+uint32_t(0x9089<< 16 ) // FD, seen in enhanced
                                                              // DICOM from Philips 5.*
                                                              // and Siemens XA10.
#define  kMREchoSequence  0x0018+uint32_t(0x9114<< 16 ) //SQ
#define  kNumberOfImagesInMosaic  0x0019+(0x100A<< 16 ) //US NumberOfImagesInMosaic
#define  kDwellTime  0x0019+(0x1018<< 16 ) //IS in NSec, see https://github.com/rordenlab/dcm2niix/issues/127
#define  kLastScanLoc  0x0019+(0x101B<< 16 )
#define  kDiffusionDirectionGEX  0x0019+(0x10BB<< 16 ) //DS phase diffusion direction
#define  kDiffusionDirectionGEY  0x0019+(0x10BC<< 16 ) //DS frequency diffusion direction
#define  kDiffusionDirectionGEZ  0x0019+(0x10BD<< 16 ) //DS slice diffusion direction
#define  kSharedFunctionalGroupsSequence  0x5200+uint32_t(0x9229<< 16 ) // SQ
#define  kPerFrameFunctionalGroupsSequence  0x5200+uint32_t(0x9230<< 16 ) // SQ
#define  kBandwidthPerPixelPhaseEncode  0x0019+(0x1028<< 16 ) //FD
//#define  kRawDataRunNumberGE  0x0019+(0x10a2<< 16 ) //SL
#define  kStudyID 0x0020+(0x0010 << 16 )
#define  kSeriesNum 0x0020+(0x0011 << 16 )
#define  kAcquNum 0x0020+(0x0012 << 16 )
#define  kImageNum 0x0020+(0x0013 << 16 )
#define  kStudyInstanceUID 0x0020+(0x000D << 16 )
#define  kSeriesInstanceUID 0x0020+(0x000E << 16 )
#define  kImagePositionPatient 0x0020+(0x0032 << 16 )   // Actually !
#define  kOrientationACR 0x0020+(0x0035 << 16 )
//#define  kTemporalPositionIdentifier 0x0020+(0x0100 << 16 ) //IS
#define  kOrientation 0x0020+(0x0037 << 16 )
#define  kImagesInAcquisition 0x0020+(0x1002 << 16 ) //IS
#define  kImageComments 0x0020+(0x4000<< 16 )// '0020' '4000' 'LT' 'ImageComments'
#define  kTriggerDelayTime 0x0020+uint32_t(0x9153<< 16 ) //FD
#define  kDimensionIndexValues 0x0020+uint32_t(0x9157<< 16 ) // UL n-dimensional index of frame.
#define  kInStackPositionNumber 0x0020+uint32_t(0x9057<< 16 ) // UL can help determine slices in volume
#define  kLocationsInAcquisitionGE 0x0021+(0x104F<< 16 )// 'SS' 'LocationsInAcquisitionGE'
#define  kRTIA_timer 0x0021+(0x105E<< 16 )// 'DS'
#define  kProtocolDataBlockGE 0x0025+(0x101B<< 16 )// 'OB'
#define  kSamplesPerPixel 0x0028+(0x0002 << 16 )
#define  kPhotometricInterpretation 0x0028+(0x0004 << 16 )
#define  kPlanarRGB 0x0028+(0x0006 << 16 )
#define  kDim3 0x0028+(0x0008 << 16 ) //number of frames - for Philips this is Dim3*Dim4
#define  kDim2 0x0028+(0x0010 << 16 )
#define  kDim1 0x0028+(0x0011 << 16 )
#define  kXYSpacing  0x0028+(0x0030 << 16 ) //'0028' '0030' 'DS' 'PixelSpacing'
#define  kBitsAllocated 0x0028+(0x0100 << 16 )
#define  kBitsStored 0x0028+(0x0101 << 16 )//'0028' '0101' 'US' 'BitsStored'
#define  kIsSigned 0x0028+(0x0103 << 16 )
#define  kIntercept 0x0028+(0x1052 << 16 )
#define  kSlope 0x0028+(0x1053 << 16 )
#define  kGeiisFlag 0x0029+(0x0010 << 16 ) //warn user if dreaded GEIIS was used to process image
#define  kCSAImageHeaderInfo  0x0029+(0x1010 << 16 )
#define  kCSASeriesHeaderInfo 0x0029+(0x1020 << 16 )
    //#define  kObjectGraphics  0x0029+(0x1210 << 16 )    //0029,1210 syngoPlatformOOGInfo Object Oriented Graphics
#define  kProcedureStepDescription 0x0040+(0x0254 << 16 )
#define  kRealWorldIntercept  0x0040+uint32_t(0x9224 << 16 ) //IS dicm2nii's SlopInt_6_9
#define  kRealWorldSlope  0x0040+uint32_t(0x9225 << 16 ) //IS dicm2nii's SlopInt_6_9
#define  kUserDefineDataGE  0x0043+(0x102A << 16 ) //OB
#define  kEffectiveEchoSpacingGE  0x0043+(0x102C << 16 ) //SS
#define  kDiffusionBFactorGE  0x0043+(0x1039 << 16 ) //IS dicm2nii's SlopInt_6_9
#define  kAcquisitionMatrixText  0x0051+(0x100B << 16 ) //LO
#define  kCoilSiemens  0x0051+(0x100F << 16 )
#define  kImaPATModeText  0x0051+(0x1011 << 16 )
#define  kLocationsInAcquisition  0x0054+(0x0081 << 16 )
//ftp://dicom.nema.org/MEDICAL/dicom/2014c/output/chtml/part03/sect_C.8.9.4.html
//If ImageType is REPROJECTION we slice direction is reversed - need example to test
// #define  kSeriesType  0x0054+(0x1000 << 16 )
#define  kDoseCalibrationFactor  0x0054+(0x1322<< 16 )
#define  kPETImageIndex  0x0054+(0x1330<< 16 )
#define  kPEDirectionDisplayedUIH  0x0065+(0x1005<< 16 )//SH
#define  kDiffusion_bValueUIH  0x0065+(0x1009<< 16 ) //FD
#define  kNumberOfImagesInGridUIH  0x0065+(0x1050<< 16 ) //DS
#define  kMRVFrameSequenceUIH  0x0065+(0x1050<< 16 ) //SQ
#define  kDiffusionGradientDirectionUIH  0x0065+(0x1037<< 16 ) //FD
#define  kIconImageSequence 0x0088+(0x0200 << 16 )
#define  kPMSCT_RLE1 0x07a1+(0x100a << 16 ) //Elscint/Philips compression
#define  kDiffusionBFactor  0x2001+(0x1003 << 16 )// FL
#define  kSliceNumberMrPhilips 0x2001+(0x100A << 16 ) //IS Slice_Number_MR
#define  kSliceOrient  0x2001+(0x100B << 16 )//2001,100B Philips slice orientation (TRANSVERSAL, AXIAL, SAGITTAL)
#define  kNumberOfSlicesMrPhilips 0x2001+(0x1018 << 16 )//SL 0x2001, 0x1018 ), "Number_of_Slices_MR"
//#define  kNumberOfLocationsPhilips  0x2001+(0x1015 << 16 ) //SS
//#define  kStackSliceNumber  0x2001+(0x1035 << 16 )//? Potential way to determine slice order for Philips?
#define  kNumberOfDynamicScans  0x2001+(0x1081 << 16 )//'2001' '1081' 'IS' 'NumberOfDynamicScans'
#define  kMRAcquisitionTypePhilips 0x2005+(0x106F << 16)
#define  kAngulationAP 0x2005+(0x1071 << 16)//'2005' '1071' 'FL' 'MRStackAngulationAP'
#define  kAngulationFH 0x2005+(0x1072 << 16)//'2005' '1072' 'FL' 'MRStackAngulationFH'
#define  kAngulationRL 0x2005+(0x1073 << 16)//'2005' '1073' 'FL' 'MRStackAngulationRL'
#define  kMRStackOffcentreAP 0x2005+(0x1078 << 16)
#define  kMRStackOffcentreFH 0x2005+(0x1079 << 16)
#define  kMRStackOffcentreRL 0x2005+(0x107A << 16)
#define  kPhilipsSlope 0x2005+(0x100E << 16 )
#define  kDiffusionDirectionRL 0x2005+(0x10B0 << 16)
#define  kDiffusionDirectionAP 0x2005+(0x10B1 << 16)
#define  kDiffusionDirectionFH 0x2005+(0x10B2 << 16)
#define  kPrivatePerFrameSq 0x2005+(0x140F << 16)
#define  kMRImageDiffBValueNumber 0x2005+(0x1412 << 16) //IS
//#define  kMRImageGradientOrientationNumber 0x2005+(0x1413 << 16) //IS
#define  kWaveformSq 0x5400+(0x0100 << 16)
#define  kImageStart 0x7FE0+(0x0010 << 16 )
#define  kImageStartFloat 0x7FE0+(0x0008 << 16 )
#define  kImageStartDouble 0x7FE0+(0x0009 << 16 )
uint32_t kItemTag = 0xFFFE +(0xE000 << 16 );
uint32_t kItemDelimitationTag = 0xFFFE +(0xE00D << 16 );
uint32_t kSequenceDelimitationItemTag = 0xFFFE +(0xE0DD << 16 );
double TE = 0.0; //most recent echo time recorded
    bool is2005140FSQ = false;
    double contentTime = 0.0;
    int philMRImageDiffBValueNumber = 0;
    int sqDepth = 0;
    int acquisitionTimesUIH = 0;
    int sqDepth00189114 = -1;
    int nDimIndxVal = -1; //tracks Philips kDimensionIndexValues
    int locationsInAcquisitionGE = 0;
    int PETImageIndex = 0;
    int inStackPositionNumber = 0;
    int maxInStackPositionNumber = 0;
    //int temporalPositionIdentifier = 0;
    int locationsInAcquisitionPhilips = 0;
    int imagesInAcquisition = 0;
    //int sumSliceNumberMrPhilips = 0;
    int sliceNumberMrPhilips = 0;
    int numberOfFrames = 0;
    //int MRImageGradientOrientationNumber = 0;
    //int minGradNum = kMaxDTI4D + 1;
    //int maxGradNum = -1;
    int numberOfDynamicScans = 0;
    uint32_t lLength;
    uint32_t groupElement;
    long lPos = 0;
    bool isPhilipsDerived = false;
    //bool isPhilipsDiffusion = false;
    if (isPart10prefix) { //for part 10 files, skip preamble and prefix
        lPos = 128+4; //4-byte signature starts at 128
        groupElement = buffer[lPos] | (buffer[lPos+1] << 8) | (buffer[lPos+2] << 16) | (buffer[lPos+3] << 24);
        if (groupElement != kStart)
            printMessage("DICOM appears corrupt: first group:element should be 0x0002:0x0000 \n");
    }
    char vr[2];
    //float intenScalePhilips = 0.0;
    char acquisitionDateTimeTxt[kDICOMStr] = "";
    bool isEncapsulatedData = false;
    int multiBandFactor = 0;
    int numberOfImagesInMosaic = 0;
    int encapsulatedDataFragments = 0;
    int encapsulatedDataFragmentStart = 0; //position of first FFFE,E000 for compressed images
    int encapsulatedDataImageStart = 0; //position of 7FE0,0010 for compressed images (where actual image start should be start of first fragment)
    bool isOrient = false;
    bool isIconImageSequence = false;
    bool isSwitchToImplicitVR = false;
    bool isSwitchToBigEndian = false;
    bool isAtFirstPatientPosition = false; //for 3d and 4d files: flag is true for slices at same position as first slice
    bool isMosaic = false;
    int patientPositionNum = 0;
    float B0Philips = -1.0;
    float vRLPhilips = 0.0;
    float vAPPhilips = 0.0;
    float vFHPhilips = 0.0;
    bool isPhase = false;
    bool isReal = false;
    bool isImaginary = false;
    bool isMagnitude = false;
    d.seriesNum = -1;
    float patientPositionPrivate[4] = {NAN, NAN, NAN, NAN};
    float patientPosition[4] = {NAN, NAN, NAN, NAN}; //used to compute slice direction for Philips 4D
    //float patientPositionPublic[4] = {NAN, NAN, NAN, NAN}; //used to compute slice direction for Philips 4D
    float patientPositionEndPhilips[4] = {NAN, NAN, NAN, NAN};
    float patientPositionStartPhilips[4] = {NAN, NAN, NAN, NAN};
    //struct TDTI philDTI[kMaxDTI4D];
    //for (int i = 0; i < kMaxDTI4D; i++)
    //  philDTI[i].V[0] = -1;
    //array for storing DimensionIndexValues
    int numDimensionIndexValues = 0;
#ifdef USING_R
    // Allocating a large array on the stack, as below, vexes valgrind and may cause overflow
    std::vector<TDCMdim> dcmDim(kMaxSlice2D);
#else
    TDCMdim dcmDim[kMaxSlice2D];
#endif
    for (int i = 0; i < kMaxSlice2D; i++) {
        dcmDim[i].diskPos = i;
        for (int j = 0; j < MAX_NUMBER_OF_DIMENSIONS; j++)
            dcmDim[i].dimIdx[j] = 0;
    }
    //http://dicom.nema.org/dicom/2013/output/chtml/part05/sect_7.5.html
    //The array nestPos tracks explicit lengths for Data Element Tag of Value (FFFE,E000)
    //a delimiter (fffe,e000) can have an explicit length, in which case there is no delimiter (fffe,e00d)
    // fffe,e000 can provide explicit lengths, to demonstrate ./dcmconv +ti ex.DCM im.DCM
    #define kMaxNestPost 128
    int nNestPos = 0;
    size_t nestPos[kMaxNestPost];
    while ((d.imageStart == 0) && ((lPos+8+lFileOffset) <  fileLen)) {
        #ifndef myLoadWholeFileToReadHeader //read one segment at a time
        if ((size_t)(lPos + 128) > MaxBufferSz) { //avoid overreading the file
            lFileOffset = lFileOffset + lPos;
            if ((lFileOffset+MaxBufferSz) > (size_t)fileLen) MaxBufferSz = fileLen - lFileOffset;
            filemem.seekg(lFileOffset, filemem.beg);
            filemem.read((char *)buffer, MaxBufferSz);
            if (!filemem) {
                    printError("Error reading stream\n");
                    return d;
            }
            lPos = 0;
        }
        #endif
        if (d.isLittleEndian)
            groupElement = buffer[lPos] | (buffer[lPos+1] << 8) | (buffer[lPos+2] << 16) | (buffer[lPos+3] << 24);
        else
            groupElement = buffer[lPos+1] | (buffer[lPos] << 8) | (buffer[lPos+3] << 16) | (buffer[lPos+2] << 24);
        if ((isSwitchToBigEndian) && ((groupElement & 0xFFFF) != 2)) {
            isSwitchToBigEndian = false;
            d.isLittleEndian = false;
            groupElement = buffer[lPos+1] | (buffer[lPos] << 8) | (buffer[lPos+3] << 16) | (buffer[lPos+2] << 24);
        }//transfer syntax requests switching endian after group 0002
        if ((isSwitchToImplicitVR) && ((groupElement & 0xFFFF) != 2)) {
            isSwitchToImplicitVR = false;
            d.isExplicitVR = false;
        } //transfer syntax requests switching VR after group 0001
        //uint32_t group = (groupElement & 0xFFFF);
        lPos += 4;
    if ((groupElement == kItemDelimitationTag) || (groupElement == kSequenceDelimitationItemTag)) isIconImageSequence = false;
    //if (groupElement == kItemTag) sqDepth++;
    bool unNest = false;
    while ((nNestPos > 0) && (nestPos[nNestPos] <= (lFileOffset+lPos))) {
            nNestPos--;
            sqDepth--;
            unNest = true;
    }
    if (groupElement == kItemDelimitationTag) { //end of item with undefined length
        sqDepth--;
        unNest = true;
    }
    if (unNest)  {
        is2005140FSQ = false;
        if (sqDepth < 0) sqDepth = 0; //should not happen, but protect for faulty anonymization
        //if we leave the folder MREchoSequence 0018,9114

        if (( nDimIndxVal > 0) && (d.manufacturer == kMANUFACTURER_PHILIPS) && (sqDepth00189114 >= sqDepth)) {
            sqDepth00189114 = -1; //triggered
            if (inStackPositionNumber > 0) {
                //for images without SliceNumberMrPhilips (2001,100A)
                int sliceNumber = inStackPositionNumber;
                if ((sliceNumber == 1) && (!isnan(patientPosition[1])) ) {
                    for (int k = 0; k < 4; k++)
                        patientPositionStartPhilips[k] = patientPosition[k];
                } else if ((sliceNumber == 1) && (!isnan(patientPositionPrivate[1]))) {
                    for (int k = 0; k < 4; k++)
                        patientPositionStartPhilips[k] = patientPositionPrivate[k];
                }
                if ((sliceNumber == maxInStackPositionNumber) && (!isnan(patientPosition[1])) ) {
                    for (int k = 0; k < 4; k++)
                        patientPositionEndPhilips[k] = patientPosition[k];
                } else if ((sliceNumber == maxInStackPositionNumber) && (!isnan(patientPositionPrivate[1])) ) {
                    for (int k = 0; k < 4; k++)
                        patientPositionEndPhilips[k] = patientPositionPrivate[k];
                }
                patientPosition[1] = NAN;
                patientPositionPrivate[1] = NAN;
            }
            inStackPositionNumber = 0;
            if (numDimensionIndexValues >= kMaxSlice2D) {
                printError("Too many slices to track dimensions. Only up to %d are supported\n", kMaxSlice2D);
                break;
            }
            int ndim = nDimIndxVal;
            for (int i = 0; i < ndim; i++)
                dcmDim[numDimensionIndexValues].dimIdx[i] = d.dimensionIndexValues[i];
            dcmDim[numDimensionIndexValues].TE = TE;
            dcmDim[numDimensionIndexValues].intenScale = d.intenScale;
            dcmDim[numDimensionIndexValues].intenIntercept = d.intenIntercept;
            dcmDim[numDimensionIndexValues].isPhase = isPhase;
            dcmDim[numDimensionIndexValues].isReal = isReal;
            dcmDim[numDimensionIndexValues].isImaginary = isImaginary;
            dcmDim[numDimensionIndexValues].intenScalePhilips = d.intenScalePhilips;
            dcmDim[numDimensionIndexValues].RWVScale = d.RWVScale;
            dcmDim[numDimensionIndexValues].RWVIntercept = d.RWVIntercept;
            dcmDim[numDimensionIndexValues].triggerDelayTime = d.triggerDelayTime;
            dcmDim[numDimensionIndexValues].V[0] = -1.0;
            #ifdef MY_DEBUG
            if (numDimensionIndexValues < 19) {
                printMessage("dimensionIndexValues0020x9157[%d] = [", numDimensionIndexValues);
                for (int i = 0; i < ndim; i++)
                    printMessage("%d ", d.dimensionIndexValues[i]);
                printMessage("]\n");
                //printMessage("B0= %g  num=%d\n", B0Philips, gradNum);
            } else return d;
            #endif
            //next: add diffusion if reported
            if (B0Philips >= 0.0) { //diffusion parameters
                // Philips does not always provide 2005,1413 (MRImageGradientOrientationNumber) and sometimes after dimensionIndexValues
                /*int gradNum = 0;
                for (int i = 0; i < ndim; i++)
                    if (d.dimensionIndexValues[i] > 0) gradNum = d.dimensionIndexValues[i];
                if (gradNum <= 0) break;
                With Philips 51.0 both ADC and B=0 are saved as same direction, though they vary in another dimension
                (0018,9075) CS [ISOTROPIC]
                (0020,9157) UL 1\2\1\33 << ADC MAP
                (0018,9075) CS [NONE]
                (0020,9157) UL 1\1\2\33
                next two lines attempt to skip ADC maps
                we could also increment gradNum for ADC if we wanted...
                */
                if (isPhilipsDerived) {
                    //gradNum ++;
                    B0Philips = 2000.0;
                    vRLPhilips = 0.0;
                    vAPPhilips = 0.0;
                    vFHPhilips = 0.0;
                }
                if (B0Philips == 0.0) {
                    //printMessage(" DimensionIndexValues grad %d b=%g vec=%gx%gx%g\n", gradNum, B0Philips, vRLPhilips, vAPPhilips, vFHPhilips);
                    vRLPhilips = 0.0;
                    vAPPhilips = 0.0;
                    vFHPhilips = 0.0;
                }
                //if ((MRImageGradientOrientationNumber > 0) && ((gradNum != MRImageGradientOrientationNumber)) break;
                /*if (gradNum < minGradNum) minGradNum = gradNum;
                if (gradNum >= maxGradNum) maxGradNum = gradNum;
                if (gradNum >= kMaxDTI4D) {
                        printError("Number of DTI gradients exceeds 'kMaxDTI4D (%d).\n", kMaxDTI4D);
                } else {
                    gradNum = gradNum - 1;//index from 0
                    philDTI[gradNum].V[0] = B0Philips;
                    philDTI[gradNum].V[1] = vRLPhilips;
                    philDTI[gradNum].V[2] = vAPPhilips;
                    philDTI[gradNum].V[3] = vFHPhilips;
                }*/
                dcmDim[numDimensionIndexValues].V[0] = B0Philips;
                dcmDim[numDimensionIndexValues].V[1] = vRLPhilips;
                dcmDim[numDimensionIndexValues].V[2] = vAPPhilips;
                dcmDim[numDimensionIndexValues].V[3] = vFHPhilips;
                isPhilipsDerived = false;
                //printMessage(" DimensionIndexValues grad %d b=%g vec=%gx%gx%g\n", gradNum, B0Philips, vRLPhilips, vAPPhilips, vFHPhilips);
                //!!! 16032018 : next line as well as definition of B0Philips may need to be set to zero if Philips omits DiffusionBValue tag for B=0
                B0Philips = -1.0; //Philips may skip reporting B-values for B=0 volumes, so zero these
                vRLPhilips = 0.0;
                vAPPhilips = 0.0;
                vFHPhilips = 0.0;
                //MRImageGradientOrientationNumber = 0;
            }//diffusion parameters
            numDimensionIndexValues ++;
            nDimIndxVal = -1; //we need DimensionIndexValues
        } //record dimensionIndexValues slice information
    } //groupElement == kItemDelimitationTag : delimit item exits folder
    if (groupElement == kItemTag) {
        uint32_t slen = dcmInt(4,&buffer[lPos],d.isLittleEndian);
        uint32_t kUndefinedLen = 0xFFFFFFFF;
        if (slen != kUndefinedLen) {
            nNestPos++;
            if (nNestPos >= kMaxNestPost) nNestPos = kMaxNestPost - 1;
            nestPos[nNestPos] = slen+lFileOffset+lPos;
        }
        lLength = 4;
        sqDepth++;
        //return d;
    } else if (( (groupElement == kItemDelimitationTag) || (groupElement == kSequenceDelimitationItemTag)) && (!isEncapsulatedData)) {
            vr[0] = 'N';
            vr[1] = 'A';
            lLength = 4;
        } else if (d.isExplicitVR) {
            vr[0] = buffer[lPos]; vr[1] = buffer[lPos+1];
            if (buffer[lPos+1] < 'A') {//implicit vr with 32-bit length
                if (d.isLittleEndian)
                    lLength = buffer[lPos] | (buffer[lPos+1] << 8) | (buffer[lPos+2] << 16) | (buffer[lPos+3] << 24);
                else
                    lLength = buffer[lPos+3] | (buffer[lPos+2] << 8) | (buffer[lPos+1] << 16) | (buffer[lPos] << 24);
                lPos += 4;
            } else if ( ((buffer[lPos] == 'U') && (buffer[lPos+1] == 'N'))
                       || ((buffer[lPos] == 'U') && (buffer[lPos+1] == 'T'))
                       || ((buffer[lPos] == 'O') && (buffer[lPos+1] == 'B'))
                       || ((buffer[lPos] == 'O') && (buffer[lPos+1] == 'W'))
                       ) { //VR= UN, OB, OW, SQ  || ((buffer[lPos] == 'S') && (buffer[lPos+1] == 'Q'))
                lPos = lPos + 4;  //skip 2 byte VR string and 2 reserved bytes = 4 bytes
                if (d.isLittleEndian)
                    lLength = buffer[lPos] | (buffer[lPos+1] << 8) | (buffer[lPos+2] << 16) | (buffer[lPos+3] << 24);
                else
                    lLength = buffer[lPos+3] | (buffer[lPos+2] << 8) | (buffer[lPos+1] << 16) | (buffer[lPos] << 24);
                lPos = lPos + 4;  //skip 4 byte length
            } else if   ((buffer[lPos] == 'S') && (buffer[lPos+1] == 'Q')) {
                lLength = 8; //Sequence Tag
                //printMessage(" !!!SQ\t%04x,%04x\n",   groupElement & 65535,groupElement>>16);
            } else { //explicit VR with 16-bit length
                if ((d.isLittleEndian)  )
                    lLength = buffer[lPos+2] | (buffer[lPos+3] << 8);
                else
                    lLength = buffer[lPos+3] | (buffer[lPos+2] << 8);
                lPos += 4;  //skip 2 byte VR string and 2 length bytes = 4 bytes
            }
        } else { //implicit VR
            vr[0] = 'U';
            vr[1] = 'N';
            if (d.isLittleEndian)
                lLength = buffer[lPos] | (buffer[lPos+1] << 8) | (buffer[lPos+2] << 16) | (buffer[lPos+3] << 24);
            else
                lLength = buffer[lPos+3] | (buffer[lPos+2] << 8) | (buffer[lPos+1] << 16) | (buffer[lPos] << 24);
            lPos += 4;  //we have loaded the 32-bit length
            if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isSQ(groupElement))) { //https://github.com/rordenlab/dcm2niix/issues/144
                vr[0] = 'S';
                vr[1] = 'Q';
                lLength = 0; //Do not skip kItemTag - required to determine nesting of Philips Enhanced
            }
        } //if explicit else implicit VR
        if (lLength == 0xFFFFFFFF) {
            lLength = 8; //SQ (Sequences) use 0xFFFFFFFF [4294967295] to denote unknown length
            //09032018 - do not count these as SQs: Horos does not count even groups
            //uint32_t special = dcmInt(4,&buffer[lPos],d.isLittleEndian);

            //http://dicom.nema.org/dicom/2013/output/chtml/part05/sect_7.5.html

            //if (special != ksqDelim) {
                vr[0] = 'S';
                vr[1] = 'Q';
            //}
        }
        /* //Handle SQs: for explicit these have VR=SQ
        if   ((vr[0] == 'S') && (vr[1] == 'Q')) {
            //http://dicom.nema.org/dicom/2013/output/chtml/part05/sect_7.5.html
            uint32_t special = dcmInt(4,&buffer[lPos],d.isLittleEndian);
            uint32_t slen = dcmInt(4,&buffer[lPos+4],d.isLittleEndian);
            //if (d.isExplicitVR)
            //  slen = dcmInt(4,&buffer[lPos+8],d.isLittleEndian);
            uint32_t kUndefinedLen = 0xFFFFFFFF;
            //printError(" SPECIAL >>>>t%04x,%04x  %08x %08x\n",   groupElement & 65535,groupElement>>16, special, slen);
            //return d;
            is2005140FSQ = (groupElement == kPrivatePerFrameSq);
            //if (isNextSQis2005140FSQ) is2005140FSQ = true;
            //isNextSQis2005140FSQ = false;
            if (special == kSequenceDelimitationItemTag) {
                //unknown
            } else if (slen == kUndefinedLen) {
                sqDepth++;
                if ((sqDepthPrivate == 0) && ((groupElement & 65535) % 2))
                    sqDepthPrivate = sqDepth; //in a private SQ: ignore contents
            } else if ((is2005140FSQ) || ((groupElement & 65535) % 2)) {//private SQ of known length - lets jump over this!
                slen = lFileOffset + lPos + slen;
                if ((sqEndPrivate < 0) || (slen > sqEndPrivate)) //sqEndPrivate is signed
                    sqEndPrivate = slen; //if nested private SQs, remember the end address of the top parent SQ
            }
        }
        //next: look for required tags
        if ((groupElement == kItemTag) && (isEncapsulatedData)) {
            d.imageBytes = dcmInt(4,&buffer[lPos],d.isLittleEndian);
            printMessage("compressed data %d-> %ld\n",d.imageBytes, lPos);

            d.imageBytes = dcmInt(4,&buffer[lPos-4],d.isLittleEndian);
            printMessage("compressed data %d-> %ld\n",d.imageBytes, lPos);
            if (d.imageBytes > 128) {
                encapsulatedDataFragments++;
                if (encapsulatedDataFragmentStart == 0)
                    encapsulatedDataFragmentStart = (int)lPos + (int)lFileOffset;
            }
        }
        if ((sqEndPrivate > 0) && ((lFileOffset + lPos) > sqEndPrivate))
            sqEndPrivate = -1; //end of private SQ with defined length
        if (groupElement == kSequenceDelimitationItemTag) { //end of private SQ with undefined length
            sqDepth--;
            if (sqDepth < sqDepthPrivate) {
                sqDepthPrivate = 0; //no longer in a private SQ
            }
        }
        if (sqDepth < 0) sqDepth = 0;*/
        if ((groupElement == kItemTag)  && (isEncapsulatedData)) { //use this to find image fragment for compressed datasets, e.g. JPEG transfer syntax
            d.imageBytes = dcmInt(4,&buffer[lPos],d.isLittleEndian);
            lPos = lPos + 4;
            lLength = d.imageBytes;
            if (d.imageBytes > 128) {
                encapsulatedDataFragments++;
                if (encapsulatedDataFragmentStart == 0)
                    encapsulatedDataFragmentStart = (int)lPos + (int)lFileOffset;
            }
        }
        if ((isIconImageSequence) && ((groupElement & 0x0028) == 0x0028 )) groupElement = kUnused; //ignore icon dimensions
        switch ( groupElement ) {
            case kTransferSyntax: {
                char transferSyntax[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], transferSyntax);
                if (strcmp(transferSyntax, "1.2.840.10008.1.2.1") == 0)
                    ; //default isExplicitVR=true; //d.isLittleEndian=true
                else if  (strcmp(transferSyntax, "1.2.840.10008.1.2.4.50") == 0) {
                    d.compressionScheme = kCompress50;
                    //printMessage("Lossy JPEG: please decompress with Osirix or dcmdjpg. %s\n", transferSyntax);
                    //d.imageStart = 1;//abort as invalid (imageStart MUST be >128)
                } else if (strcmp(transferSyntax, "1.2.840.10008.1.2.4.51") == 0) {
                        d.compressionScheme = kCompress50;
                        //printMessage("Lossy JPEG: please decompress with Osirix or dcmdjpg. %s\n", transferSyntax);
                        //d.imageStart = 1;//abort as invalid (imageStart MUST be >128)
                //uJPEG does not decode these: ..53 ...55
                // } else if (strcmp(transferSyntax, "1.2.840.10008.1.2.4.53") == 0) {
                //    d.compressionScheme = kCompress50;
                } else if (strcmp(transferSyntax, "1.2.840.10008.1.2.4.57") == 0) {
                    //d.isCompressed = true;
                    //https://www.medicalconnections.co.uk/kb/Transfer_Syntax should be SOF = 0xC3
                    d.compressionScheme = kCompressC3;
                    //printMessage("Ancient JPEG-lossless (SOF type 0xc3): please check conversion\n");
                } else if (strcmp(transferSyntax, "1.2.840.10008.1.2.4.70") == 0) {
                    d.compressionScheme = kCompressC3;
                } else if ((strcmp(transferSyntax, "1.2.840.10008.1.2.4.80") == 0)  || (strcmp(transferSyntax, "1.2.840.10008.1.2.4.81") == 0)){
                    #if defined(myEnableJPEGLS) || defined(myEnableJPEGLS1)
                    d.compressionScheme = kCompressJPEGLS;
                    #else
                    printMessage("Unsupported transfer syntax '%s' (decode with 'dcmdjpls jpg.dcm raw.dcm' or 'gdcmconv -w jpg.dcm raw.dcm', or recompile dcm2niix with JPEGLS support)\n",transferSyntax);
                    d.imageStart = 1;//abort as invalid (imageStart MUST be >128)
                    #endif
                } else if (strcmp(transferSyntax, "1.3.46.670589.33.1.4.1") == 0) {
                    d.compressionScheme = kCompressPMSCT_RLE1;
                    //printMessage("Unsupported transfer syntax '%s' (decode with rle2img)\n",transferSyntax);
                    //d.imageStart = 1;//abort as invalid (imageStart MUST be >128)
                } else if ((compressFlag != kCompressNone) && (strcmp(transferSyntax, "1.2.840.10008.1.2.4.90") == 0)) {
                    d.compressionScheme = kCompressYes;
                    //printMessage("JPEG2000 Lossless support is new: please validate conversion\n");
                } else if ((compressFlag != kCompressNone) && (strcmp(transferSyntax, "1.2.840.10008.1.2.4.91") == 0)) {
                    d.compressionScheme = kCompressYes;
                    //printMessage("JPEG2000 support is new: please validate conversion\n");
                } else if (strcmp(transferSyntax, "1.2.840.10008.1.2.5") == 0)
                    d.compressionScheme = kCompressRLE; //run length
                else if (strcmp(transferSyntax, "1.2.840.10008.1.2.2") == 0)
                    isSwitchToBigEndian = true; //isExplicitVR=true;
                else if (strcmp(transferSyntax, "1.2.840.10008.1.2") == 0)
                    isSwitchToImplicitVR = true; //d.isLittleEndian=true
                else {
                    printMessage("Unsupported transfer syntax '%s' (see www.nitrc.org/plugins/mwiki/index.php/dcm2nii:MainPage)\n",transferSyntax);
                    d.imageStart = 1;//abort as invalid (imageStart MUST be >128)
                }
                break;} //{} provide scope for variable 'transferSyntax
            /*case kImplementationVersionName: {
                char impTxt[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], impTxt);
                int slen = (int) strlen(impTxt);
                if((slen < 6) || (strstr(impTxt, "OSIRIX") == NULL) ) break;
                printError("OSIRIX Detected\n");
                break; }*/
            case kSourceApplicationEntityTitle: {
                char saeTxt[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], saeTxt);
                int slen = (int) strlen(saeTxt);
                if((slen < 5) || (strstr(saeTxt, "oasis") == NULL) ) break;
                d.isSegamiOasis = true;
                break; }
            case kImageTypeTag:
                dcmStr (lLength, &buffer[lPos], d.imageType);
                int slen;
                slen = (int) strlen(d.imageType);
                //if (strcmp(transferSyntax, "ORIGINAL_PRIMARY_M_ND_MOSAIC") == 0)
                if((slen > 5) && !strcmp(d.imageType + slen - 6, "MOSAIC") )
                    isMosaic = true;
                //isNonImage 0008,0008 = DERIVED,CSAPARALLEL,POSDISP
                // sometime ComplexImageComponent 0008,9208 is missing - see ADNI data
                // attempt to detect non-images, see https://github.com/scitran/data/blob/a516fdc39d75a6e4ac75d0e179e18f3a5fc3c0af/scitran/data/medimg/dcm/mr/siemens.py
                //For Philips combinations see Table 3-28 Table 3-28: Valid combinations of Image Type applied values
                //  http://incenter.medical.philips.com/doclib/enc/fetch/2000/4504/577242/577256/588723/5144873/5144488/5144982/DICOM_Conformance_Statement_Intera_R7%2c_R8_and_R9.pdf%3fnodeid%3d5147977%26vernum%3d-2
                if((slen > 3) && (strstr(d.imageType, "_R_") != NULL) ) {
                    d.isHasReal = true;
                    isReal = true;
                }
                if((slen > 3) && (strstr(d.imageType, "_I_") != NULL) ) {
                    d.isHasImaginary = true;
                    isImaginary = true;
                }
                if((slen > 3) && (strstr(d.imageType, "_P_") != NULL) ) {
                    d.isHasPhase = true;
                    isPhase = true;
                }
                if((slen > 6) && (strstr(d.imageType, "_REAL_") != NULL) ) {
                    d.isHasReal = true;
                    isReal = true;
                }
                if((slen > 11) && (strstr(d.imageType, "_IMAGINARY_") != NULL) ) {
                    d.isHasImaginary = true;
                    isImaginary = true;
                }
                if((slen > 6) && (strstr(d.imageType, "PHASE") != NULL) ) {
                    d.isHasPhase = true;
                    isPhase = true;
                }
                if((slen > 6) && (strstr(d.imageType, "DERIVED") != NULL) )
                    d.isDerived = true;
                //if((slen > 4) && (strstr(typestr, "DIS2D") != NULL) )
                //  d.isNonImage = true;
                //not mutually exclusive: possible for Philips enhanced DICOM to store BOTH magnitude and phase in the same image
                break;
            case kAcquisitionDate:
                char acquisitionDateTxt[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], acquisitionDateTxt);
                d.acquisitionDate = atof(acquisitionDateTxt);
                break;
            case kAcquisitionDateTime:
                //char acquisitionDateTimeTxt[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], acquisitionDateTimeTxt);
                //printMessage("%s\n",acquisitionDateTimeTxt);
                break;
            case kStudyDate:
                dcmStr (lLength, &buffer[lPos], d.studyDate);
                break;
            case kModality:
                if (lLength < 2) break;
                if ((buffer[lPos]=='C') && (toupper(buffer[lPos+1]) == 'R'))
                    d.modality = kMODALITY_CR;
                else if ((buffer[lPos]=='C') && (toupper(buffer[lPos+1]) == 'T'))
                    d.modality = kMODALITY_CT;
                if ((buffer[lPos]=='M') && (toupper(buffer[lPos+1]) == 'R'))
                    d.modality = kMODALITY_MR;
                if ((buffer[lPos]=='P') && (toupper(buffer[lPos+1]) == 'T'))
                    d.modality = kMODALITY_PT;
                if ((buffer[lPos]=='U') && (toupper(buffer[lPos+1]) == 'S'))
                    d.modality = kMODALITY_US;
                break;
            case kManufacturer:
                d.manufacturer = dcmStrManufacturer (lLength, &buffer[lPos]);
                volDiffusion.manufacturer = d.manufacturer;
                break;
            case kInstitutionName:
                dcmStr(lLength, &buffer[lPos], d.institutionName);
                break;
            case kInstitutionAddress:
                dcmStr(lLength, &buffer[lPos], d.institutionAddress);
                break;
            case kReferringPhysicianName:
                dcmStr(lLength, &buffer[lPos], d.referringPhysicianName);
                break;
            case kComplexImageComponent:
                if (is2005140FSQ) break; //see Maastricht DICOM data for magnitude data with this field set as REAL!  https://www.nitrc.org/plugins/mwiki/index.php/dcm2nii:MainPage#Diffusion_Tensor_Imaging
                if (lLength < 2) break;
                isPhase = false;
                isReal = false;
                isImaginary = false;
                isMagnitude = false;
                //see Table C.8-85 http://dicom.nema.org/medical/Dicom/2017c/output/chtml/part03/sect_C.8.13.3.html
                if ((buffer[lPos]=='R') && (toupper(buffer[lPos+1]) == 'E'))
                    isReal = true;
                if ((buffer[lPos]=='I') && (toupper(buffer[lPos+1]) == 'M'))
                    isImaginary = true;
                if ((buffer[lPos]=='P') && (toupper(buffer[lPos+1]) == 'H'))
                    isPhase = true;
                if ((buffer[lPos]=='M') && (toupper(buffer[lPos+1]) == 'A'))
                    isMagnitude = true;
                //not mutually exclusive: possible for Philips enhanced DICOM to store BOTH magnitude and phase in the same image
                if (isPhase) d.isHasPhase = true;
                if (isReal) d.isHasReal = true;
                if (isImaginary) d.isHasImaginary = true;
                if (isMagnitude) d.isHasMagnitude = true;
                break;
            case kAcquisitionTime :
                char acquisitionTimeTxt[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], acquisitionTimeTxt);
                d.acquisitionTime = atof(acquisitionTimeTxt);
                if (d.manufacturer != kMANUFACTURER_UIH) break;
                //UIH slice timing
                d.CSA.sliceTiming[acquisitionTimesUIH] = d.acquisitionTime;
                //printf("%g\n", d.CSA.sliceTiming[acquisitionTimesUIH]);
                acquisitionTimesUIH ++;
                break;
            case kContentTime :
                char contentTimeTxt[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], contentTimeTxt);
                contentTime = atof(contentTimeTxt);
                break;
            case kStudyTime :
                dcmStr (lLength, &buffer[lPos], d.studyTime);
                break;
            case kPatientName :
                dcmStr (lLength, &buffer[lPos], d.patientName);
                break;
            case kAnatomicalOrientationType: {
                char aotTxt[kDICOMStr]; //ftp://dicom.nema.org/MEDICAL/dicom/2015b/output/chtml/part03/sect_C.7.6.2.html#sect_C.7.6.2.1.1
                dcmStr (lLength, &buffer[lPos], aotTxt);
                int slen = (int) strlen(aotTxt);
                if((slen < 9) || (strstr(aotTxt, "QUADRUPED") == NULL) ) break;
                printError("Anatomical Orientation Type (0010,2210) is QUADRUPED: rotate coordinates accordingly");
                break; }
            case kPatientID :
                dcmStr (lLength, &buffer[lPos], d.patientID);
                break;
            case kPatientBirthDate :
                dcmStr (lLength, &buffer[lPos], d.patientBirthDate);
                break;
            case kPatientSex :
                d.patientSex = toupper(buffer[lPos]); //first character is either 'R'ow or 'C'ol
                break;
            case kPatientAge :
                dcmStr (lLength, &buffer[lPos], d.patientAge);
                break;
            case kPatientWeight :
                d.patientWeight = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kStationName :
                dcmStr (lLength, &buffer[lPos], d.stationName);
                break;
            case kSeriesDescription: {
                dcmStr (lLength, &buffer[lPos], d.seriesDescription);
                break; }
            case kInstitutionalDepartmentName:
                dcmStr (lLength, &buffer[lPos], d.institutionalDepartmentName);
                break;
            case kManufacturersModelName :
                dcmStr (lLength, &buffer[lPos], d.manufacturersModelName);
                break;
            case kDerivationDescription : {
                //strcmp(transferSyntax, "1.2.840.10008.1.2")
                char derivationDescription[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], derivationDescription);//strcasecmp, strcmp
                if (strcasecmp(derivationDescription, "MEDCOM_RESAMPLED") == 0) d.isResampled = true;
                break;
            }
            case kDeviceSerialNumber : {
                dcmStr (lLength, &buffer[lPos], d.deviceSerialNumber);
                break;
            }
            case kSoftwareVersions : {
                dcmStr (lLength, &buffer[lPos], d.softwareVersions);
                break;
            }
            case kProtocolName : {
                //if ((strlen(d.protocolName) < 1) || (d.manufacturer != kMANUFACTURER_GE)) //GE uses a generic session name here: do not overwrite kProtocolNameGE
                dcmStr (lLength, &buffer[lPos], d.protocolName); //see also kSequenceName
                break; }
            case kPatientOrient :
                dcmStr (lLength, &buffer[lPos], d.patientOrient);
                break;
            case kDiffusionDirectionality : {// 0018, 9075
                set_directionality0018_9075(&volDiffusion, (&buffer[lPos]));
                if ((d.manufacturer != kMANUFACTURER_PHILIPS) || (lLength < 10)) break;
                char dir[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], dir);
                if (strcmp(dir, "ISOTROPIC") == 0)
                    isPhilipsDerived = true;
                break; }
            case kMREchoSequence :
                if (sqDepth == 0) sqDepth = 1; //should not happen, in case faulty anonymization
                sqDepth00189114 = sqDepth - 1;
                break;
            case kNumberOfImagesInMosaic :
                if (d.manufacturer == kMANUFACTURER_SIEMENS)
                    numberOfImagesInMosaic =  dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kDwellTime :
                d.dwellTime  =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kLastScanLoc :
                d.lastScanLoc = dcmStrFloat(lLength, &buffer[lPos]);
                break;
                /*case kDiffusionBFactorSiemens :
                 if (d.manufacturer == kMANUFACTURER_SIEMENS)
                 printMessage("last scan location %f\n,",dcmStrFloat(lLength, &buffer[lPos]));

                 break;*/
            case kDiffusionDirectionGEX :
                if (d.manufacturer == kMANUFACTURER_GE)
                  set_diffusion_directionGE(&volDiffusion, lLength, (&buffer[lPos]), 0);
                break;
            case kDiffusionDirectionGEY :
                if (d.manufacturer == kMANUFACTURER_GE)
                  set_diffusion_directionGE(&volDiffusion, lLength, (&buffer[lPos]), 1);
                break;
            case kDiffusionDirectionGEZ :
                if (d.manufacturer == kMANUFACTURER_GE)
                  set_diffusion_directionGE(&volDiffusion, lLength, (&buffer[lPos]), 2);
                break;
            case kBandwidthPerPixelPhaseEncode:
                d.bandwidthPerPixelPhaseEncode = dcmFloatDouble(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            //GE bug: multiple echos can create identical instance numbers
            //  in theory, one could detect as kRawDataRunNumberGE varies
            //  sliceN of echoE will have the same value for all timepoints
            //  this value does not appear indexed
            //  different echoes record same echo time.
            //  use multiEchoSortGEDICOM.py to salvage
            //case kRawDataRunNumberGE :
            //  if (d.manufacturer != kMANUFACTURER_GE)
            //      break;
            //    d.rawDataRunNumberGE = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
            //    break;
            case kStudyInstanceUID : // 0020, 000D
                dcmStr (lLength, &buffer[lPos], d.studyInstanceUID);
                break;
            case kSeriesInstanceUID : // 0020, 000E
                dcmStr (lLength, &buffer[lPos], d.seriesInstanceUID);
                break;
            case kImagePositionPatient : {
                if (is2005140FSQ) {
                    dcmMultiFloat(lLength, (char*)&buffer[lPos], 3, &patientPositionPrivate[0]);
                    break;
                }
                patientPositionNum++;
                isAtFirstPatientPosition = true;
                dcmMultiFloat(lLength, (char*)&buffer[lPos], 3, &patientPosition[0]); //slice position
                if (isnan(d.patientPosition[1])) {
                    //dcmMultiFloat(lLength, (char*)&buffer[lPos], 3, &d.patientPosition[0]); //slice position
                    for (int k = 0; k < 4; k++)
                        d.patientPosition[k] = patientPosition[k];
                } else {
                    //dcmMultiFloat(lLength, (char*)&buffer[lPos], 3, &d.patientPositionLast[0]); //slice direction for 4D
                    for (int k = 0; k < 4; k++)
                        d.patientPositionLast[k] = patientPosition[k];
                    if ((isFloatDiff(d.patientPositionLast[1],d.patientPosition[1]))  ||
                        (isFloatDiff(d.patientPositionLast[2],d.patientPosition[2]))  ||
                        (isFloatDiff(d.patientPositionLast[3],d.patientPosition[3])) ) {
                        isAtFirstPatientPosition = false; //this slice is not at position of 1st slice
                        //if (d.patientPositionSequentialRepeats == 0) //this is the first slice with different position
                        //  d.patientPositionSequentialRepeats = patientPositionNum-1;
                    } //if different position from 1st slice in file
                } //if not first slice in file
                set_isAtFirstPatientPosition_tvd(&volDiffusion, isAtFirstPatientPosition);
                if (isVerbose == 1) //verbose > 1 will report full DICOM tag
                    printMessage("   Patient Position 0020,0032 (#,@,X,Y,Z)\t%d\t%ld\t%g\t%g\t%g\n", patientPositionNum, lPos, patientPosition[1], patientPosition[2], patientPosition[3]);
                break; }
            case kInPlanePhaseEncodingDirection:
                d.phaseEncodingRC = toupper(buffer[lPos]); //first character is either 'R'ow or 'C'ol
                break;
            case kSAR:
                d.SAR = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kStudyID:
                dcmStr (lLength, &buffer[lPos], d.studyID);
                break;
            case kSeriesNum:
                d.seriesNum =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kAcquNum:
                d.acquNum = dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kImageNum:
                //Enhanced Philips also uses this in once per file SQ 0008,1111
                //Enhanced Philips also uses this once per slice in SQ 2005,140f
                if (d.imageNum < 1) d.imageNum = dcmStrInt(lLength, &buffer[lPos]);  //Philips renames each image again in 2001,9000, which can lead to duplicates
                break;
            case kInStackPositionNumber:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                inStackPositionNumber = dcmInt(4,&buffer[lPos],d.isLittleEndian);
                if (inStackPositionNumber > maxInStackPositionNumber) maxInStackPositionNumber = inStackPositionNumber;
                break;
            case kTriggerDelayTime: { //0x0020+uint32_t(0x9153<< 16 ) //FD
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                //if (isVerbose < 2) break;
                double trigger = dcmFloatDouble(lLength, &buffer[lPos],d.isLittleEndian);
                d.triggerDelayTime = trigger;
                if (isSameFloatGE(d.triggerDelayTime, 0.0)) d.triggerDelayTime = 0.0; //double to single
                break; }
            case kDimensionIndexValues: { // kImageNum is not enough for 4D series from Philips 5.*.
                if (lLength < 4) break;
                nDimIndxVal = lLength / 4;
                if(nDimIndxVal > MAX_NUMBER_OF_DIMENSIONS){
                    printError("%d is too many dimensions.  Only up to %d are supported\n", nDimIndxVal,
                               MAX_NUMBER_OF_DIMENSIONS);
                    nDimIndxVal = MAX_NUMBER_OF_DIMENSIONS;  // Truncate
                }
                dcmMultiLongs(4 * nDimIndxVal, &buffer[lPos], nDimIndxVal, d.dimensionIndexValues, d.isLittleEndian);
                break; }
            case kPhotometricInterpretation: {
                char interp[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], interp);
                if (strcmp(interp, "PALETTE_COLOR") == 0)
                    printError("Photometric Interpretation 'PALETTE COLOR' not supported\n");
                break; }
            case kPlanarRGB:
                d.isPlanarRGB = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kDim3:
                d.xyzDim[3] = dcmStrInt(lLength, &buffer[lPos]);
                numberOfFrames = d.xyzDim[3];
                break;
            case kSamplesPerPixel:
                d.samplesPerPixel = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kDim2:
                d.xyzDim[2] = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kDim1:
                d.xyzDim[1] = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kXYSpacing:
                dcmMultiFloat(lLength, (char*)&buffer[lPos], 2, d.xyzMM);
                break;
            case kImageComments:
                dcmStr (lLength, &buffer[lPos], d.imageComments, true);
                break;
            case kLocationsInAcquisitionGE:
                locationsInAcquisitionGE = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kRTIA_timer:
                if (d.manufacturer != kMANUFACTURER_GE) break;
                //see dicm2nii slice timing from 0021,105E DS RTIA_timer
                d.rtia_timerGE =  dcmStrFloat(lLength, &buffer[lPos]); //RefAcqTimes = t/10; end % in ms
                //printf("%s\t%g\n", fname, d.rtia_timerGE);
                break;
            case kProtocolDataBlockGE :
                if (d.manufacturer != kMANUFACTURER_GE) break;
                d.protocolBlockLengthGE = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                d.protocolBlockStartGE = (int)lPos+(int)lFileOffset+4;
                //printError("ProtocolDataBlockGE %d  @ %d\n", d.protocolBlockLengthGE, d.protocolBlockStartGE);
                break;
            case kDoseCalibrationFactor :
                d.doseCalibrationFactor = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kPETImageIndex :
                PETImageIndex = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kPEDirectionDisplayedUIH :
                if (d.manufacturer != kMANUFACTURER_UIH) break;
                dcmStr (lLength, &buffer[lPos], d.phaseEncodingDirectionDisplayedUIH);
                break;
            case kDiffusion_bValueUIH : {
                if (d.manufacturer != kMANUFACTURER_UIH) break;
                float v[4];
                dcmMultiFloatDouble(lLength, &buffer[lPos], 1, v, d.isLittleEndian);
                d.CSA.dtiV[0] = v[0];
                d.CSA.numDti = 1;
                //printf("%d>>>%g\n", lPos, v[0]);
                break; }
            case kNumberOfImagesInGridUIH :
                if (d.manufacturer != kMANUFACTURER_UIH) break;
                d.numberOfImagesInGridUIH =  dcmStrFloat(lLength, &buffer[lPos]);
                d.CSA.mosaicSlices = d.numberOfImagesInGridUIH;
                break;
            case kDiffusionGradientDirectionUIH : { //0065,1037
            //0.03712929804225321\-0.5522387869760447\-0.8328587749392602
                if (d.manufacturer != kMANUFACTURER_UIH) break;
                float v[4];
                dcmMultiFloatDouble(lLength, &buffer[lPos], 3, v, d.isLittleEndian);
                //dcmMultiFloat(lLength, (char*)&buffer[lPos], 3, v);
                //printf(">>>%g %g %g\n", v[0], v[1], v[2]);
                d.CSA.dtiV[1] = v[0];
                d.CSA.dtiV[2] = v[1];
                d.CSA.dtiV[3] = v[2];
                    //vRLPhilips = v[0];
                    //vAPPhilips = v[1];
                    //vFHPhilips = v[2];
                break; }

            case kBitsAllocated :
                d.bitsAllocated = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kBitsStored :
                d.bitsStored = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kIsSigned : //http://dicomiseasy.blogspot.com/2012/08/chapter-12-pixel-data.html
                d.isSigned = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kTR :
                d.TR = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kTE :
                TE = dcmStrFloat(lLength, &buffer[lPos]);
                if (d.TE <= 0.0)
                    d.TE = TE;
                break;
            case kEffectiveTE : {
                TE = dcmFloatDouble(lLength, &buffer[lPos], d.isLittleEndian);
                if (d.TE <= 0.0)
                    d.TE = TE;
                break; }
            case kTI :
                d.TI = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kEchoNum :
                d.echoNum =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kMagneticFieldStrength :
                d.fieldStrength =  dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kZSpacing :
                d.zSpacing = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kPhaseEncodingSteps :
                d.phaseEncodingSteps =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kEchoTrainLength :
                d.echoTrainLength  =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kPhaseFieldofView :
                d.phaseFieldofView = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kPixelBandwidth :
                /*if (d.manufacturer == kMANUFACTURER_PHILIPS) {
                    //Private SQs can report different (more precise?) pixel bandwidth values than in the main header!
                    //  https://github.com/rordenlab/dcm2niix/issues/170
                    if (is2005140FSQ) break;
                    if ((lFileOffset + lPos) < sqEndPrivate) break; //inside private SQ, SQ has defined length
                    if (sqDepthPrivate > 0) break; //inside private SQ, SQ has undefined length
                }*/
                d.pixelBandwidth = dcmStrFloat(lLength, &buffer[lPos]);
                //printWarning(" PixelBandwidth (0018,0095)====> %g @%d\n", d.pixelBandwidth, lPos);
                break;
            case kAcquisitionMatrix :
                if (lLength == 8) {
                    uint16_t acquisitionMatrix[4];
                    dcmMultiShorts(lLength, &buffer[lPos], 4, &acquisitionMatrix[0],d.isLittleEndian); //slice position
                    //phaseEncodingLines stored in either image columns or rows
                    if (acquisitionMatrix[3] > 0)
                        d.phaseEncodingLines = acquisitionMatrix[3];
                    if (acquisitionMatrix[2] > 0)
                        d.phaseEncodingLines = acquisitionMatrix[2];
                }
                break;
            case kFlipAngle :
                d.flipAngle = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kRadionuclideTotalDose :
                d.radionuclideTotalDose = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kRadionuclideHalfLife :
                d.radionuclideHalfLife = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kRadionuclidePositronFraction :
                d.radionuclidePositronFraction = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kGantryTilt :
                d.gantryTilt = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kXRayExposure : //CTs do not have echo times, we use this field to detect different exposures: https://github.com/neurolabusc/dcm2niix/pull/48
                if (d.TE == 0) {// for CT we will use exposure (0018,1152) whereas for MR we use echo time (0018,0081)
                    d.isXRay = true;
                    d.TE = dcmStrFloat(lLength, &buffer[lPos]);
                }
                break;
            case kSlope :
                d.intenScale = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kPhilipsSlope :
                if ((lLength == 4) && (d.manufacturer == kMANUFACTURER_PHILIPS))
                    d.intenScalePhilips = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case kIntercept :
                d.intenIntercept = dcmStrFloat(lLength, &buffer[lPos]);
                break;
            case kZThick :
                d.xyzMM[3] = dcmStrFloat(lLength, &buffer[lPos]);
                d.zThick = d.xyzMM[3];
                break;
             case kAcquisitionMatrixText : {
               if (d.manufacturer == kMANUFACTURER_SIEMENS) {
                    char matStr[kDICOMStr];
                    dcmStr (lLength, &buffer[lPos], matStr);
                    char* pPosition = strchr(matStr, 'I');
                    if (pPosition != NULL)
                        printWarning("interpolated data may exhibit Gibbs ringing and be unsuitable for dwidenoise/mrdegibbs. \n");
                }
               break; }
            case kCoilSiemens : {
                if (d.manufacturer == kMANUFACTURER_SIEMENS) {
                    //see if image from single coil "H12" or an array "HEA;HEP"
                    char coilStr[kDICOMStr];
                    dcmStr (lLength, &buffer[lPos], coilStr);
                    if (strlen(coilStr) < 1) break;
                    if (coilStr[0] == 'C') break; //kludge as Nova 32-channel defaults to "C:A32" https://github.com/rordenlab/dcm2niix/issues/187
                    char *ptr;
                    dcmStrDigitsOnly(coilStr);
                    //d.coilNum = (int)strtol(coilStr, &ptr, 10);
                    //if (*ptr != '\0') d.coilNum = 0;
                }
                break; }
            case kImaPATModeText : { //e.g. Siemens iPAT x2 listed as "p2"
                char accelStr[kDICOMStr];
                dcmStr (lLength, &buffer[lPos], accelStr);
                char *ptr;
                dcmStrDigitsOnlyKey('p', accelStr); //e.g. if "p2s4" return "2", if "s4" return ""
                d.accelFactPE = (float)strtof(accelStr, &ptr);
                if (*ptr != '\0')
                    d.accelFactPE = 0.0;
                //between slice accel
                dcmStr (lLength, &buffer[lPos], accelStr);
                dcmStrDigitsOnlyKey('s', accelStr); //e.g. if "p2s4" return "4", if "p2" return ""
                multiBandFactor = (int)strtol(accelStr, &ptr, 10);
                if (*ptr != '\0')
                    multiBandFactor = 0.0;
                //printMessage("p%gs%d\n",  d.accelFactPE, multiBandFactor);
                break; }
            case kLocationsInAcquisition :
                d.locationsInAcquisition = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kIconImageSequence:
                isIconImageSequence = true;
                break;
            /*case kStackSliceNumber: { //https://github.com/Kevin-Mattheus-Moerman/GIBBON/blob/master/dicomDict/PMS-R32-dict.txt
                int stackSliceNumber = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                printMessage("StackSliceNumber %d\n",stackSliceNumber);
                break;
            }*/
            case kNumberOfDynamicScans:
                //~d.numberOfDynamicScans =  dcmStrInt(lLength, &buffer[lPos]);
                numberOfDynamicScans =  dcmStrInt(lLength, &buffer[lPos]);

                break;
            case    kMRAcquisitionType: //detect 3D acquisition: we can reorient these without worrying about slice time correct or BVEC/BVAL orientation
                if (lLength > 1) d.is2DAcq = (buffer[lPos]=='2') && (toupper(buffer[lPos+1]) == 'D');
                if (lLength > 1) d.is3DAcq = (buffer[lPos]=='3') && (toupper(buffer[lPos+1]) == 'D');
                //dcmStr (lLength, &buffer[lPos], d.mrAcquisitionType);
                break;
            case kBodyPartExamined : {
                dcmStr (lLength, &buffer[lPos], d.bodyPartExamined);
                break;
            }
            case kScanningSequence : {
                dcmStr (lLength, &buffer[lPos], d.scanningSequence);
                break;
            }
            case kSequenceVariant : {
                dcmStr (lLength, &buffer[lPos], d.sequenceVariant);
                break;
            }
            case kScanOptions:
                dcmStr (lLength, &buffer[lPos], d.scanOptions);
                break;
            case kSequenceName : {
                //if (strlen(d.protocolName) < 1) //precedence given to kProtocolName and kProtocolNameGE
                dcmStr (lLength, &buffer[lPos], d.sequenceName);
                break;
            }
            case    kMRAcquisitionTypePhilips: //kMRAcquisitionType
                if (lLength > 1) d.is3DAcq = (buffer[lPos]=='3') && (toupper(buffer[lPos+1]) == 'D');
                break;
            case    kAngulationRL:
                d.angulation[1] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case    kAngulationAP:
                d.angulation[2] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case    kAngulationFH:
                d.angulation[3] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case    kMRStackOffcentreRL:
                d.stackOffcentre[1] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case    kMRStackOffcentreAP:
                d.stackOffcentre[2] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case    kMRStackOffcentreFH:
                d.stackOffcentre[3] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case    kSliceOrient: {
                char orientStr[kDICOMStr];
                orientStr[0] = 'X'; //avoid compiler warning: orientStr filled by dcmStr
                dcmStr (lLength, &buffer[lPos], orientStr);
                if (toupper(orientStr[0])== 'S')
                    d.sliceOrient = kSliceOrientSag; //sagittal
                else if (toupper(orientStr[0])== 'C')
                    d.sliceOrient = kSliceOrientCor; //coronal
                else
                    d.sliceOrient = kSliceOrientTra; //transverse (axial)
                break; }
            case kPMSCT_RLE1 :
                if (d.compressionScheme != kCompressPMSCT_RLE1) break;
                d.imageStart = (int)lPos + (int)lFileOffset;
                d.imageBytes = lLength;
                break;
            case kDiffusionBFactor :
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                B0Philips = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            // case kDiffusionBFactor: // 2001,1003
            //     if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition)) {
            //         d.CSA.numDti++; //increment with BFactor: on Philips slices with B=0 have B-factor but no diffusion directions
            //         if (d.CSA.numDti == 2) { //First time we know that this is a 4D DTI dataset
            //             //d.dti4D = (TDTI *)malloc(kMaxDTI4D * sizeof(TDTI));
            //             dti4D->S[0].V[0] = d.CSA.dtiV[0];
            //             dti4D->S[0].V[1] = d.CSA.dtiV[1];
            //             dti4D->S[0].V[2] = d.CSA.dtiV[2];
            //             dti4D->S[0].V[3] = d.CSA.dtiV[3];
            //         }
            //         d.CSA.dtiV[0] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
            //         if ((d.CSA.numDti > 1) && (d.CSA.numDti < kMaxDTI4D))
            //             dti4D->S[d.CSA.numDti-1].V[0] = d.CSA.dtiV[0];
            //         /*if ((d.CSA.numDti > 0) && (d.CSA.numDti <= kMaxDTIv))
            //            d.CSA.dtiV[d.CSA.numDti-1][0] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);*/
            //     }
            //     break;
            case kDiffusion_bValue:  // 0018, 9087
              // Note that this is ahead of kPatientPosition (0020,0032), so
              // isAtFirstPatientPosition is not necessarily set yet.
              // Philips uses this tag too, at least as of 5.1, but they also
              // use kDiffusionBFactor (see above), and we do not want to
              // double count.  More importantly, with Philips this tag
              // (sometimes?)  gets repeated in a nested sequence with the
              // value *unset*!
              // GE started using this tag in 27, and annoyingly, NOT including
              // the b value if it is 0 for the slice.

              //if((d.manufacturer != kMANUFACTURER_PHILIPS) || !is2005140FSQ){
                // d.CSA.numDti++;
                // if (d.CSA.numDti == 2) { //First time we know that this is a 4D DTI dataset
                //   //d.dti4D = (TDTI *)malloc(kMaxDTI4D * sizeof(TDTI));
                //   dti4D->S[0].V[0] = d.CSA.dtiV[0];
                //   dti4D->S[0].V[1] = d.CSA.dtiV[1];
                //   dti4D->S[0].V[2] = d.CSA.dtiV[2];
                //   dti4D->S[0].V[3] = d.CSA.dtiV[3];
                // }
                B0Philips = dcmFloatDouble(lLength, &buffer[lPos], d.isLittleEndian);

                //d.CSA.dtiV[0] = dcmFloatDouble(lLength, &buffer[lPos], d.isLittleEndian);
                set_bVal(&volDiffusion, dcmFloatDouble(lLength, &buffer[lPos], d.isLittleEndian));

                // if ((d.CSA.numDti > 1) && (d.CSA.numDti < kMaxDTI4D))
                //   dti4D->S[d.CSA.numDti-1].V[0] = d.CSA.dtiV[0];
              //}
              break;
            case kDiffusionOrientation:  // 0018, 9089
              // Note that this is ahead of kPatientPosition (0020,0032), so
              // isAtFirstPatientPosition is not necessarily set yet.
              // Philips uses this tag too, at least as of 5.1, but they also
              // use kDiffusionDirectionRL, etc., and we do not want to double
              // count.  More importantly, with Philips this tag (sometimes?)
              // gets repeated in a nested sequence with the value *unset*!
              // if (((d.manufacturer == kMANUFACTURER_SIEMENS) ||
              //      ((d.manufacturer == kMANUFACTURER_PHILIPS) && !is2005140FSQ)) &&
              //     (isAtFirstPatientPosition || isnan(d.patientPosition[1])))

              //if((d.manufacturer == kMANUFACTURER_SIEMENS) || ((d.manufacturer == kMANUFACTURER_PHILIPS) && !is2005140FSQ))
              if((d.manufacturer == kMANUFACTURER_SIEMENS) || (d.manufacturer == kMANUFACTURER_PHILIPS)) {
                float v[4];
                //dcmMultiFloat(lLength, (char*)&buffer[lPos], 3, v);
                //dcmMultiFloatDouble(lLength, &buffer[lPos], 3, v, d.isLittleEndian);
                dcmMultiFloatDouble(lLength, &buffer[lPos], 3, v, d.isLittleEndian);
                    vRLPhilips = v[0];
                    vAPPhilips = v[1];
                    vFHPhilips = v[2];

                set_orientation0018_9089(&volDiffusion, lLength, &buffer[lPos], d.isLittleEndian);
              }
              break;
            // case kSharedFunctionalGroupsSequence:
            //   if ((d.manufacturer == kMANUFACTURER_SIEMENS) && isAtFirstPatientPosition) {
            //     break; // For now - need to figure out how to get the nested
            //            // part of buffer[lPos].
            //   }
            //   break;

            //case kSliceNumberMrPhilips :
            //  sliceNumberMrPhilips3D = dcmStrInt(lLength, &buffer[lPos]);
            //  break;
            case kSliceNumberMrPhilips : {
                if (d.manufacturer != kMANUFACTURER_PHILIPS)
                    break;

                sliceNumberMrPhilips = dcmStrInt(lLength, &buffer[lPos]);
                int sliceNumber = sliceNumberMrPhilips;
                //use public patientPosition if it exists - fall back to private patient position
                if ((sliceNumber == 1) && (!isnan(patientPosition[1])) ) {
                    for (int k = 0; k < 4; k++)
                        patientPositionStartPhilips[k] = patientPosition[k];
                } else if ((sliceNumber == 1) && (!isnan(patientPositionPrivate[1]))) {
                    for (int k = 0; k < 4; k++)
                        patientPositionStartPhilips[k] = patientPositionPrivate[k];
                }
                if ((sliceNumber == locationsInAcquisitionPhilips) && (!isnan(patientPosition[1])) ) {
                    for (int k = 0; k < 4; k++)
                        patientPositionEndPhilips[k] = patientPosition[k];
                } else if ((sliceNumber == locationsInAcquisitionPhilips) && (!isnan(patientPositionPrivate[1])) ) {
                    for (int k = 0; k < 4; k++)
                        patientPositionEndPhilips[k] = patientPositionPrivate[k];
                }
                break; }
            case kNumberOfSlicesMrPhilips :
                if (d.manufacturer != kMANUFACTURER_PHILIPS)
                    break;
                locationsInAcquisitionPhilips = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                //printMessage("====> locationsInAcquisitionPhilips\t%d\n", locationsInAcquisitionPhilips);
                break;
            case kDiffusionDirectionRL:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                vRLPhilips = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case kDiffusionDirectionAP:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                vAPPhilips = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            case kDiffusionDirectionFH:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                vFHPhilips = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
                break;
            // case    kDiffusionDirectionRL:
            //     if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition)) {
            //         d.CSA.dtiV[1] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
            //         if ((d.CSA.numDti > 1) && (d.CSA.numDti < kMaxDTI4D))
            //             dti4D->S[d.CSA.numDti-1].V[1] = d.CSA.dtiV[1];
            //     }
            //     /*if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition) && (d.CSA.numDti > 0) && (d.CSA.numDti <= kMaxDTIv))
            //         d.CSA.dtiV[d.CSA.numDti-1][1] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);*/
            //     break;
            // case kDiffusionDirectionAP:
            //     if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition)) {
            //         d.CSA.dtiV[2] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
            //         if ((d.CSA.numDti > 1) && (d.CSA.numDti < kMaxDTI4D))
            //             dti4D->S[d.CSA.numDti-1].V[2] = d.CSA.dtiV[2];
            //     }
            //     /*if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition) && (d.CSA.numDti > 0) && (d.CSA.numDti <= kMaxDTIv))
            //         d.CSA.dtiV[d.CSA.numDti-1][2] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);*/
            //     break;
            // case kDiffusionDirectionFH:
            //     if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition)) {
            //         d.CSA.dtiV[3] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);
            //         if ((d.CSA.numDti > 1) && (d.CSA.numDti < kMaxDTI4D))
            //             dti4D->S[d.CSA.numDti-1].V[3] = d.CSA.dtiV[3];
            //         //printMessage("dti XYZ %g %g %g\n",d.CSA.dtiV[1],d.CSA.dtiV[2],d.CSA.dtiV[3]);
            //     }
            //     /*if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (isAtFirstPatientPosition) && (d.CSA.numDti > 0) && (d.CSA.numDti <= kMaxDTIv))
            //         d.CSA.dtiV[d.CSA.numDti-1][3] = dcmFloat(lLength, &buffer[lPos],d.isLittleEndian);*/
            //     //http://www.na-mic.org/Wiki/index.php/NAMIC_Wiki:DTI:DICOM_for_DWI_and_DTI
            //     break;
            //~~
            case kPrivatePerFrameSq :
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                //if ((vr[0] == 'S') && (vr[1] == 'Q')) break;
                //if (!is2005140FSQwarned)
                //  printWarning("expected VR of 2005,140F to be 'SQ' (prior DICOM->DICOM conversion error?)\n");
                is2005140FSQ = true;
                //is2005140FSQwarned = true;
            //case kMRImageGradientOrientationNumber :
            //    if (d.manufacturer == kMANUFACTURER_PHILIPS)
            //       MRImageGradientOrientationNumber =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kMRImageDiffBValueNumber:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                philMRImageDiffBValueNumber =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kWaveformSq:
                d.imageStart = 1; //abort!!!
                printMessage("Skipping DICOM (audio not image) \n");
                break;
            case kCSAImageHeaderInfo:
                readCSAImageHeader(&buffer[lPos], lLength, &d.CSA, isVerbose); //, dti4D);
                if (!d.isHasPhase)
                    d.isHasPhase = d.CSA.isPhaseMap;
                break;
                //case kObjectGraphics:
                //    printMessage("---->%d,",lLength);
                //    break;
            case kCSASeriesHeaderInfo:
                if ((lPos + lLength) > fileLen) break;
                d.CSA.SeriesHeader_offset = (int)lPos;
                d.CSA.SeriesHeader_length = lLength;
                break;
            case kRealWorldIntercept:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                d.RWVIntercept = dcmFloatDouble(lLength, &buffer[lPos],d.isLittleEndian);
                if (isSameFloat(0.0, d.intenIntercept)) //give precedence to standard value
                    d.intenIntercept = d.RWVIntercept;
                break;
            case kRealWorldSlope:
                if (d.manufacturer != kMANUFACTURER_PHILIPS) break;
                d.RWVScale = dcmFloatDouble(lLength, &buffer[lPos],d.isLittleEndian);
                //printMessage("RWVScale %g\n", d.RWVScale);
                if (isSameFloat(1.0, d.intenScale))  //give precedence to standard value
                    d.intenScale = d.RWVScale;
                break;
            case kUserDefineDataGE: { //0043,102A
                if ((d.manufacturer != kMANUFACTURER_GE) || (lLength < 128)) break;
                #define MY_DEBUG_GE // <- uncomment this to use following code to infer GE phase encoding direction
                #ifdef MY_DEBUG_GE
                int isVerboseX = isVerbose; //for debugging only - in standard release we will enable user defined "isVerbose"
                //int isVerboseX = isVerbose;
                if (isVerboseX > 1) printMessage(" UserDefineDataGE file offset/length %ld %u\n", lFileOffset+lPos, lLength);
                if (lLength < 916) { //minimum size is hdr_offset=0, read 0x0394
                    printMessage(" GE header too small to be valid  (A)\n");
                    break;
                }
                //debug code to export binary data
                /*
                char str[kDICOMStr];
                sprintf(str, "%s_ge.bin",fname);
                FILE *pFile = fopen(str, "wb");
                fwrite(&buffer[lPos], 1, lLength, pFile);
                fclose (pFile);
                */
                if ((size_t)(lPos + lLength) > MaxBufferSz) {
                    //we could re-read the buffer in this case, however in practice GE headers are concise so we never see this issue
                    printMessage(" GE header overflows buffer\n");
                    break;
                }
                uint16_t hdr_offset = dcmInt(2,&buffer[lPos+24],true);
                if (isVerboseX > 1) printMessage(" header offset: %d\n", hdr_offset);
                if (lLength < (hdr_offset+916)) { //minimum size is hdr_offset=0, read 0x0394
                    printMessage(" GE header too small to be valid  (B)\n");
                    break;
                }
                //size_t hdr = lPos+hdr_offset;
                float version = dcmFloat(4,&buffer[lPos + hdr_offset],true);
                if (isVerboseX > 1) printMessage(" version %g\n", version);
                if (version < 5.0 || version > 40.0) {
                    //printMessage(" GE header file format incorrect %g\n", version);
                    break;
                }
                //char const *hdr = &buffer[lPos + hdr_offset];
                char *hdr = (char *)&buffer[lPos + hdr_offset];
                int epi_chk_off = 0x003a;
                int pepolar_off   = 0x0030;
                int kydir_off   = 0x0394;
                if (version >= 25.002) {
                    hdr       += 0x004c;
                    kydir_off -= 0x008c;
                }
                //int seqOrInter =dcmInt(2,(unsigned char*)(hdr + pepolar_off-638),true);
                //int seqOrInter2 =dcmInt(2,(unsigned char*)(hdr + kydir_off-638),true);
                //printf("%d %d<<<\n", seqOrInter,seqOrInter2);
                //check if EPI
                if (true) {
                    //int check = *(short const *)(hdr + epi_chk_off) & 0x800;
                    int check =dcmInt(2,(unsigned char*)hdr + epi_chk_off,true) & 0x800;
                    if (check == 0) {
                        if (isVerboseX > 1) printMessage("Warning: Data is not EPI\n");
                        break;
                    }
                }
                //Check for PE polarity
                // int flag1 = *(short const *)(hdr + pepolar_off) & 0x0004;
                //Check for ky direction (view order)
                // int flag2 = *(int const *)(hdr + kydir_off);
                int phasePolarityFlag = dcmInt(2,(unsigned char*)hdr + pepolar_off,true) & 0x0004;
                //Check for ky direction (view order)
                int sliceOrderFlag = dcmInt(2,(unsigned char*)hdr + kydir_off,true);
                if (isVerboseX > 1)
                    printMessage(" GE phasePolarity/sliceOrder flags %d %d\n", phasePolarityFlag, sliceOrderFlag);
                if (phasePolarityFlag == kGE_PHASE_ENCODING_POLARITY_FLIPPED)
                    d.phaseEncodingGE = kGE_PHASE_ENCODING_POLARITY_FLIPPED;
                if (phasePolarityFlag == kGE_PHASE_ENCODING_POLARITY_UNFLIPPED)
                    d.phaseEncodingGE = kGE_PHASE_ENCODING_POLARITY_UNFLIPPED;
                if (sliceOrderFlag == kGE_SLICE_ORDER_BOTTOM_UP) {
                    //https://cfmriweb.ucsd.edu/Howto/3T/operatingtips.html
                    if (d.phaseEncodingGE == kGE_PHASE_ENCODING_POLARITY_UNFLIPPED)
                        d.phaseEncodingGE = kGE_PHASE_ENCODING_POLARITY_FLIPPED;
                    else
                        d.phaseEncodingGE = kGE_PHASE_ENCODING_POLARITY_UNFLIPPED;
                }
                //if (sliceOrderFlag == kGE_SLICE_ORDER_TOP_DOWN)
                //  d.sliceOrderGE = kGE_SLICE_ORDER_TOP_DOWN;
                //if (sliceOrderFlag == kGE_SLICE_ORDER_BOTTOM_UP)
                //  d.sliceOrderGE = kGE_SLICE_ORDER_BOTTOM_UP;
                #endif
                break;
            }
            case kEffectiveEchoSpacingGE:
                if (d.manufacturer == kMANUFACTURER_GE) d.effectiveEchoSpacingGE = dcmInt(lLength,&buffer[lPos],d.isLittleEndian);
                break;
            case kDiffusionBFactorGE :
                if (d.manufacturer == kMANUFACTURER_GE)
                  set_bValGE(&volDiffusion, lLength, &buffer[lPos]);
                break;
            case kGeiisFlag:
                if ((lLength > 4) && (buffer[lPos]=='G') && (buffer[lPos+1]=='E') && (buffer[lPos+2]=='I')  && (buffer[lPos+3]=='I')) {
                    //read a few digits, as bug is specific to GEIIS, while GEMS are fine
                    printWarning("GEIIS violates the DICOM standard. Inspect results and admonish your vendor.\n");
                    isIconImageSequence = true;
                    //geiisBug = true; //compressed thumbnails do not follow transfer syntax! GE should not re-use pulbic tags for these proprietary images http://sonca.kasshin.net/gdcm/Doc/GE_ImageThumbnails
                }
                break;
            case kProcedureStepDescription:
                dcmStr (lLength, &buffer[lPos], d.procedureStepDescription);
                break;
            case kOrientationACR : //use in emergency if kOrientation is not present!
                if (!isOrient) dcmMultiFloat(lLength, (char*)&buffer[lPos], 6, d.orient);
                break;
            //case kTemporalPositionIdentifier :
            //  temporalPositionIdentifier =  dcmStrInt(lLength, &buffer[lPos]);
            //    break;
            case kOrientation : {
                if (isOrient) { //already read orient - read for this slice to see if it varies (localizer)
                    float orient[7];
                    dcmMultiFloat(lLength, (char*)&buffer[lPos], 6, orient);
                    if ((!isSameFloatGE(d.orient[1], orient[1]) || !isSameFloatGE(d.orient[2], orient[2]) ||  !isSameFloatGE(d.orient[3], orient[3]) ||
                            !isSameFloatGE(d.orient[4], orient[4]) || !isSameFloatGE(d.orient[5], orient[5]) ||  !isSameFloatGE(d.orient[6], orient[6]) ) ) {
                        if (!d.isLocalizer)
                            printMessage("slice orientation varies (localizer?) [%g %g %g %g %g %g] != [%g %g %g %g %g %g]\n",
                            d.orient[1], d.orient[2], d.orient[3],d.orient[4], d.orient[5], d.orient[6],
                            orient[1], orient[2], orient[3],orient[4], orient[5], orient[6]);
                        d.isLocalizer = true;
                    }
                }
                dcmMultiFloat(lLength, (char*)&buffer[lPos], 6, d.orient);
                isOrient = true;
                break; }
            case kImagesInAcquisition :
                imagesInAcquisition =  dcmStrInt(lLength, &buffer[lPos]);
                break;
            case kImageStart:
                //if ((!geiisBug) && (!isIconImageSequence)) //do not exit for proprietary thumbnails
                if (isIconImageSequence) {
                    int imgBytes = (d.xyzDim[1] * d.xyzDim[2] * int(d.bitsAllocated / 8));
                    if (imgBytes == lLength)
                        isIconImageSequence = false;
                    if (sqDepth < 1) printWarning("Assuming 7FE0,0010 refers to an icon not the main image\n");

                }
                if ((d.compressionScheme == kCompressNone ) && (!isIconImageSequence)) //do not exit for proprietary thumbnails
                    d.imageStart = (int)lPos + (int)lFileOffset;
                //geiisBug = false;
                //http://www.dclunie.com/medical-image-faq/html/part6.html
                //unlike raw data, Encapsulated data is stored as Fragments contained in Items that are the Value field of Pixel Data
                if ((d.compressionScheme != kCompressNone) && (!isIconImageSequence)) {
                    lLength = 0;
                    isEncapsulatedData = true;
                    encapsulatedDataImageStart = (int)lPos + (int)lFileOffset;
                }
                isIconImageSequence = false;
                break;
            case kImageStartFloat:
                d.isFloat = true;
                if (!isIconImageSequence) //do not exit for proprietary thumbnails
                    d.imageStart = (int)lPos + (int)lFileOffset;
                isIconImageSequence = false;
                break;
            case kImageStartDouble:
                printWarning("Double-precision DICOM conversion untested: please provide samples to developer\n");
                d.isFloat = true;
                if (!isIconImageSequence) //do not exit for proprietary thumbnails
                    d.imageStart = (int)lPos + (int)lFileOffset;
                isIconImageSequence = false;
                break;
        } //switch/case for groupElement

        if (isVerbose > 1) {
            //dcm2niix i fast because it does not use a dictionary.
            // this is a very incomplete DICOM header report, and not a substitute for tools like dcmdump
            // the purpose is to see how dcm2niix has parsed the image for diagnostics
            // this section will report very little for implicit data
            char str[kDICOMStr];
            sprintf(str, "%*c%04x,%04x %u@%ld ", sqDepth+1, ' ',  groupElement & 65535,groupElement>>16, lLength, lFileOffset+lPos);
            bool isStr = false;
            if (d.isExplicitVR) {
                sprintf(str, "%s%c%c ", str, vr[0], vr[1]);
                if ((vr[0]=='F') && (vr[1]=='D')) sprintf(str, "%s%g ", str, dcmFloatDouble(lLength, &buffer[lPos], d.isLittleEndian));
                if ((vr[0]=='F') && (vr[1]=='L')) sprintf(str, "%s%g ", str, dcmFloat(lLength, &buffer[lPos], d.isLittleEndian));
                if ((vr[0]=='S') && (vr[1]=='S')) sprintf(str, "%s%d ", str, dcmInt(lLength, &buffer[lPos], d.isLittleEndian));
                if ((vr[0]=='S') && (vr[1]=='L')) sprintf(str, "%s%d ", str, dcmInt(lLength,&buffer[lPos],d.isLittleEndian));
                if ((vr[0]=='U') && (vr[1]=='S')) sprintf(str, "%s%d ", str, dcmInt(lLength, &buffer[lPos], d.isLittleEndian));
                if ((vr[0]=='U') && (vr[1]=='L')) sprintf(str, "%s%d ", str, dcmInt(lLength, &buffer[lPos], d.isLittleEndian));
                if ((vr[0]=='A') && (vr[1]=='E')) isStr = true;
                if ((vr[0]=='A') && (vr[1]=='S')) isStr = true;
                //if ((vr[0]=='A') && (vr[1]=='T')) isStr = xxx;
                if ((vr[0]=='C') && (vr[1]=='S')) isStr = true;
                if ((vr[0]=='D') && (vr[1]=='A')) isStr = true;
                if ((vr[0]=='D') && (vr[1]=='S')) isStr = true;
                if ((vr[0]=='D') && (vr[1]=='T')) isStr = true;
                if ((vr[0]=='I') && (vr[1]=='S')) isStr = true;
                if ((vr[0]=='L') && (vr[1]=='O')) isStr = true;
                if ((vr[0]=='L') && (vr[1]=='T')) isStr = true;
                //if ((vr[0]=='O') && (vr[1]=='B')) isStr = xxx;
                //if ((vr[0]=='O') && (vr[1]=='D')) isStr = xxx;
                //if ((vr[0]=='O') && (vr[1]=='F')) isStr = xxx;
                //if ((vr[0]=='O') && (vr[1]=='W')) isStr = xxx;
                if ((vr[0]=='P') && (vr[1]=='N')) isStr = true;
                if ((vr[0]=='S') && (vr[1]=='H')) isStr = true;
                if ((vr[0]=='S') && (vr[1]=='T')) isStr = true;
                if ((vr[0]=='T') && (vr[1]=='M')) isStr = true;
                if ((vr[0]=='U') && (vr[1]=='I')) isStr = true;
                if ((vr[0]=='U') && (vr[1]=='T')) isStr = true;
            } else
                isStr = (lLength > 12); //implicit encoding: not always true as binary vectors may exceed 12 bytes, but often true
            if (lLength > 128) {
                sprintf(str, "%s<%d bytes> ", str, lLength);
                printMessage("%s\n", str);
            } else if (isStr) { //if length is greater than 8 bytes (+4 hdr) the MIGHT be a string
                char tagStr[kDICOMStr];
                tagStr[0] = 'X'; //avoid compiler warning: orientStr filled by dcmStr
                dcmStr (lLength, &buffer[lPos], tagStr);
                if (strlen(tagStr) > 1) {
                    for (size_t pos = 0; pos<strlen(tagStr); pos ++)
                        if ((tagStr[pos] == '<') || (tagStr[pos] == '>') || (tagStr[pos] == ':')
                            || (tagStr[pos] == '"') || (tagStr[pos] == '\\') || (tagStr[pos] == '/')
                            || (tagStr[pos] == '^') || (tagStr[pos] < 33)
                            || (tagStr[pos] == '*') || (tagStr[pos] == '|') || (tagStr[pos] == '?'))
                                tagStr[pos] = 'x';
                }
                printMessage("%s %s\n", str, tagStr);
            } else
                printMessage("%s\n", str);
            //if (d.isExplicitVR) printMessage(" VR=%c%c\n", vr[0], vr[1]);
        }   //printMessage(" tag=%04x,%04x length=%u pos=%ld %c%c nest=%d\n",   groupElement & 65535,groupElement>>16, lLength, lPos,vr[0], vr[1], nest);
        lPos = lPos + (lLength);
        //printMessage("%d\n",d.imageStart);
    } //while d.imageStart == 0
    free (buffer);
    if (encapsulatedDataFragmentStart > 0) {
        if (encapsulatedDataFragments > 1)
            printError("Compressed image stored as %d fragments: decompress with gdcmconv, Osirix, dcmdjpeg or dcmjp2k \n", encapsulatedDataFragments);
        else
            d.imageStart = encapsulatedDataFragmentStart;
    } else if ((isEncapsulatedData) && (d.imageStart < 128)) {
        //http://www.dclunie.com/medical-image-faq/html/part6.html
        //Uncompressed data (unencapsulated) is sent in DICOM as a series of raw bytes or words (little or big endian) in the Value field of the Pixel Data element (7FE0,0010). Encapsulated data on the other hand is sent not as raw bytes or words but as Fragments contained in Items that are the Value field of Pixel Data
        printWarning("DICOM violation (contact vendor): compressed image without image fragments, assuming image offset defined by 0x7FE0,x0010: \n");
        d.imageStart = encapsulatedDataImageStart;
    }
    if ((d.modality == kMODALITY_PT) && (PETImageIndex > 0)) {
        d.imageNum = PETImageIndex; //https://github.com/rordenlab/dcm2niix/issues/184
        //printWarning("PET scan using 0054,1330 for image number %d\n", PETImageIndex);
    }
    //Recent Philips images include DateTime (0008,002A) but not separate date and time (0008,0022 and 0008,0032)
    #define kYYYYMMDDlen 8 //how many characters to encode year,month,day in "YYYYDDMM" format
    if ((strlen(acquisitionDateTimeTxt) > (kYYYYMMDDlen+5)) && (!isFloatDiff(d.acquisitionTime, 0.0f)) && (!isFloatDiff(d.acquisitionDate, 0.0f)) ) {
        // 20161117131643.80000 -> date 20161117 time 131643.80000
        //printMessage("acquisitionDateTime %s\n",acquisitionDateTimeTxt);
        char acquisitionDateTxt[kDICOMStr];
        strncpy(acquisitionDateTxt, acquisitionDateTimeTxt, kYYYYMMDDlen);
        acquisitionDateTxt[kYYYYMMDDlen] = '\0'; // IMPORTANT!
        d.acquisitionDate = atof(acquisitionDateTxt);
        char acquisitionTimeTxt[kDICOMStr];
        int timeLen = (int)strlen(acquisitionDateTimeTxt) - kYYYYMMDDlen;
        strncpy(acquisitionTimeTxt, &acquisitionDateTimeTxt[kYYYYMMDDlen], timeLen);
        acquisitionTimeTxt[timeLen] = '\0'; // IMPORTANT!
        d.acquisitionTime = atof(acquisitionTimeTxt);
    }
    d.dateTime = (atof(d.studyDate)* 1000000) + atof(d.studyTime);
    //printMessage("slices in Acq %d %d\n",d.locationsInAcquisition,locationsInAcquisitionPhilips);
    if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (d.locationsInAcquisition == 0))
        d.locationsInAcquisition = locationsInAcquisitionPhilips;
    if ((d.manufacturer == kMANUFACTURER_GE) && (imagesInAcquisition > 0))
        d.locationsInAcquisition = imagesInAcquisition; //e.g. if 72 slices acquired but interpolated as 144
    if ((d.manufacturer == kMANUFACTURER_GE) && (d.locationsInAcquisition > 0)  &&  (locationsInAcquisitionGE > 0) && (d.locationsInAcquisition != locationsInAcquisitionGE) ) {
        //printMessage("Check number of slices, discrepancy between tags (0054,0081; 0020,1002; 0021,104F)\n");
        d.locationsInAcquisition = locationsInAcquisitionGE;
    }
    if ((d.manufacturer == kMANUFACTURER_GE) && (d.locationsInAcquisition == 0))
        d.locationsInAcquisition = locationsInAcquisitionGE;
    if (d.zSpacing > 0)
        d.xyzMM[3] = d.zSpacing; //use zSpacing if provided: depending on vendor, kZThick may or may not include a slice gap
    //printMessage("patientPositions = %d XYZT = %d slicePerVol = %d numberOfDynamicScans %d\n",patientPositionNum,d.xyzDim[3], d.locationsInAcquisition, d.numberOfDynamicScans);
    if ((d.manufacturer == kMANUFACTURER_PHILIPS) && (patientPositionNum > d.xyzDim[3]))
        printMessage("Please check slice thicknesses: Philips R3.2.2 bug can disrupt estimation (%d positions reported for %d slices)\n",patientPositionNum, d.xyzDim[3]); //Philips reported different positions for each slice!
    if ((d.imageStart > 144) && (d.xyzDim[1] > 1) && (d.xyzDim[2] > 1))
        d.isValid = true;
    if ((d.xyzMM[1] > FLT_EPSILON) && (d.xyzMM[2] < FLT_EPSILON)) {
        printMessage("Please check voxel size\n");
        d.xyzMM[2] = d.xyzMM[1];
    }
    if ((d.xyzMM[2] > FLT_EPSILON) && (d.xyzMM[1] < FLT_EPSILON)) {
        printMessage("Please check voxel size\n");
        d.xyzMM[1] = d.xyzMM[2];
    }
    if ((d.xyzMM[3] < FLT_EPSILON)) {
        printMessage("Unable to determine slice thickness: please check voxel size\n");
        d.xyzMM[3] = 1.0;
    }
    //printMessage("Patient Position\t%g\t%g\t%g\tThick\t%g\n",d.patientPosition[1],d.patientPosition[2],d.patientPosition[3], d.xyzMM[3]);
    //printMessage("Patient Position\t%g\t%g\t%g\tThick\t%g\tStart\t%d\n",d.patientPosition[1],d.patientPosition[2],d.patientPosition[3], d.xyzMM[3], d.imageStart);
    // printMessage("ser %ld\n", d.seriesNum);
    //int kEchoMult = 100; //For Siemens/GE Series 1,2,3... save 2nd echo as 201, 3rd as 301, etc
    //if (d.seriesNum > 100)
    //    kEchoMult = 10; //For Philips data Saved as Series 101,201,301... save 2nd echo as 111, 3rd as 121, etc
    //if (coilNum > 0) //segment images with multiple coils
    //    d.seriesNum = d.seriesNum + (100*coilNum);
    //if (d.echoNum > 1) //segment images with multiple echoes
    //    d.seriesNum = d.seriesNum + (kEchoMult*d.echoNum);
    if ((d.compressionScheme == kCompress50) && (d.bitsAllocated > 8) ) {
        //dcmcjpg with +ee can create .51 syntax images that are 8,12,16,24-bit: we can only decode 8/24-bit
        printError("Unable to decode %d-bit images with Transfer Syntax 1.2.840.10008.1.2.4.51, decompress with dcmdjpg or gdcmconv\n", d.bitsAllocated);
        d.isValid = false;
    }
    if ((numberOfImagesInMosaic > 1) && (d.CSA.mosaicSlices < 1))
        d.CSA.mosaicSlices = numberOfImagesInMosaic;
    if ((d.manufacturer == kMANUFACTURER_SIEMENS) && (isMosaic) && (d.CSA.mosaicSlices < 1) && (d.phaseEncodingSteps > 0) && ((d.xyzDim[1] % d.phaseEncodingSteps) == 0) && ((d.xyzDim[2] % d.phaseEncodingSteps) == 0) ) {
        d.CSA.mosaicSlices = (d.xyzDim[1] / d.phaseEncodingSteps) * (d.xyzDim[2] / d.phaseEncodingSteps);
        printWarning("Mosaic inferred without CSA header (check number of slices and spatial orientation)\n");
    }
    if ((d.manufacturer == kMANUFACTURER_SIEMENS) && (d.CSA.dtiV[1] < -1.0) && (d.CSA.dtiV[2] < -1.0) && (d.CSA.dtiV[3] < -1.0))
        d.CSA.dtiV[0] = 0; //SiemensTrio-Syngo2004A reports B=0 images as having impossible b-vectors.
    if ((d.manufacturer == kMANUFACTURER_GE) && (strlen(d.seriesDescription) > 1)) //GE uses a generic session name here: do not overwrite kProtocolNameGE
        strcpy(d.protocolName, d.seriesDescription);
    if ((strlen(d.protocolName) < 1) && (strlen(d.seriesDescription) > 1))
        strcpy(d.protocolName, d.seriesDescription);
    if ((strlen(d.protocolName) < 1) && (strlen(d.sequenceName) > 1) && (d.manufacturer != kMANUFACTURER_SIEMENS))
        strcpy(d.protocolName, d.sequenceName); //protocolName (0018,1030) optional, sequence name (0018,0024) is not a good substitute for Siemens as it can vary per volume: *ep_b0 *ep_b1000#1, *ep_b1000#2, etc https://www.nitrc.org/forum/forum.php?thread_id=8771&forum_id=4703
    //     if (!isOrient) {
    //      if (d.isNonImage)
    //          printWarning("Spatial orientation ambiguous  (tag 0020,0037 not found) [probably not important: derived image]: %s\n", fname);
    //      else if (((d.manufacturer == kMANUFACTURER_SIEMENS)) && (d.samplesPerPixel != 1))
    //          printWarning("Spatial orientation ambiguous (tag 0020,0037 not found) [perhaps derived FA that is not required]: %s\n", fname);
    //      else
    //          printWarning("Spatial orientation ambiguous (tag 0020,0037 not found): %s\n", fname);
    //     }
/*if (d.isHasMagnitude)
    printError("=====> mag %d %d\n", d.patientPositionRepeats, d.patientPositionSequentialRepeats );
if (d.isHasPhase)
    printError("=====> phase %d %d\n", d.patientPositionRepeats, d.patientPositionSequentialRepeats );

    printError("=====> reps %d %d\n", d.patientPositionRepeats, d.patientPositionSequentialRepeats );
*/
    /*if ((patientPositionSequentialRepeats > 1) && ( (d.xyzDim[3] % patientPositionSequentialRepeats) == 0 )) {
        //will require Converting XYTZ to XYZT
        //~ d.numberOfDynamicScans = d.xyzDim[3] / d.patientPositionSequentialRepeats;
        //~ d.xyzDim[4] = d.xyzDim[3] / d.numberOfDynamicScans;
        numberOfDynamicScans = d.xyzDim[3] / patientPositionSequentialRepeats;
        d.xyzDim[4] = d.xyzDim[3] / numberOfDynamicScans;

        d.xyzDim[3] = d.numberOfDynamicScans;
    }*/
    if (numberOfFrames == 0) numberOfFrames = d.xyzDim[3];
    if ((locationsInAcquisitionPhilips > 0) && ((d.xyzDim[3] % locationsInAcquisitionPhilips) == 0)) {
        d.xyzDim[4] = d.xyzDim[3] / locationsInAcquisitionPhilips;
        d.xyzDim[3] = locationsInAcquisitionPhilips;
    } else if ((numberOfDynamicScans > 1) && (d.xyzDim[4] < 2) && (d.xyzDim[3] > 1) && ((d.xyzDim[3] % numberOfDynamicScans) == 0)) {
        d.xyzDim[3] = d.xyzDim[3] / numberOfDynamicScans;
        d.xyzDim[4] = numberOfDynamicScans;
    }
    if ((maxInStackPositionNumber > 1) && (d.xyzDim[4] < 2) && (d.xyzDim[3] > 1) && ((d.xyzDim[3] % maxInStackPositionNumber) == 0)) {
        d.xyzDim[4] = d.xyzDim[3] / maxInStackPositionNumber;
        d.xyzDim[3] = maxInStackPositionNumber;
    }
    if ((!isnan(patientPositionStartPhilips[1])) && (!isnan(patientPositionEndPhilips[1]))) {
            for (int k = 0; k < 4; k++) {
                d.patientPosition[k] = patientPositionStartPhilips[k];
                d.patientPositionLast[k] = patientPositionEndPhilips[k];
            }
    }
    if (!isnan(patientPositionStartPhilips[1])) //for Philips data without
        for (int k = 0; k < 4; k++)
            d.patientPosition[k] = patientPositionStartPhilips[k];
    if (isVerbose) {
        printMessage("DICOM file :\n");
        printMessage(" patient position (0020,0032)\t%g\t%g\t%g\n", d.patientPosition[1],d.patientPosition[2],d.patientPosition[3]);
        if (!isnan(patientPositionEndPhilips[1]))
            printMessage(" patient position end (0020,0032)\t%g\t%g\t%g\n", patientPositionEndPhilips[1],patientPositionEndPhilips[2],patientPositionEndPhilips[3]);
        printMessage(" orient (0020,0037)\t%g\t%g\t%g\t%g\t%g\t%g\n", d.orient[1],d.orient[2],d.orient[3], d.orient[4],d.orient[5],d.orient[6]);
        //printMessage(" acq %d img %d ser %ld dim %dx%dx%dx%d mm %gx%gx%g offset %d loc %d valid %d ph %d mag %d nDTI %d 3d %d bits %d littleEndian %d echo %d coil %d TE %g TR %g\n",d.acquNum,d.imageNum,d.seriesNum,d.xyzDim[1],d.xyzDim[2],d.xyzDim[3], d.xyzDim[4],d.xyzMM[1],d.xyzMM[2],d.xyzMM[3],d.imageStart, d.locationsInAcquisition, d.isValid, d.isHasPhase, d.isHasMagnitude, d.CSA.numDti, d.is3DAcq, d.bitsAllocated, d.isLittleEndian, d.echoNum, d.coilNum, d.TE, d.TR);
        //if (d.CSA.dtiV[0] > 0)
        //  printMessage(" DWI bxyz %g %g %g %g\n", d.CSA.dtiV[0], d.CSA.dtiV[1], d.CSA.dtiV[2], d.CSA.dtiV[3]);
    }
    if ((numDimensionIndexValues > 1) && (numDimensionIndexValues == numberOfFrames)) {
        //Philips enhanced datasets can have custom slice orders and pack images with different TE, Phase/Magnitude/Etc.
        if (isVerbose > 1) { //
            int mn[MAX_NUMBER_OF_DIMENSIONS];
            int mx[MAX_NUMBER_OF_DIMENSIONS];
            for (int j = 0; j < MAX_NUMBER_OF_DIMENSIONS; j++) {
                mx[j] = dcmDim[0].dimIdx[j];
                mn[j] = mx[j];
                for (int i = 0; i < numDimensionIndexValues; i++) {
                    if (mx[j] < dcmDim[i].dimIdx[j]) mx[j] = dcmDim[i].dimIdx[j];
                    if (mn[j] > dcmDim[i].dimIdx[j]) mn[j] = dcmDim[i].dimIdx[j];
                }
            }
            printMessage(" DimensionIndexValues (0020,9157), dimensions with variability:\n");
            for (int i = 0; i < MAX_NUMBER_OF_DIMENSIONS; i++)
                if (mn[i] != mx[i])
                    printMessage(" Dimension %d Range: %d..%d\n", i, mn[i], mx[i]);
        } //verbose > 1
        //sort dimensions
#ifdef USING_R
        std::sort(dcmDim.begin(), dcmDim.begin() + numberOfFrames, compareTDCMdim);
#else
        qsort(dcmDim, numberOfFrames, sizeof(struct TDCMdim), compareTDCMdim);
#endif
        //for (int i = 0; i < numberOfFrames; i++)
        //  printf("%d -> %d  %d %d %d\n", i,  dcmDim[i].diskPos, dcmDim[i].dimIdx[1], dcmDim[i].dimIdx[2], dcmDim[i].dimIdx[3]);
        for (int i = 0; i < numberOfFrames; i++)
            dti4D->sliceOrder[i] = dcmDim[i].diskPos;
        if ((d.xyzDim[4] > 1) && (d.xyzDim[4] < kMaxDTI4D)) { //record variations in TE
            d.isScaleOrTEVaries = false;
            bool isTEvaries = false;
            bool isScaleVaries = false;
            //setting j = 1 in next few lines is a hack, just in case TE/scale/intercept listed AFTER dimensionIndexValues
            int j = 0;
            if (d.xyzDim[3] > 1) j = 1;
            for (int i = 0; i < d.xyzDim[4]; i++) {
                //dti4D->gradDynVol[i] = 0; //only PAR/REC
                dti4D->TE[i] =  dcmDim[j+(i * d.xyzDim[3])].TE;
                dti4D->intenScale[i] =  dcmDim[j+(i * d.xyzDim[3])].intenScale;
                dti4D->intenIntercept[i] =  dcmDim[j+(i * d.xyzDim[3])].intenIntercept;
                dti4D->isPhase[i] =  dcmDim[j+(i * d.xyzDim[3])].isPhase;
                dti4D->isReal[i] =  dcmDim[j+(i * d.xyzDim[3])].isReal;
                dti4D->isImaginary[i] =  dcmDim[j+(i * d.xyzDim[3])].isImaginary;
                dti4D->intenScalePhilips[i] =  dcmDim[j+(i * d.xyzDim[3])].intenScalePhilips;
                dti4D->RWVIntercept[i] =  dcmDim[j+(i * d.xyzDim[3])].RWVIntercept;
                dti4D->RWVScale[i] =  dcmDim[j+(i * d.xyzDim[3])].RWVScale;
                dti4D->triggerDelayTime[i] =  dcmDim[j+(i * d.xyzDim[3])].triggerDelayTime;
                dti4D->S[i].V[0] = dcmDim[j+(i * d.xyzDim[3])].V[0];
                dti4D->S[i].V[1] = dcmDim[j+(i * d.xyzDim[3])].V[1];
                dti4D->S[i].V[2] = dcmDim[j+(i * d.xyzDim[3])].V[2];
                dti4D->S[i].V[3] = dcmDim[j+(i * d.xyzDim[3])].V[3];
                if (dti4D->TE[i] != d.TE) isTEvaries = true;
                if (dti4D->intenScale[i] != d.intenScale) isScaleVaries = true;
                if (dti4D->intenIntercept[i] != d.intenIntercept) isScaleVaries = true;
                if (dti4D->isPhase[i] != isPhase) d.isScaleOrTEVaries = true;
                if (dti4D->triggerDelayTime[i] != d.triggerDelayTime) d.isScaleOrTEVaries = true;
                if (dti4D->isReal[i] != isReal) d.isScaleOrTEVaries = true;
                if (dti4D->isImaginary[i] != isImaginary) d.isScaleOrTEVaries = true;
            }
            if((isScaleVaries) || (isTEvaries)) d.isScaleOrTEVaries = true;
            if (isTEvaries) d.isMultiEcho = true;
            //if echoVaries,count number of echoes
            /*int echoNum = 1;
            for (int i = 1; i < d.xyzDim[4]; i++) {
                if (dti4D->TE[i-1] != dti4D->TE[i])
            }*/
            if ((isVerbose) && (d.isScaleOrTEVaries)) {
                printMessage("Parameters vary across 3D volumes packed in single DICOM file:\n");
                for (int i = 0; i < d.xyzDim[4]; i++)
                    printMessage(" %d TE=%g Slope=%g Inter=%g PhilipsScale=%g Phase=%d\n", i, dti4D->TE[i], dti4D->intenScale[i], dti4D->intenIntercept[i], dti4D->intenScalePhilips[i], dti4D->isPhase[i] );
            }
        }
    }
    /* //Attempt to append ADC
    printMessage("CXC grad %g %d %d\n", philDTI[0].V[0], maxGradNum, d.xyzDim[4]);
    if ((maxGradNum > 1) && ((maxGradNum+1) == d.xyzDim[4]) ) {
        //ADC map (non-zero b-value with zero vector length)
        if (isVerbose)
            printMessage("Final volume does not have an associated 0020,9157. Assuming final volume is an ADC/isotropic map\n", philDTI[0].V[0], maxGradNum, d.xyzDim[4]);
        philDTI[maxGradNum].V[0] = 1000.0;
        philDTI[maxGradNum].V[1] = 0.0;
        philDTI[maxGradNum].V[2] = 0.0;
        philDTI[maxGradNum].V[3] = 0.0;
        maxGradNum++;
    }*/
    /*if  ((minGradNum >= 1) && ((maxGradNum-minGradNum+1) == d.xyzDim[4])) {
        //see ADNI DWI data for 018_S_4868 - the gradient numbers are in the range 2..37 for 36 volumes - no gradient number 1!
        if (philDTI[minGradNum -1].V[0] >= 0) {
            if (isVerbose)
                printMessage("Using %d diffusion data directions coded by DimensionIndexValues\n", maxGradNum);
            int off = 0;
            if (minGradNum > 1) {
                off = minGradNum - 1;
                printWarning("DimensionIndexValues (0020,9157) is not indexed from 1 (range %d..%d). Please validate results\n", minGradNum, maxGradNum);
            }
            for (int i = 0; i < d.xyzDim[4]; i++) {
                dti4D->S[i].V[0] = philDTI[i+off].V[0];
                dti4D->S[i].V[1] = philDTI[i+off].V[1];
                dti4D->S[i].V[2] = philDTI[i+off].V[2];
                dti4D->S[i].V[3] = philDTI[i+off].V[3];
                if (isVerbose > 1)
                    printMessage(" grad %d b=%g vec=%gx%gx%g\n", i, dti4D->S[i].V[0], dti4D->S[i].V[1], dti4D->S[i].V[2], dti4D->S[i].V[3]);
            }
            d.CSA.numDti = maxGradNum - off;
        }
    }*/
    if (d.CSA.numDti >= kMaxDTI4D) {
        printError("Unable to convert DTI [recompile with increased kMaxDTI4D] detected=%d, max = %d\n", d.CSA.numDti, kMaxDTI4D);
        d.CSA.numDti = 0;
    }
    if ((d.isValid) && (d.imageNum == 0)) { //Philips non-image DICOMs (PS_*, XX_*) are not valid images and do not include instance numbers
        printError("Instance number (0020,0013) not found: \n");
        d.imageNum = 1; //not set
    }
    if ((numDimensionIndexValues < 1) && (d.manufacturer == kMANUFACTURER_PHILIPS) && (d.seriesNum > 99999) && (philMRImageDiffBValueNumber > 0)) {
        //Ugly kludge to distinguish Philips classic DICOM dti
        // images from a single sequence can have identical series number, instance number, gradient number
        // the ONLY way to distinguish slices is using the private tag MRImageDiffBValueNumber
        // confusingly, the same sequence can also generate MULTIPLE series numbers!
        // for examples see https://www.nitrc.org/plugins/mwiki/index.php/dcm2nii:MainPage#Diffusion_Tensor_Imaging
        d.seriesNum += (philMRImageDiffBValueNumber*1000);
    }
    //if (contentTime != 0.0) && (numDimensionIndexValues < (MAX_NUMBER_OF_DIMENSIONS - 1)){
    //  long timeCRC = abs( (long)mz_crc32((unsigned char*) &contentTime, sizeof(double)));
    //}
    if (numDimensionIndexValues < MAX_NUMBER_OF_DIMENSIONS) //https://github.com/rordenlab/dcm2niix/issues/221
        d.dimensionIndexValues[MAX_NUMBER_OF_DIMENSIONS-1] = abs( (long)mz_crc32((unsigned char*) &d.seriesInstanceUID, strlen(d.seriesInstanceUID)));
    if (d.seriesNum < 1) //https://github.com/rordenlab/dcm2niix/issues/218
        d.seriesNum = abs( (long)mz_crc32((unsigned char*) &d.seriesInstanceUID, strlen(d.seriesInstanceUID)));
    //getFileName(d.imageBaseName, fname);
    if (multiBandFactor > d.CSA.multiBandFactor)
        d.CSA.multiBandFactor = multiBandFactor; //SMS reported in 0051,1011 but not CSA header
    //printf("%g\t\t%g\t%g\t%g\n", d.CSA.dtiV[0], d.CSA.dtiV[1], d.CSA.dtiV[2], d.CSA.dtiV[3]);
    //printMessage("buffer usage %d  %d  %d\n",d.imageStart, lPos+lFileOffset, MaxBufferSz);
    return d;
} // readDICOM()


