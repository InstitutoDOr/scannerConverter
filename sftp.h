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
#define testMode 1

class fileObject
{
public:
   string filename;
   int fileIndex;
   time_t time;
   
    int isDicomFile()
    {
        if ((filename == ".") || (filename == ".."))
           return 0;
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

    fileObject(char *file)
    {
       filename = file;
       int point = 0;
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

       if (1)
       {
          for (int i=point+1; i<strlen(file); i++) number[i-point-1] = file[i];
          fileIndex = atoi(number);
       }
       else if (point > 0)
       {
          // transforming the last digits in numbers
          for (int i=0; i<3; i++) number[i] = file[point-3+i];
          for (int i=point+1; i<strlen(file)-4; i++) number2[i-point-1] = file[i];
          fileIndex = atoi(number) * 1000 + atoi(number2);
       }
    }   
};

typedef struct
{
   bool operator()(fileObject const &a, fileObject const &b) const { return a.fileIndex < b.fileIndex; }
} fileSort;

class sFTPGE
{
    LIBSSH2_SFTP *sftp_session;
    LIBSSH2_SESSION *session;
    struct sockaddr_in sin;
    int sock;
    const char *fingerprint;
    char *userauthlist;
    string latestExamDir, latestSerieDir, previousSerieDir;
    vector<fileObject> list, lastList;
    int numberOfTries;
    double lastCheck;
    int actualFileIndex;
    int nSlices;
    double lastTime;
    double timeBetweenReads;

public:
    char keyfile1[255];
    char keyfile2[255];
    string username;
    string password;
    string sftppath;

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
    int getFile(string &filepath, stringstream &filemem);
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
        lastTime = 0;
        timeBetweenReads = 0.5;
    }
};

double GetWallTime();
void reset(stringstream& stream);


