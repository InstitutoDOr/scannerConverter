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
    sFTPGE ge;
    if (argc > 1) {
        ge.hostaddr = inet_addr(argv[1]);
    } else {
        ge.hostaddr = inet_addr("192.168.0.10");
//        ge.hostaddr = htonl(0x7F000001);
    }

    if(argc > 2) {
        ge.username = argv[2];
    }
    if(argc > 3) {
        ge.password = argv[3];
    }
    if(argc > 4) {
        ge.sftppath = argv[4];
    }

    /* if we got an 4. argument we set this option if supported */
    if(argc > 5) {
        if ((ge.auth_pw & 1) && !strcasecmp(argv[5], "-p")) {
            ge.auth_pw = 1;
        }
        if ((ge.auth_pw & 2) && !strcasecmp(argv[5], "-i")) {
            ge.auth_pw = 2;
        }
        if ((ge.auth_pw & 4) && !strcasecmp(argv[5], "-k")) {
            ge.auth_pw = 4;
        }
    }

    int numSeries = 0;    
    ge.connectSession();

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
           char number[20];
           int p=0, q;
           while ((p < strlen(dirp->d_name)) && (!isdigit(dirp->d_name[p]))) p++;
           q = p;
           while ((p < strlen(dirp->d_name)) && (isdigit(dirp->d_name[p]))) 
           {
              number[p-q] = dirp->d_name[p];
              p++;
           }
           number[p] = 0;
           numSeries = atoi(number);
           //fprintf(stderr, "Series = %d\n", numSeries);
        }
    }
    closedir(dp);
    fprintf(stderr, "Last series folder count = %d\n", numSeries);

    while (1)
    { 
       if (ge.findInputDir()) // search for new directory in the base folder
       {
          ge.cleanUp(); // clean memory variables
          if (ge.hasNewSeriesDir()) // new directory, let's work
          {
             char outputdir[256];
             numSeries++; 
             sprintf(outputdir, "%s/serie%.2d", outputDir, numSeries); // each dicom series will be written in a different folder
             mkdir(outputdir, 0777);

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
