/*
 * Sample showing how to do SFTP transfers.
 *
 * The sample code has default values for host name, user name, password
 * and path to copy, but you can specify them on the command line like:
 *
 * "sftp 192.168.0.1 user password /tmp/secrets -p|-i|-k"
 */

#include "sftp.h"

// get time in seconds
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

// reset file stream
void reset(stringstream& stream)
{
    const static stringstream initial;
    
    stream.str(string());
    stream.clear();
    stream.copyfmt(initial);
}

//  not used rigth now
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

// search for the latest dir inside a base folder
string sFTPGE::latestDir(string &basedir)
{
    unsigned long recentTime=0;
    int rc;
    string newestDir="";
    /* Request a dir listing via SFTP */
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp_session, basedir.c_str());

    if (!sftp_handle) {
        fprintf(stderr, "Unable to open dir with SFTP\n");
        return newestDir;
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
       newestDir = basedir;
       newestDir += "/";
       newestDir += latestDir; 
    }
    libssh2_sftp_closedir(sftp_handle);
    return newestDir;
}

// get the latest exam dir 
string sFTPGE::latestSession(string &basedir)
{
    string lastPatientDir;
    string latestExamDir;
    
    // get latest patient folder from base dir
    //fprintf(stderr, "basedir = %s\n", basedir.c_str());
    lastPatientDir = latestDir(basedir);

    // latest Session/study folder from patient dir
    latestExamDir = latestDir(lastPatientDir);

    //fprintf(stderr, "PatientDir = %s\n", lastPatientDir.c_str());
    //fprintf(stderr, "ExamDir = %s\n", latestExamDir.c_str());
    return latestExamDir;
}

// get the latest serie dir from exame dir
string sFTPGE::latestSerie(string &sessionDir)
{
    string latestSerie = latestDir(sessionDir);
    return latestSerie;
}

// get a list of files from a directory
int sFTPGE::getFilelist(string &basedir, vector<fileObject>&list)
{
    int rc;
    fileSort sortFile;
 
    /* Request a dir listing via SFTP */
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(sftp_session, basedir.c_str());

    if (!sftp_handle) {
        fprintf(stderr, "Unable to open dir with SFTP\n");
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
           fileObject fileObj(mem);
           fileObj.time = attrs.mtime;
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

// read a file from sFTP to memory
int sFTPGE::getFile(string &filepath, stringstream &filemem, int showErrors)
{
    /* Request a file via SFTP */
    int rc;
    reset(filemem);
    LIBSSH2_SFTP_HANDLE *sftp_handle =
        libssh2_sftp_open(sftp_session, filepath.c_str(), LIBSSH2_FXF_READ, 0);

    if (!sftp_handle) {
        if (showErrors)
        {
           int errmsg_len;
           char *errmsg;
           fprintf(stderr, "Unable to open file with SFTP: %ld\n",
                libssh2_sftp_last_error(sftp_session));
                
           libssh2_session_last_error(session, &errmsg, &errmsg_len, 0); 
           fprintf(stderr, "Error : %s\n", errmsg);
        }
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

// save a file from memory stream to disk
int sFTPGE::saveFile(string &filepath, stringstream &filemem)
{
   ofstream outFile(filepath.c_str());
   outFile << filemem.rdbuf();
   outFile.close();
   return 0;
}

// init socket (just for Windows)
int sFTPGE::initWinsock()
{
#ifdef WIN32
    WSADATA wsadata;
    int err;

    err = WSAStartup(MAKEWORD(2,0), &wsadata);
    if (err != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", err);
        return 1;
    }
#endif
    return 0;
}

// init libSSH environment
int sFTPGE::initSSH()
{
    int rc = libssh2_init (0);
    if (rc != 0) {
        fprintf(stderr, "libssh2 initialization failed (%d)\n", rc);
        return 1;
    }
    else fprintf(stderr, "libssh initialized!\n");
    
    return 0;
}

// init socket variable
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
        fprintf(stderr, "failed to connect in socket level!\n");
        return -1;
    }
    else fprintf(stderr, "connected!\n");
    
    return 0;
}

// prepare all related network stuff to initiate a ssh connection
int sFTPGE::connectSession()
{
    connected = 0;
    initWinsock();
    initSSH();
    initSock();
    initSSHSession();
    initsFTPSession();
    
    return 0;
}

// prepare all stuuf related to initiate a libssh session
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
        fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
        return -1;
    }
    
    /* At this point we havn't yet authenticated.  The first thing to do
     * is check the hostkey's fingerprint against our known hosts Your app
     * may have it hard coded, may go to a file, may present it to the
     * user, that's your call
     */
    fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    
    fprintf(stderr, "Fingerprint: ");
    for(int i = 0; i < 20; i++) {
        fprintf(stderr, "%02X ", (unsigned char)fingerprint[i]);
    }
    fprintf(stderr, "\n");
    
    
    /* check what authentication methods are available */
    userauthlist = libssh2_userauth_list(session, username.c_str(), username.size());
    fprintf(stderr, "Authentication methods: %s\n", userauthlist);
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
            fprintf(stderr, "Authentication by password failed.\n");
            return -1;
        }
    } else if (auth_pw & 2) {
        /* Or via keyboard-interactive */
        if (libssh2_userauth_keyboard_interactive(session, username.c_str(), &kbd_callback) ) {
            fprintf(stderr,
                    "\tAuthentication by keyboard-interactive failed!\n");
            return -1;
        } else {
            fprintf(stderr,
                    "\tAuthentication by keyboard-interactive succeeded.\n");
        }
    } else if (auth_pw & 4) {
        /* Or by public key */
        if (libssh2_userauth_publickey_fromfile(session, username.c_str(), keyfile1, keyfile2, password.c_str())) {
            fprintf(stderr, "\tAuthentication by public key failed!\n");
            return -1;
        } else {
            fprintf(stderr, "\tAuthentication by public key succeeded.\n");
        }
    } else {
        fprintf(stderr, "No supported authentication methods found!\n");
        return -1;
    }
    
    libssh2_session_flag(session, LIBSSH2_FLAG_COMPRESS, 1);
    return 0;
}

// initiate de facto a sftp session
int sFTPGE::initsFTPSession()
{
    //fprintf(stderr, "libssh2_sftp_init()!\n");
    sftp_session = libssh2_sftp_init(session);
    
    if (!sftp_session) {
        fprintf(stderr, "Unable to init SFTP session\n");
        return -1;
    }
    
    /* Since we have not set non-blocking, tell libssh2 we are blocking */
    libssh2_session_set_blocking(session, 1);
    connected = 1;
    return 0;
}

// return if the object successfully conected to the server
int sFTPGE::isConnected()
{
   return connected;
}

// compares to list to indicate that new files arrived
int sFTPGE::hasNewFiles()
{
//   fprintf(stderr, "Lista atual = %ld, Anterior = %ld\n", list.size(), lastList.size());
   return (list.size()-lastList.size());
}

// returns the latest exam/study dir not equal to the last vieweddd path
int sFTPGE::getLatestExamDir()
{
    string examDir = latestSession(sftppath);
    if (examDir.size() > 0)
    {
        if  (examDir != latestExamDir)
        {
           latestExamDir = examDir;
           fprintf(stderr, "Most recent path : %s\n", latestExamDir.c_str());
        } 
        return 1;
    }
    else return 0;
}

// verifies if we had a new series folder
int sFTPGE::hasNewSeriesDir()
{
//   fprintf(stderr, "previousSerieDir = %s, actual = %s\n", previousSerieDir.c_str(), latestSerieDir.c_str());
   return (previousSerieDir != latestSerieDir);
}

// get the last series dir
int sFTPGE::getLatestSeriesDir()
{
    string seriesDir = latestSerie(latestExamDir);
    if ((seriesDir.size() > 0) && (seriesDir != latestSerieDir))
    {
        previousSerieDir = latestSerieDir;
        latestSerieDir = seriesDir; 
        fprintf(stderr, "Most recent series path : %s\n", latestSerieDir.c_str());
        return 1;
    }
    else return 0;
}

// get the file list of the current seriesDir
int sFTPGE::getFileList()
{
    double ini = GetWallTime();
    lastList = list;
    list.clear();
    getFilelist(latestSerieDir, list);
    double end = GetWallTime();
    //fprintf(stderr, "Time to get list %f sec\n", end-ini);
    return 0;
}

// find the latest series dir
int sFTPGE::findInputDir()
{
   if (getLatestExamDir())
      return getLatestSeriesDir();
   else return 0;
}

// converts a list of dicom slices in nifti format
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

    // the file list is ordered by file index, so its ok to pass by list index.
    for (int t=actualFileIndex; list.size(); t++)
    {
        stringstream filemem; // memory stream that holds the file information
        string fname = latestSerieDir + "/" + list[t].filename; // filename to retrieve
        if (getFile(fname, filemem)==0) // getting the file from ssh
        {
            TDTI4D unused;
            struct TDICOMdata d = readDICOMv(filemem, 0, 0, &unused); // read the file
            if (t==actualFileIndex) // if first, get header and imgsize, numslices
            {
                if (headerDcm2Nii(d, &hdr, true) != EXIT_FAILURE)
                {
                    imgsz = nii_ImgBytes(hdr);
                    nSlices = d.locationsInAcquisition;
                    fprintf(stderr, "slice size=%ld numslices = %d\n", imgsz, d.locationsInAcquisition);
                    hdr.dim[3] = d.locationsInAcquisition;
                    for (int i = 4; i < 8; i++) hdr.dim[i] = 0;
                    imgM = (unsigned char *)malloc(imgsz* (uint64_t)d.locationsInAcquisition);
                    img = (unsigned char *)malloc(imgsz);
                }
            }
            
            if (imgsz > 0)
            {
                int i = t % d.locationsInAcquisition;
                //fprintf(stderr, "index = %d file read = %s\n", i, fname.c_str());
                // if the header do not inform the ImageStart, it will be file size minus the image size from previous calculations.
                // image size did not change and there is no compression
                if (d.imageStart == 0)
                {
                    filemem.seekg(0, filemem.end);
                    int fileLen=filemem.tellg(); //Get file length
                    d.imageStart = fileLen-imgsz;
                }

                // reads image data to buffer
                //fprintf(stderr, "Reading %ld bytes from %d\n", imgsz, d.imageStart); 
                filemem.seekg(d.imageStart);
                filemem.read((char *)img, imgsz);
                if (filemem)
                {
                    memcpy(&imgM[(uint64_t)(d.locationsInAcquisition-i-1)*imgsz], &img[0], imgsz);
                    //fprintf(stderr, "Writing slice %d of volume %d\n", (i+1), ((int)(t / d.locationsInAcquisition) + 1));
                    nslices++;
                }
                
                // if all slices in the deck, save the volume
                if (nslices == d.locationsInAcquisition)
                {
                    char outputname[1024];
                    
                    int volumeIndex = ((int)(t / d.locationsInAcquisition) + 1);
                    sprintf(outputname, "%s/vol_%.5d", outputdir.c_str(), volumeIndex);
                    nii_saveNII(outputname, hdr, imgM, opts);
                    nslices=0;
                    actualFileIndex=t+1;
                    fprintf(stderr, "Volume %d written.\n", volumeIndex);
                }
            }
        };
    }
    if (img)
       free(img);

    if (imgM)
       free(imgM);
    fprintf(stderr, "Time to get files %f sec\n", GetWallTime()-ini);
    return 0;
}

// convert the memory maintained list of read files to nifti 
int sFTPGE::convertVolumes(string &outputdir)
{
    if (list.size() == 0) return 0;
    if (firstVolume)
    {
       fprintf(stderr, "Setting the volume information\n");
       //dConv.ge = this;
       dConv.setFirstSliceName((char *)list[0].filename.c_str());
       firstVolume = 0;
    }
    else 
    {
       if (dConv.checkVolumeStatus())
          dConv.convert2Nii(outputdir);
    }
    return 0;
}

int sFTPGE::cleanUp()
{
   resetTries();
   nSlices = 0;
   actualFileIndex = 0;
   resetTries();
   firstVolume = 1;
   return 0;
}

// verifies if its time to search for a new directory
int sFTPGE::isTimeToEnd()
{
   // if no files
   if (!hasNewFiles())
   {
      if (lastCheck == 0)
         lastCheck = GetWallTime();

      // and more than 1 second has passed 
      if (GetWallTime()-lastCheck > timeBetweenChecks)
      {
         // increment the number of tries
         lastCheck = GetWallTime();
         numberOfTries++;   
      }
   }
   else 
   {
      numberOfTries = 0;
      lastCheck = 0;
   }
   // if number of tries greater than maximumTries exit
   return numberOfTries > maximumTries; 
}

// resets tries checks
int sFTPGE::resetTries()
{
   numberOfTries = 0;
   lastCheck = 0;
   list.clear();
   lastList.clear();
   return 0;
}

int sFTPGE::copyStep(string &outputdir)
{
   if (firstVolume)
   {
      fprintf(stderr, "Reading file list\n"); 
      getFileList();
   }
   convertVolumes(outputdir);
   return isTimeToEnd();
}

// closes the SSH session
int sFTPGE::closeSSH()
{
   libssh2_sftp_shutdown(sftp_session);
   libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
   libssh2_session_free(session);
    
   return 0;
}

// closes the socket variables
int sFTPGE::closeSock()
{
#ifdef WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    fprintf(stderr, "all done\n");
    
    libssh2_exit();
    return 0;
}

// closes all types of connections (socket and ssh)
int sFTPGE::closeSession()
{
   closeSSH();
   closeSock();
   return 0;
}

void dicomConverter::setFirstSliceName(char *filename)
{
   reset();
   fileObject fo(filename);
   fileTemplate = fo.fileTemplate;
   firstSliceIndex = fo.fileIndex;
   prepareVariables(); 
}

string dicomConverter::fileIndex(int index)
{
   string fname = ge->latestSerieDir + "/" + fileTemplate + to_string((int) index / 1000) + "." + to_string((int) index % 1000);
   if (!noDCM)
      fname = fname + ".dcm";
   return fname;
}

void dicomConverter::resetFlag()
{
   for (int t=0;t < nslices; t++)
      sliceReady[t] = 0;
}

void dicomConverter::prepareVariables()
{
   TDTI4D unused;
   stringstream filemem;

   string filename = fileIndex(firstSliceIndex);
   if (ge->getFile(filename, filemem)==0)
   {
      struct TDICOMdata d = readDICOMv(filemem, 0, 0, &unused);
      if (headerDcm2Nii(d, &hdr, true) != EXIT_FAILURE)
      {
         imgsz = nii_ImgBytes(hdr);
         fprintf(stderr, "slice size=%ld numslices = %d\n", imgsz, d.locationsInAcquisition);
         hdr.dim[3] = d.locationsInAcquisition;
         for (int i = 4; i < 8; i++) hdr.dim[i] = 0;
         imgM = (unsigned char *) malloc(imgsz* (uint64_t)d.locationsInAcquisition);
         img  = (unsigned char *) malloc(imgsz);
         nslices = d.locationsInAcquisition;
         sliceReady = (int *) malloc(nslices * sizeof(int));
         resetFlag();
      }
   }
}

int dicomConverter::checkVolumeStatus()
{
   int ready=1;
   
   if (nslices == 0)
   {
      prepareVariables();
      ready=0;
   } 
   else
   { 
      stringstream filemem;

      for (int t=0;t<nslices;t++)
      {
          if (sliceReady[t]==0)
          {
             string filename = fileIndex(firstSliceIndex+t);
             //fprintf(stderr, "index = %d filename = %s \n", t, filename.c_str());
             if (ge->getFile(filename, filemem, 0)==0)
             {
                TDTI4D unused;
                struct TDICOMdata d = readDICOMv(filemem, 0, 0, &unused); // read the file
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
                    memcpy(&imgM[(uint64_t)(nslices-t-1)*imgsz], &img[0], imgsz);
                    //fprintf(stderr, "Writing slice %d of volume %d\n", (t+1), volumeIndex);
                    sliceReady[t]=1;
                    lastRead = d;
                }
                else ready=0;
             }
             else ready=0;
          }
          else ready=0;
      }
   }
   return ready;
}

int dicomConverter::convert2Nii(string outputdir)
{
   char outputname[1024];

   fprintf(stderr, "First Slice = %d\n", firstSliceIndex);
   fprintf(stderr, "Volume %d written.\n", volumeIndex);
   sprintf(outputname, "%s/vol_%.5d", outputdir.c_str(), volumeIndex);
   nii_saveNII(outputname, hdr, imgM, opts);
   firstSliceIndex+=nslices;
   volumeIndex++;
   resetFlag();
}

void dicomConverter::reset()
{
   imgsz = 0;
   free(imgM);
   free(img);
   free(sliceReady);
   nslices = 0;
}

