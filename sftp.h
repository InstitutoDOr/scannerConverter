/*
 * Sample showing how to do SFTP transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password /tmp/secrets -p|-i|-k"
 */

#include <stdlib.h>
#include "libssh2_config.h"
#include "libssh2.h"
#include "libssh2_sftp.h"

#ifdef HAVE_WINSOCK2_H
# include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
# ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <time.h>
#include "memoryDCM.hpp"

using namespace std;

#define SSHbufferSize 8*1024
#define timeBetweenChecks 1
#define maximumTries 2
#define noDCM 0
#define testMode 1

class fileObject
{
public:
    string filename;
    string fileTemplate;
    int thousands, units, fileIndex;
    double time;
   
    int isDicomFile()
    {
        if ((filename == ".") || (filename == ".."))
           return 0;
        string::size_type idx;
        idx = filename.rfind('.');
        if (idx != std::string::npos)
        {
           if (noDCM) return 1;
           else
           {
              string extension = filename.substr(idx+1);
              transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
              return (extension == "DCM");
           }
        }
        return 0;
    }

    void returnFileNameParts(char *fileName)
    {
       char file[1024];
       strcpy(file, fileName);
       
       int point = 0;
       if (!noDCM) // removing the dcm extension
       {
          for (int c=strlen(file)-1;c >-1; c--)
          {
             if (file[c] == '.')
             {
                file[c] = 0;
                break;
             }   
          }
       }
       // find the point position
       for (int c=strlen(file)-1;c >-1; c--)
       {
          if (file[c] == '.')
          {
             point = c;
             break;
          }   
       }

       char number[10], number2[10];
       // cleaning the variables
       for (int i=0; i<10; i++) 
       {
          number[i] = 0; number2[i] = 0;
       } 

       // transforming the last digits in numbers
       for (int i=0; i<3; i++) number[i] = file[point-3+i];
       for (int i=point+1; i<strlen(file); i++) number2[i-point-1] = file[i];
       
       file[point-3] = 0;
       fileTemplate = file;
       thousands = atoi(number);
       units = atoi(number2);
    }

    fileObject(char *fileName)
    {
       filename = fileName;
       returnFileNameParts(fileName);
       fileIndex = thousands * 1000 + units;
    }   
};

typedef struct
{
   bool operator()(fileObject const &a, fileObject const &b) const { return a.fileIndex < b.fileIndex; }
} fileSort;

class sFTPGE;
class dicomConverter
{
   string fileTemplate;
   int firstSliceIndex;
   int *sliceReady;
   int volumeIndex;

   struct nifti_1_header hdr;
   unsigned char * imgM, *img;
   size_t imgsz;
   int nslices;
   struct TDCMopts opts;

public:   
   string fileIndex(int index);
   void prepareVariables();
   int checkVolumeStatus();
   int convert2Nii(string outputdir);
   void reset(); 
   void resetFlag();
   void setFirstSliceName(char *filename);
   struct TDICOMdata lastRead;

   sFTPGE *ge;

   dicomConverter()
    {
        nslices=0;
        imgsz=0;
        firstSliceIndex=0;
        imgM = NULL;
        img=NULL;
        volumeIndex = 1;
        sliceReady = NULL;
        ge = NULL;

        opts.isCreateBIDS = false;
        opts.isForceStackSameSeries = false;
        opts.isOnlySingleFile = true;
        opts.isCreateText = false;
        opts.isVerbose = 0;
        opts.isCrop = false;
        opts.pigzname[0] = 0;
        opts.filename[0] = 0;
        opts.isGz = false;
    }
};

class sFTPGE
{
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SESSION *session;
    struct sockaddr_in sin;
    int sock;
    const char *fingerprint;
    char *userauthlist;
    vector<fileObject> list, lastList;
    int numberOfTries;
    double lastCheck;
    int actualFileIndex;
    int nSlices;
    int firstVolume;
    dicomConverter dConv; 
    int connected;

public:
    char keyfile1[255];
    char keyfile2[255];
    string username;
    string password;
    string sftppath;
    string latestExamDir, latestSerieDir, previousSerieDir;

    unsigned long hostaddr;
    int port;
    int auth_pw;

    string latestSession(string &basedir);
    string latestDir(string &basedir);
    string latestSerie(string &sessionDir);
    int getFilelist(string &basedir, vector<fileObject>&list);
    int saveFile(string &filepath, stringstream &filemem);

    int initWinsock();
    int initSock();
    int initSSH();
    int initSSHSession();
    int initsFTPSession();

    int connectSession();
    int getFile(string &filepath, stringstream &filemem, int showErros=1);
    int downloadFileList(string &outputdir);
    int getFileList();
    int closeSock();
    int closeSSH();
    int closeSession();
    int hasNewFiles();
    int copyStep(string &outputdir);
    int isTimeToEnd();
    int getLatestExamDir();
    int getLatestSeriesDir();
    int findInputDir();
    int resetTries();
    int hasNewSeriesDir();
    int cleanUp();
    int convertVolumes(string &outputdir);
    int isConnected();

    
    sFTPGE()
    {
        strcpy(keyfile1, "");
        strcpy(keyfile2, "");
        
        username = "sdc";
        password = "adw2.0";

        if (testMode) 
           sftppath = "/test_data";
        else
           sftppath = "/export/home1/sdc_image_pool/images";
        
        auth_pw = 0;

        if (testMode)
           port = 2124;
        else
           port = 22;

        numberOfTries = 0;
        actualFileIndex = 0;
        lastCheck = 0;
        latestSerieDir   = "";
        previousSerieDir = "";
        firstVolume = 1;
        dConv.ge = this;
    }
};

double GetWallTime();
void reset(stringstream& stream);


