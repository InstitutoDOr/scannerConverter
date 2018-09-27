#!/bin/sh

#  compile.sh
#  
#
#  Created by Rede Dor on 05/09/18.
#  

g++ -w -O3 -DHAVE_ARPA_INET_H -DUSE_JPEGLS=ON -DmyDisableOpenJPEG \
     main.cpp sftp.cpp \
     memoryDCM.cpp \
     ../dcm2niix/console/ujpeg.cpp \
     ../dcm2niix/console/nii_dicom.cpp \
     ../dcm2niix/console/nii_dicom_batch.cpp \
     ../dcm2niix/console/nii_ortho.cpp \
     ../dcm2niix/console/nii_foreign.cpp \
     ../dcm2niix/console/nifti1_io_core.cpp \
     ../dcm2niix/console/jpg_0XC3.cpp \
     -lssh2 -lssl -lz -lcrypto -o dicomFTP \
     -I../dcm2niix/console \

