/*
 * Sample showing how to do SFTP transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password /tmp/secrets -p|-i|-k"
 */

#include "sftp.h"
#include "dirent.h"

int main(int argc, char *argv[])
{
    char *inputpath = NULL;
    if (argc > 1) {
        inputpath = argv[1];
    } 
    sFTPGE ge(inputpath);
    
    if (0)
    {
       if (argc > 2) {
          ge.hostaddr = inet_addr(argv[2]);
       }
       if (argc > 3) {
          ge.username = argv[3];
       }
       if(argc > 4) {
          ge.password = argv[4];
       }
       if(argc > 5) {
          ge.sftppath = argv[5];
       }
       /* if we got an 4. argument we set this option if supported */
       if(argc > 6) {
           if ((ge.auth_pw & 1) && !strcasecmp(argv[6], "-p")) {
              ge.auth_pw = 1;
           }
           if ((ge.auth_pw & 2) && !strcasecmp(argv[6], "-i")) {
              ge.auth_pw = 2;
           }
           if ((ge.auth_pw & 4) && !strcasecmp(argv[6], "-k")) {
              ge.auth_pw = 4;
           }
       }
    }

    int numSeries = 0;    
    ge.connectSession();

    // create parent output folder
    char logDir[1024];
    sprintf(logDir, "logs");
    mkdir(logDir, 0777);

    // create parent output folder
    char outputDir[1024];
    sprintf(outputDir, "output_scans");
    mkdir(outputDir, 0777);
    
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(outputDir)) == NULL) {
        cout << "Error(" << errno << ") opening " << outputDir << endl;
        return errno;
    }
    
    while ((dirp = readdir(dp)) != NULL) 
    {
        if ((strcmp(dirp->d_name, ".") != 0) && (strcmp(dirp->d_name, "..") != 0))
        {
           if (dirp->d_type == DT_DIR)
           { 
              char number[20];
              int p=0, q;
              fprintf(stderr, "Directory seeing %s\n", dirp->d_name);
              while ((p < strlen(dirp->d_name)) && (!isdigit(dirp->d_name[p]))) p++;
              q = p;
              while ((p < strlen(dirp->d_name)) && (isdigit(dirp->d_name[p]))) 
              {
                 number[p-q] = dirp->d_name[p];
                 p++;
              }
              number[p] = 0;
              int tempNum = atoi(number);
              if (numSeries < tempNum)
                 numSeries = tempNum;
              //fprintf(stderr, "Series = %d\n", numSeries);
           }
        }
    }
    closedir(dp);

    string timeStampString;
    timeStamp(timeStampString); 
    timeStampString = string(logDir) + "/" + timeStampString + ".txt";
    ge.logMain.initializeLogFile((char *)timeStampString.c_str());
    ge.logMain.writeLog(1, "Last series folder count = %d\n", numSeries);
    while (1)
    { 
       if (ge.findInputDir()) // search for new directory in the base folder
       {
          ge.cleanUp(); // clean memory variables
          if (ge.hasNewSeriesDir()) // new directory, let's work
          {  
             char outputdir[256], logName[256];
             numSeries++; 
             sprintf(outputdir, "%s/serie%.2d", outputDir, numSeries); // each dicom series will be written in a different folder
             sprintf(logName, "%s/serie%.2d.txt", outputDir, numSeries); // name of the log file for the serie
             ge.logSeries.resetLog();  
             ge.logMain.flushLog();
             mkdir(outputdir, 0777);

             ge.logSeries.initializeLogFile(logName);    
             // just converting char to string
             string outputDir = outputdir;
             ge.setStartTime();
             while (!ge.isTimeToEnd()) // while reading new files, copy and make niftis. Waiting a maximum of 3 secs for new slice files
             {
                ge.copyStep(outputDir);
             }
          }
       }  
    }
    ge.closeSession();
    return 0;
}
