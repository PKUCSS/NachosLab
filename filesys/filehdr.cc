// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include <time.h>
//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space
    FileHeader* hdr = this;
    FileHeader* new_hdr;
    int cur_sector,next_sector;
    int i ;
    for ( i = 0; i < numSectors; i++){
        if (i != 0 && i % NumDirect == 0){ 
            new_hdr = new FileHeader;
            next_sector = freeMap->Find();	// find a sector to hold the new file header
            hdr->nextFdrSector = next_sector;
            if (i != NumDirect) hdr->WriteBack(cur_sector);
            cur_sector = next_sector;
            hdr = new_hdr;
        }
        hdr->dataSectors[i%NumDirect] = freeMap->Find();
    }
    hdr->nextFdrSector = -1;
    if (i >= NumDirect) hdr->WriteBack(cur_sector);
    time_t currentTime = time(NULL);
    createTime = currentTime;
    lastWriteTime = createTime;
    lastWriteTime = currentTime;
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    FileHeader* hdr = this;
    while(true)
        for (int i = 0; i < numSectors; i++) {
        ASSERT(freeMap->Test((int) hdr->dataSectors[i%NumDirect]));  // ought to be marked!
        freeMap->Clear((int) hdr->dataSectors[i% NumDirect]);
        if (hdr->nextFdrSector == -1) break;
        FileHeader* tmp_hdr = new FileHeader;
        tmp_hdr->FetchFrom(hdr->nextFdrSector);
        hdr = tmp_hdr;
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    FileHeader* tmp_hdr = this;
    // printf("%d\n",tmp_hdr->nextFdrSector);
    while (offset >= NumDirect*SectorSize){
       // printf("### %d\n",tmp_hdr->nextFdrSector);
        FileHeader* Tmp_hdr = new FileHeader;
        Tmp_hdr->FetchFrom(tmp_hdr->nextFdrSector);
        offset -= NumDirect*SectorSize;
        tmp_hdr = Tmp_hdr;
    }
    offset %= NumDirect*SectorSize;
    //printf("Byte To Sector Return: %d",tmp_hdr->dataSectors[offset / SectorSize]);
    return(tmp_hdr->dataSectors[offset / SectorSize]);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{

    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    FileHeader* hdr = this;
    for (i = 0; i < numSectors; i++){
        if (i !=0 && i % NumDirect ==0 ){ 
            FileHeader* tmp_hdr = new FileHeader;
            tmp_hdr->FetchFrom(hdr->nextFdrSector);
            hdr = tmp_hdr;
        }
        printf("%d ", hdr->dataSectors[i%NumDirect]);
    }
	
    printf("\nCreate Time:%sLast Access Time:%sLast Write Time:%s\n",asctime(localtime(&createTime)),asctime(localtime(&lastAccessTime)),asctime(localtime(&lastWriteTime)));
    printf("\nFile contents:\n");
    hdr = this;
    for (i = k = 0; i < numSectors; i++) {
        if (i !=0 && i % NumDirect ==0 ){ 
            FileHeader* tmp_hdr = new FileHeader;
            tmp_hdr->FetchFrom(hdr->nextFdrSector);
            hdr = tmp_hdr;
        }
	    synchDisk->ReadSector(hdr->dataSectors[i%NumDirect], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}
