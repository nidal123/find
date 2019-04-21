//nfind.c by Nidal Hishmeh

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

// Write message to stderr using format FORMAT
#define WARN(format,...) fprintf (stderr, "nfind: " format "\n", __VA_ARGS__)

// Write message to stderr using format FORMAT and exit.
#define DIE(format,...)  WARN(format,__VA_ARGS__), exit (EXIT_FAILURE)

int depth=0; //set to 1 if -depth appears
long maxdepth=-1; //set to whatever -maxdepth's value is if the flag appears
int realoption=0; //flag for the "real" options -P or -L. realoption=1 means -L, otheriwise -P by default


const char *filenames[200] = {NULL};//200 is max number of filenames possible in this case
int filenamescount=0;

const char *namenewer[200] = {NULL}; //used to store whatever comes after -name or newer (index should correspond to expressiontrack)
const char *exec[200][200] = {{NULL}};//matrix used to store things following exec flags
int exectrackstore[200] = {-1};

int expression[500] = {-1};
int expressiontrack=0;

int actionflag=0; //-print/exec either appears in expression or does not. If it does, print when flag appears. Else, print if expression evaluates to true.
//set printflag=1 if -print appears in expression

bool isNumber(char number[]);
void printdir(char *dir, int d, ino_t inodenumbers[], int nodedevtrack, dev_t devnumbers[]);
bool evaluate(char *fdir);
void printexp();
bool assess (char *d, int index);

int main (int argc, char *argv[])
{
    //parse through command line
    int reading=0;//0 means parsing through real options, 1 means parsing through filenames, 2 means parsing through expression componenets
    int nonoptionappeared=0;//set to 1 in order to give warning when -depth or maxdepth appear after a non-option expression componenet
    
    for (int i=1; i<argc; i++){
        if (strcmp(argv[i], "-P")==0 && reading==0){
            realoption=0;
        }
        else if (strcmp(argv[i], "-L")==0 && reading==0){
            realoption=1;
        }
        else if (argv[i][0]!='-' && reading==1){//reading in filenames
            filenames[filenamescount] = argv[i];
            filenamescount = filenamescount+1;
        }
        else if (reading==0){//either start reading in filenames or expression depending on the nature of argv[i]
            if (argv[i][0]=='-'){//no listed filenames, skip straight to expression
                reading=2;
                i = i-1;//decrement i so that loop starts over at same argv[i] but this time reading=2 so it will know to process the given argv as an expression componenet
            }
            else{//argv[i] must be a filename, so set reading to 1 and decrement i so that we can go through the loop again with the knowledge that argv[i] is a filename (it will be processed in the else if directly above this.
                reading=1;
                i = i-1;
            }
        }
        else if (reading==1){//was reading filenames but agrv[i] now no longer appears to be a filename so the expression must have started
            reading=2;
            i = i-1;
        }
        else if (reading==2){//processing an expression
            
            if (strcmp(argv[i], "-o")==0){
                nonoptionappeared=1;
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 0;
                    expressiontrack+=1;
                    if (i==(argc-1))
                        DIE("%s with no right operand", argv[i]);
                }
                else
                    DIE("%s with no left operand", argv[i]);
            }
            else if (strcmp(argv[i], "-a")==0){
                nonoptionappeared=1;
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    if (i==(argc-1))
                        DIE("%s with no right operand", argv[i]);
                }
                else
                    DIE("%s with no left operand", argv[i]);
            }
            else if (strcmp(argv[i], "-depth")==0){
                depth=1;
                if (nonoptionappeared==1)
                    WARN("%s follows non-option", argv[i]);
                
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    expression[expressiontrack] = 2;
                    expressiontrack+=1;
                }
                else{
                    expression[expressiontrack] = 2;
                    expressiontrack+=1;
                }
            }
            else if (strcmp(argv[i], "-exec")==0){ //ask about {}?
                nonoptionappeared=1;
                actionflag=1;
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    expression[expressiontrack] = 3;
                    //look at stuff following -exec
                    int foundsemicolon=0;
                    int exectrack=0;
                    
                    while (!foundsemicolon && i<argc){
                        if (strcmp(argv[i], ";")==0){//ascii ; = 59
                            foundsemicolon=1;
                        }
                        else{
                            exec[expressiontrack][exectrack] = argv[i];
                            exectrack+=1;
                            exectrackstore[expressiontrack] = exectrack;
                            i+=1;
                        }
                    }
                    if (!foundsemicolon)
                        DIE("no ; following %s", argv[(argc-1)]);
                    if (!exectrack)
                        fprintf(stderr, "no command follows -exec");
                    
                    expressiontrack+=1;
                }
                else{
                    expression[expressiontrack] = 3;
                    //look at stuff following -exec (copied from above)
                    int foundsemicolon=0;
                    int exectrack=0;
                    while (!foundsemicolon && i<argc){
                        if (strcmp(argv[i], ";")==0){//ascii ; = 59
                            foundsemicolon=1;
                        }
                        else{
                            exec[expressiontrack][exectrack] = argv[i];
                            exectrack+=1;
                            exectrackstore[expressiontrack] = exectrack;
                            i+=1;
                        }
                    }
                    if (!foundsemicolon)
                        DIE("no ; following %s", argv[(argc-1)]);
                    if (!exectrack)
                        fprintf(stderr, "nfind: no command follows -exec");
                    
                    expressiontrack+=1;
                }
                
            }
            else if (strcmp(argv[i], "-maxdepth")==0){
                if (nonoptionappeared==1)
                    WARN("%s follows non-option", argv[i]);
                if (i==(argc-1))
                    DIE("nfind: missing argument to %s", argv[i]);
                i+=1;
                if (!isNumber(argv[i]))
                    DIE("invalid maxdepth: %s", argv[i]);
                
                char *ptr;
                maxdepth = strtoul(argv[i], &ptr, 10);
                
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    expression[expressiontrack] = 4;
                    expressiontrack+=1;
                }
                else{
                    expression[expressiontrack] = 4;
                    expressiontrack+=1;
                }
                
            }
            else if (strcmp(argv[i], "-name")==0){
                nonoptionappeared=1;
                if (i==(argc-1))
                    DIE("missing argument to %s", argv[i]);
                i+=1;
                //store string for -name --> did this with namenewer
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    expression[expressiontrack] = 5;
                    namenewer[expressiontrack] = argv[i];
                    expressiontrack+=1;
                }
                else{
                    expression[expressiontrack] = 5;
                    namenewer[expressiontrack] = argv[i];
                    expressiontrack+=1;
                }
            }
            else if (strcmp(argv[i], "-newer")==0){
                nonoptionappeared=1;
                if (i==(argc-1))
                    DIE("missing argument to %s", argv[i]);
                i+=1;
                //store string for -newer -->did this with namenewer
                
                struct stat filestat;
                if (stat(argv[i], &filestat) < 0)
                    DIE("stat(%s) failed", argv[i]);
                
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    expression[expressiontrack] = 6;
                    namenewer[expressiontrack] = argv[i];
                    expressiontrack+=1;
                }
                else{
                    expression[expressiontrack] = 6;
                    namenewer[expressiontrack] = argv[i];
                    expressiontrack+=1;
                }
                
            }
            else if (strcmp(argv[i], "-print")==0){
                nonoptionappeared=1;
                actionflag=1;
                
                if (expressiontrack%2==1){
                    expression[expressiontrack] = 1;
                    expressiontrack+=1;
                    expression[expressiontrack] = 7;
                    expressiontrack+=1;
                }
                else{
                    expression[expressiontrack] = 7;
                    expressiontrack+=1;
                }
            }
            else{//flag doesn't make sense
                DIE("invalid argument: %s", argv[i]);
            }
            
        }//end of *processing an expression* section
        
        else{//the argument could not be identified as something that belongs in the command line for nfind
            DIE("invalid argument: %s", argv[i]);
        }
    }
    
    //if no filenames were declared use getcwd to populate the first element in filenames?
    if (filenamescount==0){
        filenames[0] = ".";
        filenamescount+=1;
    }
    
    //printexp();
    
    
    //cycle through each filename
    for (int j=0; j<filenamescount; j++){
        struct stat buf;
        if (stat(filenames[j], &buf)<0){
            WARN ("stat(%s) Failed", filenames[j]);
            continue;
        }
        if (maxdepth==-1)
            maxdepth=400;//change this to a higher arbitrary value. it's set to 4 now to help debugging
        ino_t nodenums[500] = {-1};
        dev_t devnums[500] = {-1};
        printdir((char *)filenames[j], maxdepth, nodenums, 0, devnums);
        
    }
    
    
}

void printdir(char *dir, int d, ino_t inodenumbers[], int nodedevtrack, dev_t devnumbers[]){
    if (d>=0){
        DIR *dp;
        struct dirent *entry;
        
        struct stat buffer;
        if (realoption==1){//-L
            stat(dir, &buffer);
        }
        else{//-P
            lstat(dir , &buffer);
        }
        
        //process current file
        //fprintf(stdout, "dev: %d  inode: %llu\n", buffer.st_dev, buffer.st_ino);
        //check to see if there is a loop
        for (int i=0; i<nodedevtrack; i++){
            if ((buffer.st_dev == devnumbers[i]) && (buffer.st_ino == inodenumbers[i])){//loop detected
                WARN("Loop at %s", dir);
                return;
            }
        }
        inodenumbers[nodedevtrack] = buffer.st_ino;
        devnumbers[nodedevtrack] = buffer.st_dev;
        
        if (depth==0){
            if (actionflag==1){
                evaluate(dir);
            }
            else{
                if (evaluate(dir)==true)
                    printf("%s\n", dir);
            }
        }
        
        //skip opendir() and readdir() stuff if not a directory?
        if (S_ISDIR(buffer.st_mode)){
            //didnt indent below
            if ((dp = opendir(dir))==NULL){
                WARN("opendir(%s) failed", dir);
                return;
            }
            
            while((entry = readdir(dp))){
                
                if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0){//ignore . and ..
                    continue;
                }
                char tempname[600] = {'\0'};
                strcat(tempname, dir);
                strcat(tempname, "/");
                strcat(tempname, entry->d_name);
                printdir(tempname, (d-1), inodenumbers, (nodedevtrack+1), devnumbers);
            }
            closedir(dp);
        }
        
        if (depth==1){
            if (actionflag==1){
                evaluate(dir);
            }
            else{
                if (evaluate(dir)==true)
                    printf("%s\n", dir);
            }
        }
    }
    else
        return;
}

bool evaluate(char *fdir){//evaluate expression for a given file until the outcome is known
    struct stat statbuf;
    
    if (expressiontrack==0){//no expression so just print fdir
        //fprintf("%s\n", fdir);
        return true;
    }
    else if ((expressiontrack%2)!=1){
        fprintf(stderr, "Error evaluating expression (expression track ends on an even number). This implied that an operator is missing either its left or right side\n");
    }
    
    if (realoption==1){//-L
        stat(fdir, &statbuf);
    }
    else{//-P
        lstat(fdir , &statbuf);
    }
    
    bool exp = true;//set to whatever expression evaluates to
    for (int j=0; j<expressiontrack; j+=2){
        if (expression[j]==2){//-depth always returns true
            if ((expression[(j+1)]==0) || ((j+1)==expressiontrack)){
                exp = true;
                j = expressiontrack;//end loop becuase outcome is known
            }
        }
        else if (expression[j]==3){//-exec true if 0 status is returned from system() call
            //concatenate a string with string for system() call
            
            char dest[1000] = {'\0'};
            
            for (int f=1; f<exectrackstore[j]; f++){//this should construct the string to be executed called dest system(dest)
                if (f!=1)
                    strcat(dest, " ");
                if (strcmp(exec[j][f], "{}")==0){
                    strcat(dest, fdir);
                }
                else
                {
                    strcat(dest, exec[j][f]);
                }
            }
            int sysval = system(dest);//0 on success
            //fprintf(stdout, "sysval: %d\n", sysval);
            if (sysval==0){//true
                if ((expression[(j+1)]==0) || ((j+1)==expressiontrack)){
                    exp = true;
                    j = expressiontrack;
                }
            }
            else{//false
                if ((expression[(j+1)]==1) || ((j+1)==expressiontrack)){
                    exp = false;
                    //j = expressiontrack;
                    while (expression[(j+1)]==1){
                        j += 2;
                    }
                }
            }
            
        }
        else if (expression[j]==4){//-maxdepth always returns true
            if ((expression[(j+1)]==0) || ((j+1)==expressiontrack)){
                exp = true;
                j = expressiontrack;//end loop becuase outcome is known
            }
        }
        else if (expression[j]==5){//-name true if matches
            int t=0;
            int lastforwardslash=0;
            while (fdir[t]!='\0'){
                t++;
                if (fdir[t]=='/')
                    lastforwardslash=t;
            }
            //fprintf(stdout, "\n\npatter: xx%sxx\nstring: xx%sxx\n\n", namenewer[j], &fdir[lastforwardslash+1]);
            if (fnmatch(namenewer[j], &fdir[lastforwardslash+1], 0)==0){//true
                if ((expression[(j+1)]==0) || ((j+1)==expressiontrack)){
                    exp = true;
                    j = expressiontrack;
                }
            }
            else{//false
                if ((expression[(j+1)]==1) || ((j+1)==expressiontrack)){
                    exp = false;
                    //j = expressiontrack;
                    while (expression[(j+1)]==1){
                        j += 2;
                    }
                }
            }
        }
        else if (expression[j]==6){//-newer true if newer
            struct stat buf2;
            if (realoption==1){//-L
                stat(namenewer[j], &buf2);
            }
            else{//-P
                lstat(namenewer[j] , &buf2);
            }
            
            struct timespec tnewer = buf2.st_mtim;
            //fprintf(stdout, "%s:  %ld    %ld\n", namenewer[j], tnewer.tv_sec, tnewer.tv_nsec);
            struct timespec tfdir = statbuf.st_mtim;
            //fprintf(stdout, "%s:  %ld    %ld\n", fdir, tfdir.tv_sec, tfdir.tv_nsec);
            
            //true if tfdir is a later time than tnewer
            
            if (tfdir.tv_sec > tnewer.tv_sec){
                //fprintf(stdout, "\nAAA\n");
                if ((expression[(j+1)]==0) || ((j+1)==expressiontrack)){
                    exp = true;
                    j = expressiontrack;//end loop becuase outcome is known
                }
            }
            else if ((tfdir.tv_sec == tnewer.tv_sec) && (tfdir.tv_nsec > tnewer.tv_nsec)){
                //fprintf(stdout, "tfdir.tv_sec: %ld\ntnewer.tv_nsec: %ld\n", tfdir.tv_nsec, tnewer.tv_nsec);
                //if (tfdir.tv_nsec > tnewer.tv_nsec){
                //fprintf(stdout, "WTF\n");
                if ((expression[(j+1)]==0) || ((j+1)==expressiontrack)){
                    exp = true;
                    j = expressiontrack;//end loop becuase outcome is known
                }
                //}
            }
            else{//result of -newer is false
                if ((expression[(j+1)]==1) || ((j+1)==expressiontrack)){
                    exp = false;
                    //j = expressiontrack;
                    while (expression[(j+1)]==1){
                        j += 2;
                    }
                }
            }
            
        }
        else if (expression[j]==7){//-print always returns true according to man find it seems
            //the value t/f of exp shouldn't even matter if this flag exists so just make sure it prints don't worry about the boolean
            printf("%s\n", fdir);
        }
    }
    return exp;
}

void printexp(){
    fprintf(stdout, "******************************\n\n\n");
    fprintf(stdout, "filenames: ");
    for (int i=0; i<filenamescount; i++){
        fprintf(stdout, "%s ", filenames[i]);
    }
    fprintf(stdout, "\n\nexpression: ");
    for (int i=0; i<=expressiontrack; i++){
        fprintf(stdout, "%d ", expression[i]);
    }
    fprintf(stdout, "\n\n");
    
    fprintf(stdout, "namenewer: ");
    for (int i=0; i<expressiontrack; i++){
        fprintf(stdout, "%d%s ", i, namenewer[i]);
    }
    fprintf(stdout, "\n\n");
    
    fprintf(stdout, "exec: ");
    for (int j=0; j<expressiontrack; j++){
        char dest[1000] = {'\0'};
        
        for (int f=1; f<exectrackstore[j]; f++){//this should construct the string to be executed called dest
            if (f!=1)
                strcat(dest, " ");
            if (strcmp(exec[j][f], "{}")==0){
                strcat(dest, "fdir");
            }
            else
            {
                strcat(dest, exec[j][f]);
            }
        }
        fprintf(stdout, "%d%s ", j, dest);
    }
    fprintf(stdout, "\n\n");
    
    fprintf(stdout, "depth: %d\n", depth);
    fprintf(stdout, "\n\n");
    fprintf(stdout, "maxdepth: %ld\n\n", maxdepth);
    fprintf(stdout, "******************************\n\n\n");
}

bool isNumber(char number[]){
    for (int i=0; number[i]!=0; i++)
        if (!isdigit(number[i])){
            return false;
        }
    return true;
}
