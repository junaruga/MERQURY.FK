/********************************************************************************************
 *
 *  Command line utility to produce CN-spectra plots
 *
 *  Author:  Gene Myers
 *  Date  :  March, 2021
 *
 *********************************************************************************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <math.h>

#include "libfastk.h"

static char *Usage = " [-v] [-T<int(4)>] <mat>[.ktab] <pat>[.ktab] <child>[.ktab]";

static int check_table(char *name, int lmer)
{ int   kmer;
  FILE *f;

  f = fopen(name,"r");
  if (f == NULL)
    { fprintf(stderr,"%s: Cannot find FastK table %s\n",Prog_Name,name);
      exit (1);
    }
  else
    { fread(&kmer,sizeof(int),1,f);
      if (lmer != 0 && kmer != lmer)
        { fprintf(stderr,"%s: Kmer (%d) of table %s != %d\n",Prog_Name,kmer,name,lmer);
          exit (1);
        }
      fclose(f);
      return (kmer);
    }
}

static int reliable_cutoff(char *input)
{ Histogram *hist;
  int        thresh;

  //  Load read histogram and from it get KMER size and infer SOLID threshold

  hist = Load_Histogram(input);
  if (input == NULL)
    { fprintf(stderr,"\n%s: Cannot find FastK histograam %s.hist\n",Prog_Name,input);
      exit (1);
    }

  Modify_Histogram(hist,hist->low,hist->high,1);

  { int    low  = hist->low;
    int    high = hist->high;
    int64 *cvec = hist->hist;
    int    k;

    for (k = low+1; cvec[k] < cvec[k-1]; k++)
      if (k >= high)
        break;
    thresh = k;
  }

  Free_Histogram(hist);

  return (thresh);
}

int main(int argc, char *argv[])
{ int    KMER;
  char  *MAT;
  char  *PAT;
  char  *CHILD;
  int    VERBOSE;
  int    NTHREADS;

  int   MSOLID;
  int   PSOLID;
  
  { int    i, j, k;
    int    flags[128];
    char  *eptr;

    ARG_INIT("HAPmaker");

    NTHREADS = 4;

    j = 1;
    for (i = 1; i < argc; i++)
      if (argv[i][0] == '-')
        switch (argv[i][1])
        { default:
            ARG_FLAGS("v")
            break;
          case 'T':
            ARG_POSITIVE(NTHREADS,"Number of threads")
            break;
        }
      else
        argv[j++] = argv[i];
    argc = j;

    VERBOSE = flags['v'];

    if (argc != 4)
      { fprintf(stderr,"\nUsage: %s %s\n",Prog_Name,Usage);
        fprintf(stderr,"\n");
        fprintf(stderr,"      -v: verbose output to stderr\n");
	fprintf(stderr,"      -T: number of threads to use\n");
        exit (1);
      }
  }

  MAT = Root(argv[1],".ktab");
  PAT = Root(argv[2],".ktab");
  CHILD = Root(argv[3],".ktab");

  { char  command[5000];

    KMER = check_table(Catenate(MAT,".ktab","",""),0);
    KMER = check_table(Catenate(PAT,".ktab","",""),KMER);
    KMER = check_table(Catenate(CHILD,".ktab","",""),KMER);

    //  Form the inherited k-mer tables

    if (VERBOSE)
      fprintf(stderr,"\n  Computing tables of specific k-mers\n");

    sprintf(command,"Logex -T%d -h '%s.S = A-B' '%s.S = B-A' %s %s",
                    NTHREADS,MAT,PAT,MAT,PAT);
    system(command);

    MSOLID = reliable_cutoff(Catenate(MAT,".S",".hist",""));
    PSOLID = reliable_cutoff(Catenate(PAT,".S",".hist",""));

    if (VERBOSE)
      fprintf(stderr,"\n  Specific k-mers reliability cutoffs are %d(mat) & %d(pat)\n",
                     MSOLID,PSOLID);

    if (VERBOSE)
      fprintf(stderr,"\n  Computing tables of specific, inherited k-mers\n");

    sprintf(command,"Logex -T%d -h '%s.I = C&.A[%d-]' '%s.I = C&.B[%d-]' %s.S %s.S %s",
                    NTHREADS,MAT,MSOLID,PAT,PSOLID,MAT,PAT,CHILD);
    system(command);

    MSOLID = reliable_cutoff(Catenate(MAT,".I",".hist",""));
    PSOLID = reliable_cutoff(Catenate(PAT,".I",".hist",""));

    if (VERBOSE)
      fprintf(stderr,"\n  Inherited k-mers reliability cutoffs are %d(mat) & %d(pat)\n",
                     MSOLID,PSOLID);

    if (VERBOSE)
      fprintf(stderr,"\n  Computing final tables of hap-mers\n");

    sprintf(command,"Logex -T%d '%s.hap = A[%d-]' '%s.hap = B[%d-]' %s.I %s.I",
                    NTHREADS,MAT,MSOLID,PAT,PSOLID,MAT,PAT);
    system(command);

    sprintf(command,"Fastrm %s.S %s.S %s.I %s.I",MAT,PAT,MAT,PAT);
    system(command);
  }

  free(CHILD);
  free(PAT);
  free(MAT);

  Catenate(NULL,NULL,NULL,NULL);
  Numbered_Suffix(NULL,0,NULL);
  free(Prog_Name);

  exit (0);
}
