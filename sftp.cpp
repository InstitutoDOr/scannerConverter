/*
 * Sample showing how to do SFTP transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password /tmp/secrets -p|-i|-k"
 */

#include "sftp.h"
#include "stdarg.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include "dirent.h"

void timeStamp(string &timestamp)
{
   char auxString[100];
   time_t time_now;
   time(&time_now);
   struct tm *timeinfo = localtime(&time_now);
   strftime(auxString, 100, "%Y%m%d%H%M%S", timeinfo);
   timestamp = auxString;
}

double GetWallTime()
{

#ifdef WINDOWS
	return (double)getMilliCount() / 1000.0;
#else
	struct timeval time;
	if (gettimeofday(&time, NULL))
	{
		//  Handle error
		return 0;
	}
	return (double)time.tv_sec + (double)time.tv_usec * .000001;
#endif
}

double GetMTime()
{

#ifdef WINDOWS
	return (double)getMilliCount();
#else
	struct timeval time;
	if (gettimeofday(&time, NULL))
	{
		//  Handle error
		return 0;
	}
	return (double)time.tv_sec * 1000 + (double)time.tv_usec * .001;
#endif
}

void reset(stringstream& stream)
{
    const static stringstream initial;
    
    stream.str(string());
    stream.clear();
    stream.copyfmt(initial);
}

static void kbd_callback(const char *name, int name_len, 
             const char *instruction, int instruction_len, int num_prompts,
             const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
             LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
             void **abstract)
{
    int i;
    size_t n;
    char buf[1024];
    char *result;
    (void)abstract;

    fprintf(stderr, "Performing keyboard-interactive authentication.\n");

    fprintf(stderr, "Authentication name: '");
    fwrite(name, 1, name_len, stderr);
    fprintf(stderr, "'\n");

    fprintf(stderr, "Authentication instruction: '");
    fwrite(instruction, 1, instruction_len, stderr);
    fprintf(stderr, "'\n");

    fprintf(stderr, "Number of prompts: %d\n\n", num_prompts);

    for (i = 0; i < num_prompts; i++) {
        fprintf(stderr, "Prompt %d from server: '", i);
        fwrite(prompts[i].text, 1, prompts[i].length, stderr);
        fprintf(stderr, "'\n");

        fprintf(stderr, "Please type response: ");
        result = fgets(buf, sizeof(buf), stdin);
        n = strlen(buf);
        while (n > 0 && strchr("\r\n", buf[n - 1]))
          n--;
        buf[n] = 0;

        responses[i].text = strdup(buf);
        responses[i].length = n;

        fprintf(stderr, "Response %d from user is '", i);
        fwrite(responses[i].text, 1, responses[i].length, stderr);
        fprintf(stderr, "'\n\n");
    }

    fprintf(stderr,
        "Done. Sending keyboard-interactive responses to server now.\n");
}

void LogObject::resetLog()
{
    closeLogFile();
    reset(buffer);
    fileName = "";
}

void LogObject::initializeLogFile(char *filename)
{
	fileName = filename;
	outputStream.open(filename, fstream::out | fstream::trunc);
	outputStream << buffer.str().c_str();
}

// closes the log file
void LogObject::closeLogFile()
{
	if (fileName != "")
		outputStream.close();
}

void LogObject::flushLog()
{
	if (fileName != "")
		outputStream.flush();
}

void LogObject::writeLog(int inScreen, const char * format, ...)
{
	char auxBuffer[3000];
	char timestamp[100];


	time_t time_now;
	time(&time_now);
	struct tm *timeinfo = localtime(&time_now);
	strftime(timestamp, 100, "%Y-%m-%d %H:%M:%S : ", timeinfo);
	va_list args, screen_args;

	va_start(args, format);
	vsprintf(auxBuffer, format, args);
	va_end(args);

	if (fileName != "")
		outputStream << timestamp << auxBuffer;
	else
		buffer << timestamp << auxBuffer;

	va_start(screen_args, format);
	if (inScreen)
		vfprintf(stderr, format, screen_args);
	va_end(screen_args);
}

string sFTPGE::latestDirSFTP(string &basedir)
{
    unsigned long recentTime=0;
    int rc;
    string latestExamDir="";
    /* Request a dir listing via SFTP */
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp_session, basedir.c_str());

    if (!sftp_handle) {
        logMain.writeLog(1, "Unable to open dir with SFTP\n");
        return latestExamDir;
    }

    string latestDir;
    do {
        char mem[512];
        char longentry[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        /* loop until we fail */
        rc = libssh2_sftp_readdir_ex(sftp_handle, mem, sizeof(mem),
                                     longentry, sizeof(longentry), &attrs);
        if (rc > 0)
        {
           if ((attrs.mtime > recentTime) && (strcmp(mem, ".DS_Store")) && (strcmp(mem, ".")) && (strcmp(mem, "..")))
           {
              recentTime = attrs.mtime;
              latestDir = mem; 
           }
        }
        else
            break;

    } 
    while (1);
    if (latestDir.size() > 0) 
    {
       latestExamDir = basedir;
       latestExamDir += "/";
       latestExamDir += latestDir; 
    }
    libssh2_sftp_closedir(sftp_handle);
    return latestExamDir;
}

string sFTPGE::_latestDir(string &basedir)
{
    DIR *dp;
    unsigned long recentTime=0;
    struct dirent *dirp;
    struct stat attrs;
    string latestExamDir="";

    if((dp  = opendir(basedir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << basedir << endl;
        return latestExamDir;
    }
    
    string latestDir = "";
    while ((dirp = readdir(dp)) != NULL) 
    {
        if ((strcmp(dirp->d_name, ".") != 0) && (strcmp(dirp->d_name, "..") != 0))
        {
           if (dirp->d_type == DT_DIR)
           {
              char tempDir[512];
              sprintf(tempDir, "%s/%s", basedir.c_str(), dirp->d_name);
              stat(tempDir, &attrs);  
              if ((attrs.st_mtime > recentTime) && (strcmp(dirp->d_name, ".DS_Store")))
              {
                 recentTime = attrs.st_mtime;
                 latestDir = dirp->d_name; 
              }
           }
        }
    }
    closedir(dp);

    if (latestDir.size() > 0) 
    {
       latestExamDir = basedir;
       latestExamDir += "/";
       latestExamDir += latestDir; 
    }
    return latestExamDir;
}

string sFTPGE::latestDir(string &basedir)
{
    if (mode == 1)
       return _latestDir(basedir);
    else if (mode == 2)
       return latestDirSFTP(basedir);
}

string sFTPGE::latestSession(string &basedir)
{
    string lastPatientDir;
    string latestExamDir;
    
    // latest patient
    lastPatientDir = latestDir(basedir);

    // latest Session
    latestExamDir = latestDir(lastPatientDir);

    return latestExamDir;
}

string sFTPGE::latestSerie(string &sessionDir)
{
    string latestSerie = latestDir(sessionDir);
    return latestSerie;
}

int sFTPGE::getFilelistSFTP(string &basedir, vector<fileObject>&list)
{
    int rc;
    fileSort sortFile;
 
    /* Request a dir listing via SFTP */
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp_session, basedir.c_str());

    if (!sftp_handle) {
        logMain.writeLog(1, "Unable to open dir with SFTP\n");
        return 1;
    }

    string filename;
    do {
        char mem[512];
        char longentry[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        /* loop until we fail */
        rc = libssh2_sftp_readdir_ex(sftp_handle, mem, sizeof(mem),
                                     longentry, sizeof(longentry), &attrs);
        if (rc > 0)
        {
           fileObject fileObj(mem, testMode);
           fileObj.time = (time_t) attrs.mtime;
           
           if (fileObj.isDicomFile())
              list.push_back(fileObj);
        }
        else break;
    }
    while (1);
    libssh2_sftp_closedir(sftp_handle);
    sort(list.begin(), list.end(), sortFile);
    return 0;
}

int fileIndex(char *file)
{
   int point = 0;
   int startingPoint = 1;

   for (int c=strlen(file)-startingPoint;c >-1; c--)
   {
       if (file[c] == '.')
       {
          point = c;
          break;
       }   
   }

   char number[10];
   // cleaning the variables
   for (int i=0; i<10; i++) number[i] = 0; 

   for (int i=point+1; i<strlen(file); i++) number[i-point-1] = file[i];
   return atoi(number);
}

int sFTPGE::indexExists(string &basedir, int indexToCheck, vector<fileObject>&list)
{
    DIR *dp;
    char fname[512];
    struct dirent *dirp;
    if((dp  = opendir(basedir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << basedir << endl;
        return errno;
    }
    int inserted = 0;
    while ((dirp = readdir(dp)) != NULL) 
    {
        if ((strcmp(dirp->d_name, ".") != 0) && (strcmp(dirp->d_name, "..") != 0))
        {
           if (dirp->d_type == DT_REG)
           { 
              int idx = fileIndex(dirp->d_name);
              if ((idx >= indexToCheck) || ((idx < list.size()) && (list[idx-1].filename == "")) )
              {
                 struct stat attrs;
                 sprintf(fname, "%s/%s", basedir.c_str(), dirp->d_name);
                 stat(dirp->d_name, &attrs);
                 inserted ++;
                 if (list.size() < idx)
                    list.resize(idx);
                 list[idx-1].setFilename(dirp->d_name, testMode); 
                 list[idx-1].time = (time_t) attrs.st_mtime;
              } 
           }
        }
    }
    closedir(dp);
    return 0;
}

int sFTPGE::updateFilelist(string &basedir, vector<fileObject>&list)
{
    int indexToCheck = list.size()+1;
    lastListSize = list.size();
    return indexExists(basedir, indexToCheck, list);
}

int sFTPGE::getFilelist(string &basedir, vector<fileObject>&list)
{
   if (mode == 1)
      return updateFilelist(basedir, list);
   else if (mode == 2)
      return getFilelistSFTP(basedir, list);
}

int sFTPGE::getFileSFTP(string &filepath, stringstream &filemem)
{
    /* Request a file via SFTP */
    int rc;
    reset(filemem);
    LIBSSH2_SFTP_HANDLE *sftp_handle =
        libssh2_sftp_open(sftp_session, filepath.c_str(), LIBSSH2_FXF_READ, 0);

    if (!sftp_handle) {
        int errmsg_len;
        char *errmsg;
        logMain.writeLog(1, "Unable to open file with SFTP: %ld\n",
                libssh2_sftp_last_error(sftp_session));
                
        libssh2_session_last_error(session, &errmsg, &errmsg_len, 0); 
        logMain.writeLog(1, "Error : %s\n", errmsg);
        return 1;
    }
    
    do
    {
        char mem[SSHbufferSize];

        /* loop until we fail */
        rc = libssh2_sftp_read(sftp_handle, mem, sizeof(mem));
        if (rc > 0)
        {
            filemem.write(mem, rc);
        }
        else break;
    } while (1);
    
    libssh2_sftp_close(sftp_handle); 
    return 0;   
}

int sFTPGE::_getFile(string &filepath, stringstream &filemem)
{
    reset(filemem);
    struct stat attrs;
    if (stat(filepath.c_str(), &attrs) !=0)
       return -1;

    std::ifstream file(filepath, ios::binary );
    if ( file )
    {
        filemem << file.rdbuf();
        file.close();
        filemem.seekg(0, filemem.end);
        int fileLen=filemem.tellg();
        if  (fileLen < 1024) return -1;
        else return 0;
    }
    else return -1;
}

int sFTPGE::getFile(string &filepath, stringstream &filemem)
{
   if (mode == 1) 
      return _getFile(filepath, filemem);
   else
      return getFileSFTP(filepath, filemem);
}

int sFTPGE::saveFile(string &filepath, stringstream &filemem)
{
   ofstream outFile(filepath.c_str());
   outFile << filemem.rdbuf();
   outFile.close();
   return 0;
}

int sFTPGE::initWinsock()
{
#ifdef WIN32
    WSADATA wsadata;
    int err;

    err = WSAStartup(MAKEWORD(2,0), &wsadata);
    if (err != 0) {
        logMain.writeLog(1, "WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
    return 0;
}
 
int sFTPGE::initSSH()
{
    int rc = libssh2_init (0);
    if (rc != 0) {
        logMain.writeLog(1, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }
    else logMain.writeLog(1, "initialized!\n");
    
    return 0;
}

int sFTPGE::initSock()
{
    /*
     * The application code is responsible for creating the socket
     * and establishing the connection
     */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = hostaddr;
    if (connect(sock, (struct sockaddr*)(&sin),
                sizeof(struct sockaddr_in)) != 0) {
        logMain.writeLog(1, "failed to connect!\n");
        return -1;
    }
    else logMain.writeLog(1, "connected!\n");
    
    return 0;
}


int sFTPGE::initSSHSession()
{
    /* Create a session instance
     */
    session = libssh2_session_init();
    if(!session)
        return -1;
    
    /* Since we have set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking(session, 1);
    
    /* ... start it up. This will trade welcome banners, exchange keys,
     * and setup crypto, compression, and MAC layers
     */
    int rc = libssh2_session_handshake(session, sock);
    if(rc) {
        logMain.writeLog(1, "Failure establishing SSH session: %d\n", rc);
        return -1;
    }
    
    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    
    logMain.writeLog(1, "Fingerprint: ");
    for(int i = 0; i < 20; i++) {
        logMain.writeLog(1, "%02X ", (unsigned char)fingerprint[i]);
    }
    logMain.writeLog(1, "\n");
    
    
    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username.c_str(), username.size());
    logMain.writeLog(1, "Authentication methods: %s\n", userauthlist);
    if (strstr(userauthlist, "password") != NULL) {
        auth_pw |= 1;
    }
    if (strstr(userauthlist, "keyboard-interactive") != NULL) {
        auth_pw |= 2;
    }
    if (strstr(userauthlist, "publickey") != NULL) {
        auth_pw |= 4;
    }

    if (auth_pw & 1) {
        /* We could authenticate via password */
        if (libssh2_userauth_password(session, username.c_str(), password.c_str())) {
            logMain.writeLog(1, "Authentication by password failed.\n");
            return -1;
        }
    } else if (auth_pw & 2) {
        /* Or via keyboard-interactive */
        if (libssh2_userauth_keyboard_interactive(session, username.c_str(), &kbd_callback) ) {
            logMain.writeLog(1, 
                    "\tAuthentication by keyboard-interactive failed!\n");
            return -1;
        } else {
            logMain.writeLog(1, 
                    "\tAuthentication by keyboard-interactive succeeded.\n");
        }
    } else if (auth_pw & 4) {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile(session, username.c_str(), keyfile1, keyfile2, password.c_str())) {
            logMain.writeLog(1, "\tAuthentication by public key failed!\n");
            return -1;
        } else {
            logMain.writeLog(1, "\tAuthentication by public key succeeded.\n");
        }
    } else {
        logMain.writeLog(1, "No supported authentication methods found!\n");
        return -1;
    }
    
    libssh2_session_flag(session, LIBSSH2_FLAG_COMPRESS, 1);
    return 0;
}

int sFTPGE::initsFTPSession()
{
    logMain.writeLog(0, "libssh2_sftp_init()!\n");
    sftp_session = libssh2_sftp_init(session);
    
    if (!sftp_session) {
        logMain.writeLog(1, "Unable to init SFTP session\n");
        return -1;
    }
    
    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking(session, 1);
    return 0;
}

int sFTPGE::connectSession()
{
    if (mode == 2)
    {
       initWinsock();
       initSSH();
       initSock();
       initSSHSession();
       initsFTPSession();
    }
    return 0;
}

int sFTPGE::hasNewFiles()
{
   return (list.size()-lastListSize);
}

int sFTPGE::getLatestExamDir()
{
    string examDir = latestSession(sftppath);
    if (examDir.size() > 0)
    {
        if  (examDir != latestExamDir)
        {
           latestExamDir = examDir;
           logMain.writeLog(1, "Most recent path : %s\n", latestExamDir.c_str());
        } 
        return 1;
    }
    else return 0;
}

int sFTPGE::hasNewSeriesDir()
{
   return (previousSerieDir != latestSerieDir);
}

int sFTPGE::getLatestSeriesDir()
{
    string seriesDir = latestSerie(latestExamDir);
    if ((seriesDir.size() > 0) && (seriesDir != latestSerieDir))
    {
        previousSerieDir = latestSerieDir;
        latestSerieDir = seriesDir; 
        logMain.writeLog(1, "Most recent series path : %s\n", latestSerieDir.c_str());
        return 1;
    }
    else return 0;
}

int sFTPGE::getFileList()
{
    double ini = GetWallTime();
    lastListSize = list.size();
    if (mode == 2) 
       list.clear();
    getFilelist(latestSerieDir, list);
    double end = GetWallTime();
    logSeries.writeLog(1, "Time to get list %f sec\n", end-ini);
    return 0;
}

int sFTPGE::findInputDir()
{
   if (getLatestExamDir())
      return getLatestSeriesDir();
   else return 0;
}

void sFTPGE::setStartTime()
{
   startTime = GetMTime();
}

int sFTPGE::saveNifti(char * niiFilename, struct nifti_1_header hdr, unsigned char* im, struct TDCMopts opts) 
{
    hdr.vox_offset = 352;
    size_t imgsz = nii_ImgBytes(hdr);
    if (imgsz < 1) {
    	logSeries.writeLog(1, "Error: Image size is zero bytes %s\n", niiFilename);
    	return 1;
    }

    char fname[2048] = {""};
    strcpy (fname,niiFilename);
    strcat (fname,".nii");
    FILE *fp = fopen(fname, "wb");
    if (!fp) 
    {
       logSeries.writeLog(1, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!    Error opening the file %s for writing !!!\n", fname);
       return 2;
    } 
    fwrite(&hdr, sizeof(hdr), 1, fp);
    uint32_t pad = 0;
    fwrite(&pad, sizeof( pad), 1, fp);
    fwrite(&im[0], imgsz, 1, fp);
    fclose(fp);
    return 0;
}

int sFTPGE::downloadFileList(string &outputdir)
{
    double ini = GetWallTime();
    struct nifti_1_header hdr;
    unsigned char * imgM=NULL, *img=NULL;
    size_t imgsz=0;
    int nslices=0;
    int sliceDir = 0;
    struct TDCMopts opts;
    struct TDICOMdata firstHeader;

    opts.isCreateBIDS = false;
    opts.isForceStackSameSeries = false;
    opts.isOnlySingleFile = true;
    opts.isCreateText = false;
    opts.isVerbose = 0;
    opts.isCrop = false;
    opts.pigzname[0] = 0;
    opts.filename[0] = 0;
    opts.isGz = false;

    for (int t=actualFileIndex; t<list.size(); t++)
    {
        if (list[t].filename == "")
        {
           logSeries.writeLog(1, "Slice file with index %d not found\n", t+1); 
           break;
        }
        stringstream filemem;
        string fname = latestSerieDir + "/" + list[t].filename;
        if (getFile(fname, filemem)==0)
        {
            TDTI4D unused;
            struct TDICOMdata d = readDICOMv(filemem, 0, 0, &unused);
            if (t==actualFileIndex)
            {
                if (headerDcm2Nii(d, &hdr, true) != EXIT_FAILURE)
                {
                    imgsz = nii_ImgBytes(hdr);
                    filemem.seekg(0, filemem.end);
                    int fileLen=filemem.tellg();
                    logSeries.writeLog(1, "filename = %s, file size = %d, slice size=%ld numslices = %d\n", fname.c_str(), fileLen, imgsz, d.locationsInAcquisition); 

                    hdr.dim[3] = d.locationsInAcquisition;
                    for (int i = 4; i < 8; i++) hdr.dim[i] = 0;
                    imgM = (unsigned char *)malloc(imgsz* (uint64_t)d.locationsInAcquisition);
                    img = (unsigned char *)malloc(imgsz);
                    nSlices = d.locationsInAcquisition;
                }
            }
            
            if ((imgsz > 0) && (nSlices > 0))
            {
                time_t creationTime = list[t].time;    
                time_t actualTime;
                time(&actualTime); 
                if (t > lastIndexChecked) 
                {
                   logSeries.writeLog(1, "file read = %s \nTimestamp (Creation) = %sTimeStamp (Viewing from beging sequence aquisition) = %2.3f ms\n", fname.c_str(), ctime(&creationTime), (GetMTime()-startTime));
                }
                int i = t % d.locationsInAcquisition;
                if (d.imageStart == 0)
                {
                    filemem.seekg(0, filemem.end);
                    int fileLen=filemem.tellg(); //Get file length
                    d.imageStart = fileLen-imgsz;
                }

                //fprintf(stderr, "Reading %ld bytes from %d\n", imgsz, d.imageStart); 
                filemem.seekg(d.imageStart);
                filemem.read((char *)img, imgsz);
                if (filemem)
                {
                    memcpy(&imgM[(uint64_t)(d.locationsInAcquisition-1-i)*imgsz], &img[0], imgsz);
                    if (t > lastIndexChecked)
                    { 
                       logSeries.writeLog(1, "Writing (in memory) slice %d of volume %d TimeStamp = %2.3f ms\n\n", (i+1), ((int)(t / d.locationsInAcquisition) + 1), (GetMTime()-startTime));
                       lastIndexChecked = t;
                    }
                    nslices++;
                }
                
                if (nslices == d.locationsInAcquisition)
                {
                    char outputname[1024];
                    
                    int volumeIndex = ((int)(t / d.locationsInAcquisition) + 1);
                    sprintf(outputname, "%s/vol_%.5d", outputdir.c_str(), volumeIndex);
                    saveNifti(outputname, hdr, imgM, opts);
                    nslices=0;
                    actualFileIndex=t+1;
                    if (actualFileIndex+nSlices > list.size())
                       break;
         
                    time_t actualTime;
                    time(&actualTime); 
                    logSeries.writeLog(1, "Volume %d written. File name = %s\n", volumeIndex, outputname);
                    logSeries.writeLog(1, "Timestamp (Volume creation) = %s", ctime(&actualTime));
                    logSeries.writeLog(1, "Timestamp (millisecs from sequence start) = %2.3f\n\n", (GetMTime()-startTime));
                    logSeries.flushLog();
                }
            }
        }
        else 
        {
            logSeries.writeLog(1, "Slice file %s has invalid filesize\n", fname.c_str());
            break;
        }
    }
    if (img)
       free(img);

    if (imgM)
       free(imgM);
    logSeries.writeLog(1, "Time to get files %f sec\n\n\n", GetWallTime()-ini);
    return 0;
}

int sFTPGE::resetTries()
{
   numberOfTries = 0;
   lastCheck = 0;
   list.clear();
   lastListSize = 0;
   return 0;
}

int sFTPGE::cleanUp()
{
   resetTries();
   nSlices = 0;
   actualFileIndex = 0;
   lastIndexChecked = -1;
   lastSliceListed = 0;
   resetTries(); 
   return 0;
}

int sFTPGE::isTimeToEnd()
{
   if (!hasNewFiles())
   {
      if (lastCheck == 0)
         lastCheck = GetWallTime();

      if (GetWallTime()-lastCheck > timeBetweenChecks)
      {
         lastCheck = GetWallTime();
         numberOfTries++;   
      }
   }
   else 
   {
      numberOfTries = 0;
      lastCheck = 0;
   }
   return numberOfTries > maximumTries; 
}

int sFTPGE::copyStep(string &outputdir)
{
   if ((GetWallTime()-lastTime) > timeBetweenReads)
   {
      getFileList();
      lastTime = GetWallTime();
      if ((hasNewFiles()) && (actualFileIndex+nSlices <= list.size()))
      {
         downloadFileList(outputdir);
      }
   }
   return isTimeToEnd();
}

int sFTPGE::closeSession()
{
    if (mode == 2)
    {
       closeSSH();
       closeSock();
    }
    return 0;
}

int sFTPGE::closeSSH()
{
   libssh2_sftp_shutdown(sftp_session);
   libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
   libssh2_session_free(session);
    
   return 0;
}

int sFTPGE::closeSock()
{
#ifdef WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    logMain.writeLog(1, "all done\n");
    
    libssh2_exit();
    return 0;
}
