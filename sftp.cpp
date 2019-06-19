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

string sFTPGE::latestDir(string &basedir)
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

string sFTPGE::latestSession(string &basedir)
{
    string lastPatientDir;
    string latestExamDir;
    
    // latest patient
    //fprintf(stderr, "basedir = %s\n", basedir.c_str());
    lastPatientDir = latestDir(basedir);

    // latest Session
    latestExamDir = latestDir(lastPatientDir);

    //fprintf(stderr, "PatientDir = %s\n", lastPatientDir.c_str());
    //fprintf(stderr, "ExamDir = %s\n", latestExamDir.c_str());
    return latestExamDir;
}

string sFTPGE::latestSerie(string &sessionDir)
{
    string latestSerie = latestDir(sessionDir);
    return latestSerie;
}

int sFTPGE::getFilelist(string &basedir, vector<fileObject>&list)
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

int sFTPGE::getFile(string &filepath, stringstream &filemem)
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

int sFTPGE::connectSession()
{
    initWinsock();
    initSSH();
    initSock();
    initSSHSession();
    initsFTPSession();
    
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

int sFTPGE::hasNewFiles()
{
//   fprintf(stderr, "Lista atual = %ld, Anterior = %ld\n", list.size(), lastListSize);
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
//   fprintf(stderr, "previousSerieDir = %s, actual = %s\n", previousSerieDir.c_str(), latestSerieDir.c_str());
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
                    logSeries.writeLog(1, "slice size=%ld numslices = %d\n", imgsz, d.locationsInAcquisition);
                    hdr.dim[3] = d.locationsInAcquisition;
                    for (int i = 4; i < 8; i++) hdr.dim[i] = 0;
                    imgM = (unsigned char *)malloc(imgsz* (uint64_t)d.locationsInAcquisition);
                    img = (unsigned char *)malloc(imgsz);
                    nSlices = d.locationsInAcquisition;
                }
            }
            
            if (imgsz > 0)
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
                    nii_saveNII(outputname, hdr, imgM, opts);
                    nslices=0;
                    actualFileIndex=t+1;
                    if (actualFileIndex+nSlices > list.size())
                       break;
         
                    time_t actualTime;
                    time(&actualTime); 
                    logSeries.writeLog(1, "Volume %d written.\n", volumeIndex);
                    logSeries.writeLog(1, "Timestamp (Volume creation) = %s", ctime(&actualTime));
                    logSeries.writeLog(1, "Timestamp (millisecs from sequence start) = %2.3f\n\n", (GetMTime()-startTime));
                    logSeries.flushLog();
                }
            }
        };
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
      if ((hasNewFiles()) && (actualFileIndex+nSlices < list.size()))
      {
         downloadFileList(outputdir);
      }
   }
   return isTimeToEnd();
}

int sFTPGE::closeSession()
{
    closeSSH();
    closeSock();
    
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
