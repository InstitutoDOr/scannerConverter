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
    /*
    fileObject f1("dicom.dcm1");
    if (f1.isDicomFile())
        fprintf(stderr, "is dicom\n");
      else
        fprintf(stderr, "is not dicom\n");

    return 0; 

    char fn1[] = "/test_data/p005/e16216/16216_5_2_dicoms/MR.1.2.840.113619.2.374.15512023.2926070.17242.1507999257.999.dcm";
    char fn2[] = "/test_data/p005/e16216/16216_5_2_dicoms/MR.1.2.840.113619.2.374.15512023.2926070.17242.1507999258.0.dcm";
    char fn3[] = "/test_data/p005/e16216/16216_5_2_dicoms/MR.1.2.840.113619.2.374.15512023.2926070.17242.1507999258.10.dcm";
    fileObject f1(fn1);
    fileObject f2(fn2);
    fileObject f3(fn3);
    fprintf(stderr, "Index values f1 = %d, f2 = %d, f3 = %d\n", f1.fileIndex, f2.fileIndex, f3.fileIndex);
    return 0;
    */
 
    sFTPGE ge;
    if (argc > 1) {
        ge.hostaddr = inet_addr(argv[1]);
    } else {
        ge.hostaddr = htonl(0x7F000001);
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
    sprintf(outputDir, "outputdir");
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