#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesys.h"

void D3_D1()
{
    printf("(!) Execucting D3_D1 functionality\n");
    format();
    writedisk("virtualdiskD3_D1");
}

void C3_C1()
{
    printf("(!) Execucting C3_C1 functionality\n");
    format();
    MyFILE *myFile = myfopen("testfile.txt", "w");

    char *input[4 * BLOCKSIZE];
    char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (int i = 0; i < 4 * BLOCKSIZE; i++)
    {
        input[i] = alphabet[i % strlen(alphabet)];
    }

    for (int i = 0; i < 4 * BLOCKSIZE; i++)
    {
        // Here I don't know why input[i] doesn't work and I tried multiple different
        // ways but the only one that seems to work is dividing it by the length of the whole input
        myfputc(myFile, alphabet[i % strlen(alphabet)]);
    }
    myfclose(myFile);

    FILE *sysFile = fopen("testfileC3_C1_copy.txt", "w");
    MyFILE *myFileRead = myfopen("testfile.txt", "r");

    for (int i = 0; i < 4096; i++)
    {
        char c = myfgetc(myFileRead);
        if (c == EOF)
        {
            break;
        }
        if (i % 64 == 0)
        {
            printf("%c|\n", c);
        }
        else
        {
            printf("%c", c);
        }

        // printf("%d -> %c\n", i, c);
        fprintf(sysFile, "%c", c);
    }
    printf("\n");
    myfclose(myFileRead);
    fclose(sysFile);
    writedisk("virtualdiskC3_C1");
}

void B3_B1()
{
    printf("(!) Execucting B3_B1 functionality\n");
    format();
    char **result = malloc(sizeof(char **));
    int i = 0;
    mymkdir("/myfirstdir");
    mymkdir("/myfirstdir/myseconddir");
    mymkdir("myseconddir/mythirddir");
    result = mylistdir("/myfirstdir/myseconddir");
    while (result[i] != '\0')
    {
        printf("%d -> %s\n", i, result[i]);
        i++;
    }
    writedisk("virtualdiskB3_B1_a");

    /*
    * I couldn't get myfopen() to work with paths
    * It just works when creating a file like in part C3_C1()
    */

    // const char *input = "I LOVE seg faults!";
    // MyFILE *myFile = myfopen("/myfirstdir/myseconddir/testfile.txt", "w");
    // for (int i = 0; i < sizeof(input) * 10; i++)
    // {
    //     myfputc(myFile, input[i % strlen(input)]);
    // }
    // myfclose(myFile);
    // i = 0;
    // result = mylistdir("/myfirstdir/myseconddir");
    // while (result[i] != NULL)
    // {
    //     printf("%d -> %s\n", i, result[i]);
    //     i++;
    // }
    // writedisk("virtualdiskB3_B1_b");
}

int main()
{
    // D3_D1();
    // C3_C1();
    B3_B1();
    return 0;
}