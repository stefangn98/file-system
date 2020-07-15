/* filesys.c
 * 
 * provides interface to virtual disk
 * 
 */

/*
   Some helper functions were written with help from 
   StackOverflow and other sources
*/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "filesys.h"

diskblock_t virtualDisk[MAXBLOCKS]; // define our in-memory virtual, with MAXBLOCKS blocks
fatentry_t FAT[MAXBLOCKS];          // define a file allocation table with MAXBLOCKS 16-bit entries
fatentry_t rootDirIndex = 0;        // rootDir will be set by format
direntry_t *currentDir = NULL;
fatentry_t currentDirIndex = 0;

/* writedisk : writes virtual disk out to physical disk
 * 
 * in: file name of stored virtual disk
 */

void writedisk(const char *filename)
{
   printf("writedisk> virtualdisk[0] = %s\n", virtualDisk[0].data);
   FILE *dest = fopen(filename, "w");
   if (fwrite(virtualDisk, sizeof(virtualDisk), 1, dest) < 0)
      fprintf(stderr, "write virtual disk to disk failed\n");
   write(dest, virtualDisk, sizeof(virtualDisk));
   fclose(dest);
}

void readdisk(const char *filename)
{
   FILE *dest = fopen(filename, "r");
   if (fread(virtualDisk, sizeof(virtualDisk), 1, dest) < 0)
      fprintf(stderr, "write virtual disk to disk failed\n");
   //write( dest, virtualDisk, sizeof(virtualDisk) ) ;
   fclose(dest);
}

/* the basic interface to the virtual disk
 * this moves memory around
 */

void writeblock(diskblock_t *block, int block_address)
{
   //printf ( "writeblock> block %d = %s\n", block_address, block->data ) ;
   memmove(virtualDisk[block_address].data, block->data, BLOCKSIZE);
   //printf ( "writeblock> virtualdisk[%d] = %s / %d\n", block_address, virtualDisk[block_address].data, (int)virtualDisk[block_address].data ) ;
}

// /* read and write FAT
//  *
//  * please note: a FAT entry is a short, this is a 16-bit word, or 2 bytes
//  *              our blocksize for the virtual disk is 1024, therefore
//  *              we can store 512 FAT entries in one block
//  *
//  *              how many disk blocks do we need to store the complete FAT:
//  *              - our virtual disk has MAXBLOCKS blocks, which is currently 1024
//  *                each block is 1024 bytes long
//  *              - our FAT has MAXBLOCKS entries, which is currently 1024
//  *                each FAT entry is a fatentry_t, which is currently 2 bytes
//  *              - we need (MAXBLOCKS /(BLOCKSIZE / sizeof(fatentry_t))) blocks to store the
//  *                FAT
//  *              - each block can hold (BLOCKSIZE / sizeof(fatentry_t)) fat entries
//  */

// /* implement format()
//  */
void format()
{
   // Preparing block 0
   diskblock_t block;
   for (int i = 0; i < BLOCKSIZE; i++)
   {
      block.data[i] = '\0';
   }
   strcpy(block.data, "CS3026 Operating Systems Assessment");
   writeblock(&block, 0);

   // Preparing the FAT table
   FAT[0] = ENDOFCHAIN;
   FAT[1] = 2;
   FAT[2] = ENDOFCHAIN;
   FAT[3] = ENDOFCHAIN;

   for (int i = 4; i < MAXBLOCKS; i++)
   {
      FAT[i] = UNUSED;
   }
   // Copying the FAT table onto the disk
   copyFAT();

   // Creating a root directory block and entry properties
   diskblock_t rootDirBlock;
   direntry_t emptyDir;
   memset(rootDirBlock.data, '\0', BLOCKSIZE);
   memset(&emptyDir, '\0', sizeof(direntry_t));
   rootDirBlock.dir.isdir = TRUE;      // TRUE  = 1 as per def. in header
   rootDirBlock.dir.nextEntry = FALSE; // FALSE = 0 as per def. in header
   emptyDir.isdir = FALSE;
   emptyDir.unused = TRUE;
   for (int i = 0; i < DIRENTRYCOUNT; i++)
   {
      rootDirBlock.dir.entrylist[i] = emptyDir;
      rootDirBlock.dir.nextEntry = i;
   }

   rootDirIndex = (MAXBLOCKS / FATENTRYCOUNT) + 1;
   currentDirIndex = rootDirIndex;
   writeblock(&rootDirBlock, rootDirIndex);
}

void copyFAT()
{
   int fatentry = 0;
   int fatblocksneeded = (MAXBLOCKS / FATENTRYCOUNT);
   diskblock_t block;
   for (int i = 1; i <= fatblocksneeded; i++)
   {
      for (int j = 0; j < FATENTRYCOUNT; j++)
      {
         block.fat[j] = FAT[fatentry];
         fatentry++;
      }
      writeblock(&block, i);
   }
}

MyFILE *myfopen(const char *filename, const char *mode)
{
   // if (!(strcmp(mode, "r") == 0 || strcmp(mode, "w") == 0))
   // {
   //    printf("(!) Wrong file access mode. Closing!\n");
   //    return;
   // }

   // TODO: If I get to making new directories and files in them,
   // Use the string tokenization example from practical 10

   // Buffers
   // We could use malloc() here but it's easier to use calloc
   // since the memory allocated will be initialized to 0
   char *buffer = calloc(MAXPATHLENGTH, sizeof(char));
   diskblock_t block;
   memset(block.data, '\0', BLOCKSIZE);
   // Copying the name of the file to the buffer so that later on
   // it can appear in our virtualdisk
   strcpy(buffer, filename);

   // Create a file
   MyFILE *myFile = malloc(sizeof(MyFILE));
   myFile->pos = 0;
   myFile->buffer = block;
   strcpy(myFile->mode, mode);

   // If we want to read from a file
   if (strcmp(mode, "r") == 0)
   {
      // Check if the file exists and exit if it doesn't
      if (getLocation(filename) == FALSE)
      {
         printf("No such file exists. Exiting!\n");
         return;
      }
      else
      { // Or return the block which contains it if it does exist
         myFile->blockno = getLocation(filename);
         return myFile;
      }
   }

   // Here we're handling the case in which the file is opened
   // with the "w" (write) flag
   // If the file exists
   if (getLocation(filename) != FALSE)
   {
      // We get the block in which the file resides
      int pathToFile = getLocation(filename);
      // We empty out the block (because we are overwriting it)
      clearFAT(pathToFile);
      // Updating the FAT table
      copyFAT();

      // Creating a new buffer
      diskblock_t block;
      fatentry_t nextDir = rootDirIndex;

      while (nextDir != ENDOFCHAIN)
      {
         currentDirIndex = nextDir;
         // Not using writeblock() because it's writing in the oposite direction
         memmove(&block.data, virtualDisk[nextDir].data, BLOCKSIZE);
         currentDir = block.dir.entrylist;
         for (int i = 0; i < block.dir.nextEntry; i++)
         {
            if (currentDir[i].name == filename)
            {
               currentDir[i].firstblock = firstUnusedBlock();
               currentDir[i].entrylength = sizeof(currentDir);
               break;
            }
            nextDir = FAT[nextDir];
         }
      }
      int location = firstUnusedDir();
      currentDir[location].unused = FALSE;
   }
   else
   { // If the file doesn't exist, we create a new one
      // Create a buffer for our file
      diskblock_t block;
      int unusedBlock = firstUnusedBlock();
      // Filling the buffer with currentDir data (should be empty)
      // We aren't using writeblock() because again the operation is backwards
      memmove(&block.data, virtualDisk[currentDirIndex].data, BLOCKSIZE);

      // Creating a new dirEntry
      direntry_t *newEntry = calloc(1, sizeof(direntry_t));
      newEntry->isdir = FALSE;  // Because the entry is a file, not a directory
      newEntry->unused = FALSE; // Because it is used
      newEntry->firstblock = unusedBlock;
      // Setting the next block to be an end block
      FAT[unusedBlock] = ENDOFCHAIN;
      // Saving the table
      copyFAT();

      int entry = firstUnusedDir();
      currentDir = block.dir.entrylist;
      currentDir[entry] = *newEntry;
      strcpy(currentDir[entry].name, filename);

      writeblock(&block, currentDirIndex);
      myFile->blockno = unusedBlock;

      return myFile;
   }
}
void myfputc(MyFILE *myFile, int input)
{
   // Check if the file is open in read mode, if it is just exit out of the func
   if (myFile->mode == "r" || !myFile)
   {
      printf("Error when opening file. Exiting\n");
      return;
   }
   // Check if the length of the text is bigger than the blocksize(1024) if it is just moves to the next one
   if (myFile->pos >= BLOCKSIZE)
   {
      int firstBlock = firstUnusedBlock();
      FAT[myFile->blockno] = firstBlock;
      FAT[firstBlock] = ENDOFCHAIN;
      copyFAT();

      // Create an empty buffer
      diskblock_t block;
      // Moves the pointer of the string input to the beginning
      myFile->pos = 0;
      writeblock(&myFile->buffer, myFile->blockno);
      myFile->buffer = block;
      myFile->blockno = firstBlock;
   }
   myFile->buffer.data[myFile->pos] = input;
   myFile->pos++;
}

int myfgetc(MyFILE *myFile)
{
   // Getting the block in which the file resides
   int blockNo = myFile->blockno;

   if (blockNo == ENDOFCHAIN)
   {
      return EOF;
   }

   // Since each block is of size 1024, once the file pos reaches 1024 it
   // has to be reset because it moves on to the next block
   // and it shouldn't continue increasing above 1024
   if (BLOCKSIZE - myFile->pos == 0)
   {
      // if (FAT[blockNo] == ENDOFCHAIN)
      // {
      //    return EOF;
      // }
      myFile->blockno = FAT[blockNo];
      // Copying the data from the virtual disk block to the buffer we're reading from
      // Without this line, nothing will print to the console once we've ran the algorithm
      memcpy(&myFile->buffer, &virtualDisk[blockNo], BLOCKSIZE);
      // Moving onto the next block and resetting the pos pointer
      myFile->pos = myFile->pos - BLOCKSIZE; // This value should always be 0 but just in case we're setting it like this
   }
   char c = myFile->buffer.data[myFile->pos];
   myFile->pos++;
   return c;
}

void myfclose(MyFILE *myFile)
{
   printf("Closing file\n");
   // If there's no such file opened, we exit out of the function
   // So there's an issue with this method
   // If I pass it a file which has been initialized using myfopen()
   // I get a seg fault, but if I just declare a file (MyFILE *test; for example)
   // it works just fine.
   if (!myFile)
   {
      printf("Error when closing file. No such file. Exiting\n");
      return;
   }
   writeblock(&myFile->buffer, myFile->blockno);
   free(myFile);
}

void mymkdir(const char *path)
{
   // As always creating a buffer
   diskblock_t block;
   // Using calloc because it's more convenient than using malloc
   char *buffer = calloc(sizeof(path), sizeof(char));
   char *remainder = calloc(MAXPATHLENGTH, sizeof(char));

   if (path[0] == DELIMETER) // DELIMETER is defined in the head as "/"
   {
      // Used to set to root dir if specified by the path
      currentDirIndex = rootDirIndex;
   }
   // Moving the path to the buffer
   strcpy(buffer, path);
   // For each token in the path
   // For example if we specify /myfirstdir/myseconddir/
   // it will loop over myfirstdir first and then myseconddir
   // strtok_r() looks for the delimeter and splits the string around it
   char *token = strtok_r(buffer, DELIMETER, &remainder);
   while (token)
   {
      int location = dirBlockNumber(token);
      // If we have a path
      if (*remainder)
      {
         if (location == EOF)
         {
            return;
         }
         currentDirIndex = virtualDisk[currentDirIndex].dir.entrylist[location].firstblock;
      }
      else
      {
         if (location != EOF)
         {
            return;
         }
         int newLoc = firstUnusedDir();          // Getting the first unused directory
         strcpy(currentDir[newLoc].name, token); // Setting the name of it to the token
         // Setting properties to create a directory
         currentDir[newLoc].isdir = TRUE;
         currentDir[newLoc].unused = FALSE;
         currentDir[newLoc].firstblock = firstUnusedBlock();

         // Creating a new empty block
         diskblock_t emptyBlock;
         // Creating the empty entries to be put into the block
         direntry_t emptyEntry;
         // "Emptying" the block and the entries
         memset(emptyBlock.data, '\0', BLOCKSIZE);
         memset(&emptyEntry, '\0', sizeof(direntry_t));
         // Setting the block as dir block
         emptyBlock.dir.nextEntry = FALSE;
         emptyBlock.dir.isdir = TRUE;
         // Setting the entry properties
         emptyEntry.isdir = FALSE;
         emptyEntry.unused = TRUE;

         // Iterating over all dir entries in the empty block
         // and setting them to the empty entry
         for (int i = 0; i < DIRENTRYCOUNT; i++)
         {
            emptyBlock.dir.entrylist[i] = emptyEntry;
            emptyBlock.dir.nextEntry = i;
         }
         writeblock(&emptyBlock, currentDir[newLoc].firstblock);

         // Finalizing the FAT table
         FAT[currentDir[newLoc].firstblock] = ENDOFCHAIN;
         copyFAT();
      }
      token = strtok_r(NULL, DELIMETER, &remainder); // We move onto the next token
   }
}

// I tried to write this func the same way as mymkdir
// By making a buffer and looping over directories
// using the dirBlockNumber() function
// but I constantly get seg faults
// so I had to figure out a different way to do it
char **mylistdir(const char *path)
{
   // Setting root directory (if specified)
   if (path[0] == '/')
      currentDirIndex = rootDirIndex;

   char *pathCopy = calloc(sizeof(path), sizeof(int));
   char *remainder = calloc(MAXNAME, sizeof(char));
   // copy string path to b
   strcpy(pathCopy, path);
   char **output = malloc(12 * sizeof(char *));
   int index = 0;

   // Explanation in myfopen()
   char *token = strtok_r(pathCopy, DELIMETER, &remainder);
   while (token)
   {
      // Getting the block number of the token. It's always either a
      // directory or EOF in which case we exit out of the function
      int location = dirBlockNumber(token);
      // if it does not exist
      if (location == EOF)
         return;

      // Moving the directory index to the index of the token
      // Since the method dirBlockNumber always (if it exists) returns the block num
      // of the directory with a given name (token in this case)
      // We don't have to wrap the following line of code in an if statement (if entrylist[location].isdir)
      currentDirIndex = virtualDisk[currentDirIndex].dir.entrylist[location].firstblock;
      // Using NULL in this case means strtok moves on to the next token
      token = strtok_r(NULL, DELIMETER, &remainder);
   }

   // Creating buffers used when moving around the directories
   // to save contents
   diskblock_t buffer;
   fatentry_t next = currentDirIndex;

   while (next != ENDOFCHAIN)
   {
      // Moving the contents of the FAT block to the buffer
      memmove(&buffer.data, virtualDisk[next].data, BLOCKSIZE);
      // Changing the current working directory
      currentDir = buffer.dir.entrylist;
      for (int i = 0; i <= buffer.dir.nextEntry; i++)
      {
         if (currentDir[i].unused == FALSE)
         {
            // Setting aside memory for the entry in the output list
            output[index] = calloc(sizeof(currentDir[i].name), sizeof(char));
            // Copying the name in the previously allocated part of memory
            strcpy(output[index], currentDir[i].name);
            index++;
         }
      }
      // Moving to the next dir
      next = FAT[next];
   }
   // Setting the final entry as an end of string symbol
   output[index] = '\0';

   return output;
}

/* use this for testing
 */

void printBlock(int blockIndex)
{
   for (int i = 0; i < blockIndex; i++)
   {
      // printf("virtualdisk[%d] = %s\n", i, virtualDisk[i].data);
      printf("fat[%d] = %d\n", i, FAT[i]);
   }
}

// Helpers
// Getting the location of a file with a given name
// -> FALSE if file doesn't exist
// -> block where the file is located if file exists
int getLocation(const char *filename)
{
   diskblock_t block;
   fatentry_t next = currentDirIndex;

   while (next != ENDOFCHAIN)
   {
      currentDirIndex = next;
      // Using memmove here because it's like a reverse move of what writeblock() does
      // Essentially we are moving the data from the block on to our buffer
      memmove(&block.data, virtualDisk[next].data, BLOCKSIZE);
      currentDir = block.dir.entrylist;
      // Going throught all dir entries in the buffer
      for (int i = 0; i < block.dir.nextEntry; i++)
      {
         // Checking if the dir entry is the same as our filename
         // if it is, we return the block where the dir is located
         if (strcmp(currentDir[i].name, filename) == 0)
         {
            return currentDir[i].firstblock;
         }
      }

      next = FAT[next];
   }
   // There is no such file in our virtual disk
   return FALSE;
}

void clearFAT(int entry)
{
   // This is a method that's used when we're overwriting a file
   // To set the current files' blocks in the FAT table to unused (i.e to delete the file)
   diskblock_t block;
   memset(block.data, '\0', BLOCKSIZE);
   int nextEntry = FAT[entry];
   if (nextEntry != ENDOFCHAIN)
   {
      clearFAT(nextEntry);
   }
   writeblock(&block, entry);
   FAT[entry] = UNUSED;
}

int firstUnusedBlock()
{ // This function returns the first free block in the FAT
   // We're starting from 4 because the first 4(0,1,2 and 3) blocks are reserved
   for (int i = 4; i < MAXBLOCKS; i++)
   {
      if (FAT[i] == UNUSED)
      {
         return i;
      }
   }
   // If we don't find an unused block we just exit
   return;
}

int firstUnusedDir()
{
   diskblock_t block;
   fatentry_t nextDir = currentDirIndex;

   // Going through all dir blocks
   while (nextDir != ENDOFCHAIN)
   {
      // Moving the current dir to the buffer
      memmove(&block.data, virtualDisk[nextDir].data, BLOCKSIZE);
      for (int i = 0; i < block.dir.nextEntry; i++)
      {
         if (block.dir.entrylist[i].unused == TRUE)
         {
            currentDirIndex = nextDir;
            currentDir = virtualDisk[currentDirIndex].dir.entrylist;
            return i;
         }
      }
      // If we don't have an empty dir entry in the current dir, check the next one
      nextDir = FAT[nextDir];
   }
   // Finding the first (i.e the next) unused block and setting some params
   int firstUnused = firstUnusedBlock();
   FAT[currentDirIndex] = firstUnused;
   FAT[firstUnused] = ENDOFCHAIN;
   // Saving the FAT table
   copyFAT();

   // The following 11 lines are used to clean up the dir buffer "block"
   direntry_t emptyEntry;
   memset(block.data, '\0', BLOCKSIZE);
   block.dir.nextEntry = FALSE;
   block.dir.isdir = TRUE;
   memset(&emptyEntry, '\0', sizeof(direntry_t));
   emptyEntry.isdir = FALSE;
   emptyEntry.unused = TRUE;
   for (int i = 0; i < DIRENTRYCOUNT; i++)
   {
      block.dir.entrylist[i] = emptyEntry;
      block.dir.nextEntry = i;
   }
   // Setting the current dir index to be the first found unused dir
   currentDirIndex = firstUnused;
   writeblock(&block, currentDirIndex);
   currentDir = virtualDisk[currentDirIndex].dir.entrylist;
   return 0;
}

// Function to return the block number of a dir
// In our case it's used only when a path is tokenized
// to access the block number of a token
int dirBlockNumber(const char *path)
{
   diskblock_t block;
   fatentry_t nextDir = currentDirIndex;
   // Looping while we haven't reached an end of chain
   while (nextDir != ENDOFCHAIN)
   {
      // Again not using the method writeblock() because
      // we're moving data from the disk to a buffer block
      // instead of the other way around
      memmove(&block.data, virtualDisk[currentDirIndex].data, BLOCKSIZE);
      // Looping over each directory
      for (int i = 0; i < block.dir.nextEntry; i++)
      {
         // Exit condition if the name of the directory is the same as the specified token
         if (strcmp(block.dir.entrylist[i].name, path) == 0)
         {
            currentDirIndex = nextDir;
            currentDir = block.dir.entrylist;
            // return block number
            return i;
         }
      }
      // Incrementing nextDir
      nextDir = FAT[nextDir];
   }
   // If no such directory is found we return end of file
   return EOF;
}
