import os;
import sys;
from shutil import copyfile;
from time import sleep, time;

TR=2.0;
nSlices = 40.0;

current_milli_time = lambda: int(round(time() * 1000))

def sortFiles(fileList):
    sortedFileList = [];
    auxDict = {};
    for filename in fileList:
        tokens = filename.split('.');   
        if 'MRDC' in filename:
           sortedFileList[int(tokens[-1])] = filename;
        else:
           # Flyview mode
           index = '%s%.3d' % (tokens[-3], int(tokens[-2]));
           auxDict[index] = filename;
    
    keys = list(auxDict.keys());
    sortedkeys = sorted(keys);
    for i in range(len(sortedkeys)):
        sortedFileList.append(auxDict[sortedkeys[i]]);

    return sortedFileList;
 
def copyFiles(fileList, dirInput, dirOutput):
    if not os.path.exists(dirOutput):   
       os.makedirs(dirOutput);
    
    for t in range(len(fileList)):
       filename = fileList[t];
       filein  = '%s/%s' % (dirInput, filename);
       fileout = '%s/i%d.MRDC.%d' % (dirOutput, current_milli_time(), t+1);
       copyfile(filein, fileout);
       sleep(TR/nSlices);

if len(sys.argv) > 2:
   dirInput = sys.argv[1];
   dirOutput = sys.argv[2];
   fileList = os.listdir(dirInput);
   sortedFileList = sortFiles(fileList);
   copyFiles(sortedFileList, dirInput, dirOutput); 
