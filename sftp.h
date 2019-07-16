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
#define numDigits 4 // to get before the point in test mode

class fileObject
{
public:
   string filename;
   int fileIndex;
   time_t time;
   int testMode;
   
    int isDicomFile()
    {
        if ((filename == ".") || (filename == ".."))
           return 0;
        if (!testMode) return 1;
        string::size_type idx;
        idx = filename.rfind('.');
        if (idx != std::string::npos)
        {
           string extension = filename.substr(idx+1);
           transform(extension.begin(), extension.end(), extension.begin(), ::toupper);
           return 1; //(extension == "DCM");
        }
        return 0;
    }

    void setFilename(char *file, int InTestMode)
    {
       testMode = InTestMode;
       filename = file;
       int point = 0;
       int startingPoint = 1;
       if (testMode==1)
          startingPoint = 5; // before .dcm

       for (int c=strlen(file)-startingPoint;c >-1; c--)
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

       if (!testMode)
       {
          for (int i=point+1; i<strlen(file); i++) number[i-point-1] = file[i];
          fileIndex = atoi(number);
       }
       else if (point > 0)
       {
          // transforming the last digits in numbers
          for (int i=0; i<numDigits; i++) number[i] = file[point-numDigits+i];
          for (int i=point+1; i<strlen(file)-4; i++) number2[i-point-1] = file[i];
          fileIndex = atoi(number) * 1000 + atoi(number2);
       }
    }   

    fileObject()
    {
       fileIndex = -1;
    }

    
    fileObject(char *file, int InTestMode)
    {
       setFilename(file, InTestMode);
    }
};

typedef struct
{
   bool operator()(fileObject const &a, fileObject const &b) const { return a.fileIndex < b.fileIndex; }
} fileSort;


class LogObject
{
	stringstream buffer;
	fstream outputStream;
	string fileName;
public:
	// create log file
	void initializeLogFile(char *filename);

	// closes the log file
	void closeLogFile();

	// writes the message in the log file
	void writeLog(int inScreen, const char * format, ...);
        void flushLog();
        void resetLog();
	LogObject() { fileName = ""; };
	~LogObject() { closeLogFile(); };
};

class sFTPGE
{
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SESSION *session;
    struct sockaddr_in sin;
    int sock;
    const char *fingerprint;
    char *userauthlist;
    string latestExamDir, latestSerieDir, previousSerieDir;
    vector<fileObject> list;
    int numberOfTries;
    double lastCheck;
    int actualFileIndex;
    int nSlices;
    int lastListSize;
    double lastTime;
    double timeBetweenReads;
    double startTime;
    int lastIndexChecked;
    int lastSliceListed;
    int mode;

public:
    char keyfile1[255];
    char keyfile2[255];
    string username;
    string password;
    string sftppath;
    int testMode;

    unsigned long hostaddr;
    int port;
    int auth_pw;
    LogObject logMain, logSeries;

    string latestSession(string &basedir);
    string latestDir(string &basedir);
    string _latestDir(string &basedir);
    string latestDirSFTP(string &basedir);

    string latestSerie(string &sessionDir);
    int getFilelist(string &basedir, vector<fileObject>&list);
    int updateFilelist(string &basedir, vector<fileObject>&list);
    int getFilelistSFTP(string &basedir, vector<fileObject>&list);

    int saveFile(string &filepath, stringstream &filemem);

    int initWinsock();
    int initSock();
    int initSSH();
    int initSSHSession();
    int initsFTPSession();

    int connectSession();
    int getFile(string &filepath, stringstream &filemem);
    int _getFile(string &filepath, stringstream &filemem);
    int getFileSFTP(string &filepath, stringstream &filemem);
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
    void setStartTime();
    int saveNifti(char * niiFilename, struct nifti_1_header hdr, unsigned char* im, struct TDCMopts opts);
    int indexExists(string &basedir, int indexToCheck, vector<fileObject>&list);

    sFTPGE(char *inputpath)
    {
        testMode = 0;
        strcpy(keyfile1, "");
        strcpy(keyfile2, "");
        
        username = "sdc";
        password = "adw2.0";
        mode = 1; // directory search
        if (testMode) 
        {
           hostaddr = htonl(0x7F000001);
           if (mode == 1)
              sftppath = "/home/osboxes/Desktop/Stanford/rtfmri/test_data";
        }
        else
        {
           hostaddr = inet_addr("192.168.0.10");
           if (mode == 1)
              sftppath = "/cnimr/images";
        }
        if (inputpath != NULL)
           sftppath = inputpath; 
        
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
        lastTime = 0;
        timeBetweenReads = 0.1;  // 50 ms
        lastIndexChecked = -1;
        lastListSize = 0;
        lastSliceListed = 0;
    }
};

double GetWallTime();
double GetMTime();
void reset(stringstream& stream);
void timeStamp(string &timestamp);


