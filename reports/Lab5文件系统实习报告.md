# 操作系统实习报告5——文件系统

2020.4

[TOC]

## 内容一 总体概述

本次Lab的内容是在阅读源代码的基础上，通过对Nachos文件系统代码的修改完善其文件系统。Nachos初始版本的文件系统比较简单，文件长度是固定的，只有一级根目录，而且支持粗粒度的同步互斥机制(同一时刻只有一个线程能访问文件系统)。通过本次Lab完成的修改，实现了扩展文件属性、多级目录、动态扩展文件长度、完善同步互斥机制等功能，并优化了模拟磁盘的性能，实现了模拟的管道机制，使Nachos的文件系统功能与真实系统更为接近，有助于加深对文件系统的理解。

## 内容二 完成情况

### 任务完成列表

完成全部要求(7个Exercise和2个Challenge)。

| 1    | 2    | 3    | 4    | 5    | 6    | 7    | 8    | 9    | C1   | C2   |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Y    | Y    | Y    | Y    | Y    | Y    | Y    | Y    | Y    | Y    | Y    |

### 第一部分 文件系统的基本操作

#### Exercise 1(源代码阅读)

> 阅读Nachos 源代码中与文件系统相关的代码，理解 Nachos 文件系统的工作原理。

- `code/filesys/filesys.h `和`code/filesys/filesys.cc`:实现了Nachos系统中的文件系统类`FileSystem`,代码中定义了两个文件系统，`FILESYS_STUB`宏定义的是基于宿主机Linux上的文件系统，调用LInux文件系统的功能暂时实现Nachos文件系统的功能，等到Nachos本身的文件系统 可用。另一个是Nachos本身的文件系统，成员变量有记录空闲磁盘块的文件`freeMapFile`和目录文件`directoryFile`(初始版本只有一个根目录，所有文件都在根目录下);成员方法是对文件系统的一些列操作函数，包括：
  - `FileSystem(bool format);`构造函数，接受一个bool类型的参数format，含义为磁盘是否需要初始化，若为真，则需要初始化磁盘，new出一个记录空闲磁盘块的`freemap`和目录`directory`,将它们分别写入文件；若为假，则只需要打开`FreeMap`和`Directory`对应的文件。
  - `bool Create(char *name, int initialSize); `创建文件，参数为文件名和字节数(初始版本中文件大小固定)。创建文件的步骤是：确定文件是否已经存在→为文件头分配一个磁盘扇区→为数据区分配磁盘扇区→将文件名加入目录中→将新的文件头写入磁盘→更新Bitmap和目录，写回磁盘。创建成功后返回True,如文件已存在或磁盘空间不足，则返回`False`. 
  - `OpenFile* Open(char *name); `根据文件名返回打开文件指针，步骤是通过目录查找文件头所在的扇区，再读取文件头，返回打开文件的指针。
  - `bool Remove(char *name);`根据文件名删除文件，在目录中查找文件名，如果文件名存在，查找到存放文件头的扇区，读取文件头后删除，根据文件头释放数据区占据的磁盘块，从目录中删去文件名，更新目录和freeMap，写回磁盘文件。
  - `void List();`列出根目录下所有文件名。
  - `void Print();  `打印文件系统信息(空闲区表bitmao、目录内容、每个文件的文件头和数据)。

- `code/filehdr/filehdr.h` 和 `code/filesys/filehdr.cc`:定义了Nachos系统中的文件头`FileHeader`类。该类成员变量有文件的字节数`numBytes`、文件数据占用的磁盘块的个数以及`numSectors`、记录每个磁盘块的数组`dataSectors`,成员方法是对文件头的一系列操作函数，包括：

  - `Allocate(BitMap *bitMap, int fileSize);`为文件分配空闲磁盘块，参数包括文件大小(字节数`fileSize`)和空闲磁盘块的位图指针`bitmap`,其操作是根据文件大小计算需要的磁盘块数目，若剩余的空闲磁盘块数目不足则返回`False`,否则调用bitmap的find方法逐一分配空闲磁盘块，返回true;
  - `Deallocate(BitMap *freeMap)`释放文件占据的磁盘块，更新维护空闲磁盘块的位图；
  - `FetchFrom`:从磁盘中读出本文件头；
  - `WriteBack`:将本文件头内容写回磁盘
  - `ByteToSector(int offset)`:返回距开头offset字节数据所在的磁盘块指针；
  - `FileLength`:返回文件大小(字节数)；`Print`：打印文件信息
  
- `directory.cc`和`directory.h`:定义了Nachos文件系统中目录类`Directory`,其形式是由<文件名,文件头所在磁盘块>配对组成的数组，成员方法有：

  - `Directory(int size)`构造函数，初始化目录表
  - `FetchFrom(OpenFile *file)`:从磁盘中读取目录
  - `WriteBack(OpenFile *file)`：将自身写回磁盘文件
  - `FindIndex(char *name)`:根据文件名寻找相应的页表项位置(数组下标)，如果没有对应返回-1
  - `Find(char *name)`：通过调用`FindIndex`根据文件名寻找相应的文件头所在扇区，如果没有对应返回-1
  - `Add(char *name, int newSector)`:向目录中添加文件，接受文件名和文件头所在磁盘块两个参数，若添加成功返回TRUE,若添加失败（目录已满或已有此文件名），则返回FALSE;
  - `Remove(char *name)`:从目录中删除文件，成功返回TRUE，失败(没有此文件)则返回FALSE;
  - `List()`打印本目录包含的所有文件名,`Print()`打印本目录包含的所有文件详细信息。
  
- `openfile.h` 和 `openfile.cc`:定义Nachos系统用于读写文件的数据结构`OpenFile`,其私有成员包括指向文件头的指针`hdr`和当前偏移量`seekPosition`(距离文件开头的字节数)，公有成员是一些读写文件的方法，包括：

  - `OpenFile(int sector)`构造函数，根据给定的磁盘块从中读取文件头，将`seekPosition`设为0；

  - `~OpenFile()`析构函数

  - `SeekPosition(int position)`:设定读写指针位置

  - `ReadAt(char *into, int numBytes, int position)`:从position位置开始读取numBytes个字节，写入into所指的位置。过程中要检查读取字节数是否不为正数或超过文件大小：

    ~~~cpp
        if ((numBytes <= 0) || (position >= fileLength))
        	return 0; 				// check request
        if ((position + numBytes) > fileLength)		
    	numBytes = fileLength - position;
    ~~~

    接着计算起始和结束的磁盘块，调用磁盘的ReadSector函数将磁盘内容读入缓冲区中，再复制到目标位置，返回成功读取的字节数。

  - `Read(char *into, int numBytes)`:封装ReadAt方法，从当前读写指针位置读指定字节数到目标位置，更新读写指针；

  - `WriteAt(char *from, int numBytes, int position)`:以from指向的位置为源，向position位置写入`numBytes`个字节的数据。过程中要检查写入的字节数是否不为正数或超过文件大小：

    ~~~cpp
        if ((numBytes <= 0) || (position >= fileLength))
    	return 0;				// check request
        if ((position + numBytes) > fileLength)
    	numBytes = fileLength - position;
    ~~~

    接着计算起始和结束的磁盘块，设立缓冲区，检查要写的第一个和最后一个磁盘块是否对齐，如果未对齐则读入缓冲区；再将from指向的内容复制到缓冲区，再调用磁盘的`WriteSector`方法逐个将缓冲区内容写入磁盘块，删除缓冲区，返回成功写入的字节数。

  - `Length()`：返回打开的文件长度(字节数)。

- `code/userprog/bitmap.h`和`code/userprog/bitmap.cc`:位图数据结构，在文件系统中用于记录磁盘块的使用情况。

#### Exercise 2 扩展文件属性

> 增加文件描述信息，如“类型”、“创建时间”、“上次访问时间”、“上次修改时间”、“路径”等等。尝试突破文件名长度的限制。

直接在目录项中添加属性，添加的属性有：

- 类型，设置为字符型，'F'表示文件，'D'表示目录
- 文件名改为字符指针型以突破长度限制
- 路径留待Exercise 4(多级目录)多级目录中实现

修改后的目录项定义：

~~~c++
class DirectoryEntry {
  public:
    bool inUse;				// Is this directory entry in use?
    int sector;				// Location on disk to find the 
					//   FileHeader for this file
    char type;	// Directory entry type:'F' for file,'D' for directory
    char *name;
    // char name[FileNameMaxLen + 1];	// Text name for file, with +1 for 
					// the trailing '\0'
};
~~~

考虑到在目录项中修改访问时间不方便，将三个时间属性记录在file header中，即:上次访问时间、上次修改时间、创建时间，用time_t类型记录。

~~~cpp
    time_t createTime;	// Create time of the file
    time_t lastAccessTime;	// Last access time of the file
    time_t lastWriteTime;	// Last write time of the file
~~~

修改Directory类的`Add`函数，设置新添加的属性(类型和文件名)：

~~~cpp
bool
Directory::Add(char *name, int newSector)
{ 
    if (FindIndex(name) != -1)
	return FALSE;

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
            table[i].type = 'F';
            // strncpy(table[i].name, name, FileNameMaxLen); 
            table[i].name = name;
            table[i].sector = newSector;
        return TRUE;
	}
    return FALSE;	// no space.  Fix when we have extensible files.
}

~~~

在`FildHeader`类的`Allocate`函数中设置创建时间，在`openfile`类的`read`和`write`函数中设置最近访问时间和最近修改时间，并将文件头的信息写回磁盘(为了方便写回，在`openfile`类中添加一个变量：文件头所在的磁盘扇区`sector)`.

~~~cpp
int
OpenFile::Read(char *into, int numBytes)
{
   int result = ReadAt(into, numBytes, seekPosition);
   seekPosition += result;
   time_t currentTime = time(NULL);
   hdr->lastAccessTime = currentTime;
   hdr->WriteBack(sector);
   return result;
}

int
OpenFile::Write(char *into, int numBytes)
{
   int result = WriteAt(into, numBytes, seekPosition);
   seekPosition += result;
    time_t currentTime = time(NULL);
   hdr->lastAccessTime = currentTime;
   hdr->lastWriteTime = currentTime;
   hdr->WriteBack(sector);
   return result;
}

~~~

测试：把`fstest.cc`中的文件大小设为50bytes,并修改`PerformanceTest`函数，注释掉删除文件的代码，在最后调用`fileSystem->Print()`.输入命令`./nachos -f`进行文件系统初始化，输入`./nachos -t `进行测试，可以按照预期创建文件、进行读写、输出文件属性：

![image-20200426002712735](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200426002712735.png)

![image-20200426002755420](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200426002755420.png)

另外，需要重新定义`NumDirect`宏。

~~~cpp
#define NumDirect 	((SectorSize - 2 * sizeof(int)-3*sizeof(time_t)) / sizeof(int))$
~~~

#### Exercise 3 扩展文件长度

> 改直接索引为间接索引，以突破文件长度不能超过 4KB 的限制。

初始版本中，由于文件头只能放在一个扇区中，只有有限的空间记录文件的数据所在的扇区号，因此文件大小受到限制。为了突破这一限制，采用简单的链表结构进行间接索引，即在文件头中加入一个指示下一个文件头扇区的变量`nextFdrSector`,若为-1则表示这是最后一个。相应地，需要修改`FileHeader`类的`Allocate`和`Deallocate`函数，在需要分配的数据扇区大于`NUmDirect`时，就New一个新的扇区块做链表中的下一项记录数据块号，若已记录完毕则标记`nextFdrSector`为-1。`Deallocate`时，遍历链表释放每一个数据块。

修改代码时要注意C语言中指针的性质，不要让新创建的文件头覆盖链表中的上一项。以关键的`Allocate`函数为例：

~~~cpp
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

~~~

相应地，还需要修改`Deallocate`、`ByteToTensor`、`Print`等函数，此处不赘述。

另外，由于文件头中添加了一个新变量，重新定义`NumDirect`宏：

~~~cpp
#define NumDirect 	((SectorSize - 3 * sizeof(int)-3*sizeof(time_t)) / sizeof(int))
~~~

将测试函数中的文件大小改为5000字节，修改代码前会因为内存越界报段错误：

![image-20200426021756586](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200426021756586.png)

修改后，重新测试，可以正常创建和读写5000字节大小的文件，内容也符合预期：

![image-20200426170604980](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200426170604980.png)

进一步压力测试，把文件大小改为50000，仍可以正常运行：

![image-20200426170839932](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200426170839932.png)

#### Exercise 4 实现多级目录

在`FileSystem`类中添加`CreateDir`函数用于创建文件夹，调用`Directory`类中新添加的`Add`函数，默认以根目录为当前目录，用`/`作为目录名之间的分隔符，解析后递归地检查子目录是否存在，若不存在则创建新目录项，将其文件头和所在磁盘块号记入上级目录表项中。同理，相应地修改`Remove`、`Print`、`List`函数。核心函数是`Directory`类的`Add`函数，修改后代码及注释如下：

~~~cpp
int 
Directory::Add(char *name, BitMap *freeMap,bool isDirectory)
{ 
    if (isDirectory && FindDir(name) != -1) // 如果文件夹已存在则返回-1
	    return -1;
    if (isDirectory == FALSE && Find(name) != -1) // 如果文件已存在则返回-1
        return -1; 
    int newSector = freeMap->Find(); // 为文件头分配sector
    if (newSector == -1){ // 如果磁盘空间不足，返回-1
        return -1;
    }
    /*解析文件名，拆分为两级*/
    int split = -1;
    for (int i = 0 ; i < strlen(name) ;++i){
        if (name[i] == '/'){
            split = i;
            break;
        }
    }
    if (split == -1){ // 如果只有一级，直接加到本目录下，返回
        for (int i = 0; i < tableSize; i++)
            if (!table[i].inUse) {
                table[i].inUse = TRUE;
                table[i].type = 'F';
                if (isDirectory)
                    table[i].type == 'D';
                // strncpy(table[i].name, name, FileNameMaxLen); 
                table[i].name = name;
                table[i].sector = newSector;
                return newSector;
            }
            return -1;
    }
    char* dir1 = new char[split + 1];
    for ( int i = 0 ; i < split; i++) dir1[i] = name[i];
    dir1[split] = '\0';
    char* dir2 = new char[strlen(name)-split];
    for ( int i = 0 ; i < strlen(name)-split-1 ; i++) dir2[i] = name[i+split+1];
    dir2[strlen(name)-split-1] = '\0';

    // 如果有两级，先判断第一级目录存不存在，如不存在则创建之，挂在本目录下，递归调用Add
    int subDirectorySector = FindDir(dir1);
    Directory* subDirectory;
    if (subDirectorySector == -1){ // 不存在子目录，创建之,并挂在父目录下
        subDirectory = new Directory(NumDirEntries);
        FileHeader *dirHdr = new FileHeader;
        subDirectorySector = freeMap->Find();
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));
        dirHdr->WriteBack(subDirectorySector);
        int  i = 0;
        for (i = 0; i < tableSize; i++)
            if (!table[i].inUse) {
                table[i].inUse = TRUE;
                table[i].type = 'D';
                // strncpy(table[i].name, name, FileNameMaxLen); 
                table[i].name = dir1;
                table[i].sector = subDirectorySector;
            }
        if( i == tableSize) return -1;
    }
    OpenFile* subDirectoryFile = new OpenFile(subDirectorySector);
    subDirectory->WriteBack(subDirectoryFile);
    
    int ans = subDirectory->Add(dir2,freeMap,isDirectory); // 递归调用
    subDirectory->WriteBack(subDirectoryFile);
    return ans;
}

~~~

目录类的`List`函数修改，每级增加缩进以表示目录树：

~~~cpp
void
Directory::List(int tabNum)
{
   for (int i = 0; i < tableSize; i++)
	if (table[i].inUse && table[i].type == 'F'){
        for(int j = 0 ; j < tabNum; j++) printf("  ");
	     printf("%s(File)\n", table[i].name);
    }
    else if (table[i].inUse && table[i].type == 'D'){
        for(int j = 0 ; j < tabNum; j++) printf("  ");
        printf("%s(Directory)\n", table[i].name);
        Directory* directory = new Directory(NumDirEntries);
        int subDirectorySector = table[i].sector;
        OpenFile* subdirectoryFile = new OpenFile(subDirectorySector);
        directory->FetchFrom(subdirectoryFile);
        directory->List(tabNum+1);
    }
}
~~~

测试：编写测试函数`HierarchicalDirectoryTest()`：

~~~cpp
void HierarchicalDirectoryTest(){
    fileSystem->Create("1.txt",100);
    fileSystem->Create("2.txt",100);
    fileSystem->Create("dir1/3.txt",100);
    fileSystem->Create("dir2/4.txt",100);
    fileSystem->Create("dir3/pku/xxx.avi",100);
    printf("###########\n");
    fileSystem->List();
}
~~~

在`filesys`文件夹下输入`./nachos -h` 命令调用，结果符合预期，正确实现了多级目录的功能：

![image-20200427022130468](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200427022130468.png)

#### Exercise 5 动态调整文件长度

> 对文件的创建操作和写入操作进行适当修改，以使其符合实习要求。

由于C语言没有类似C++或python的默认参数机制，为了满足“文件创建时没有必要指定大小”的要求，在`FileSystem`类中重载一个`Create`函数，只有`name`一个参数，把初始时设为大小为0。为了实现写入时动态调整大小的要求，只需修改`FileHeader::ByteToSector`函数，当需要写的偏移位置超过文件大小时，在文件末尾继续分配磁盘块即可。`ByteToSector`函数修改前的代码如下,默认`offset`不会超过文件大小。

~~~cpp
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
    //printf("Byte To Sector Return: %d",tmp_hdr->dataSectors[offset / SectorSize]);
    return(tmp_hdr->dataSectors[offset / SectorSize]);
}
~~~

现在对它进行修改，如果`offset`对应的磁盘块数目大于`numSectors`，则调用`AddSectors`函数添加相应数据的磁盘块，记录在索引链表末尾：

~~~cpp
int
FileHeader::ByteToSector(int offset,OpenFile* freeMapFile)
{
    FileHeader* tmp_hdr = this;
    int NumSectorsNeed = divRoundUp(offset, SectorSize);
    if (NumSectorsNeed > numSectors){
        this->AddSectors(NumSectorsNeed-numSectors+1,freeMapFile);
    }

    // printf("%d\n",tmp_hdr->nextFdrSector);
    while (offset >= NumDirect*SectorSize){
       // printf("### %d\n",tmp_hdr->nextFdrSector);
        FileHeader* Tmp_hdr = new FileHeader;
        Tmp_hdr->FetchFrom(tmp_hdr->nextFdrSector);
        offset -= NumDirect*SectorSize;
        tmp_hdr = Tmp_hdr;
    }
    //printf("Byte To Sector Return: %d",tmp_hdr->dataSectors[offset / SectorSize]);
    return(tmp_hdr->dataSectors[offset / SectorSize]);
}


~~~

`AddSectors`函数定义如下，思路由注释注明：

~~~cpp
bool
FileHeader::AddSectors(int num,OpenFile* freeMapFile){
    int old_num;
    FileHeader* hdr = this;
    FileHeader* new_hdr;
    BitMap *freeMap;
    int cur_sector,next_sector;
    while(true){ // get the tail first
        for (int i = 0; i < numSectors; i++) {
            if (hdr->nextFdrSector == -1) break;
            cur_sector = hdr->nextFdrSector;
            FileHeader* tmp_hdr = new FileHeader;
            tmp_hdr->FetchFrom(hdr->nextFdrSector);
            hdr = tmp_hdr;
        }
    }
    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    if (freeMap->NumClear() < num)
	    return FALSE;		// not enough space
    while(true){
        if(num == 0) break;
        int idx ;
        for ( idx = 0 ; idx < NumDirect; idx++){
            if (hdr->dataSectors[idx] == -1) break;
        }
        if (num <= NumDirect-idx) { // only need to add sectors for this file header
            for( int i = idx ; i < idx+num ; ++i){
                 hdr->dataSectors[i] = freeMap->Find();
            }
            break;
        } 
        else{ // Allocate a new link node for the file header
            for( int i = idx ; i < NumDirect ; ++i){ // 
                 hdr->dataSectors[i] = freeMap->Find();
                 num--;
            }
            new_hdr = new FileHeader;
            for(int j = 0; j < numSectors; j++) new_hdr->dataSectors[j] = -1;
            next_sector = freeMap->Find();	// find a sector to hold the new file header
            hdr->nextFdrSector = next_sector;
            hdr->WriteBack(cur_sector);
            cur_sector = next_sector;
            hdr = new_hdr;
        }
    }
    numSectors += old_num; // update the numSectors member
    numBytes += old_num*SectorSize;
    freeMap->WriteBack(freeMapFile);
    delete freeMap;
    return TRUE;
}
~~~

编写测试函数，不指定文件大小，创建一个默认大小为0的文件，五次写入`Contents`,都可以正常写入：

~~~cpp
void DynamicTest(){ 
    fileSystem->Create("dynamic.txt");
        OpenFile *openFile;    
    int i, numBytes;

    openFile = fileSystem->Open("dynamic.txt");
    //printf("%d\n", openFile->hdr->nextFdrSector);
    if (openFile == NULL) {
        printf("Dynamic test: unable to open %s\n", FileName);
        return;
    }
    for (i = 0; i < 5; i++) {
        numBytes = openFile->Write(Contents, ContentSize);
        printf("Write %d bytes:%s\n",10,Contents);
    }
    delete openFile;	// close file
}
~~~



![image-20200427034929030](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200427034929030.png)



### 第二部分 文件访问的同步与互斥

#### Exercise 6 源代码阅读

> 阅读 Nachos 源代码中与异步磁盘相关的代码，理解 Nachos 系统中异步访问模拟磁盘的工作原理。

相关的代码文件是`filesys/synchdisk.h`和`filesys/synchdisk.cc`,将磁盘类与信号量、锁封装在一个同步磁盘类中，互斥锁lock保证对磁盘的读写操作是互斥进行的。当一次读/写请求后产生中断，通过调用中断处理函数`DiskRequestDone`将信号量semaphore释放，从而使在等待信号量的其他线程得以访问磁盘。

~~~cpp
class SynchDisk {
  public:
    SynchDisk(char* name);    		// Initialize a synchronous disk,
					// by initializing the raw Disk.
    ~SynchDisk();			// De-allocate the synch disk data
    
    void ReadSector(int sectorNumber, char* data);
    					// Read/write a disk sector, returning
    					// only once the data is actually read 
					// or written.  These call
    					// Disk::ReadRequest/WriteRequest and
					// then wait until the request is done.
    void WriteSector(int sectorNumber, char* data);
    
    void RequestDone();			// Called by the disk device interrupt
					// handler, to signal that the
					// current disk operation is complete.

  private:
    Disk *disk;		  		// Raw disk device
    Semaphore *semaphore; 		// To synchronize requesting thread 
					// with the interrupt handler
    Lock *lock;		  		// Only one read/write request
					// can be sent to the disk at a time
};
~~~

> 利用异步访问模拟磁盘的工作原理，在 Class Console 的基础上，实现 Class SynchConsole。

模仿`SynchDisk`，将`Console`类和互斥锁、信号量(读写各一个)封装在一起实现`SynchConsole`类，通过锁和信号量实现控制台的同步互斥机制。

~~~cpp
char SynchConsole::getChar()
{
	char ch;
	lock->Acquire();
    readAvail->P();
	ch = console->GetChar();
	lock->Release();
	return ch;
}

void SynchConsole::putChar(char ch){
	lock->Acquire();
	console->PutChar(ch);
	writeDone->P();
	lock->Release();
}
~~~

测试：在main函数的用户程序部分，添加`-sc`参数调用`SynchConsoleTest`调用带有同步互斥机制的控制台，输入输出表现正常：

![image-20200426200536098](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200426200536098.png)

#### Exercise 7 实现文件系统的同步互斥访问机制

>  一个文件可以同时被多个线程访问。且每个线程独自打开文件，独自拥有一个当前文件访问位置，彼此间不会互相干扰。

> 所有对文件系统的操作必须是原子操作和序列化的。例如，当一个线程正在修改一 个文件，而另一个线程正在读取该文件的内容时，读线程要么读出修改过的文件， 要么读出原来的文件，不存在不可预计的中间状态。

对于第一点要求，由于`Nachos`中线程访问文件使用的是`OpenFile`数据结构，每个线程都可以独立打开文件并有自己独立的读写指针`position`,不需要修改代码就能实现。对于第二点要求，我选择实现第一类读写锁(读者优先)，即可以允许多个读者一起读，有读者在读时不允许写、有写者在写是不允许读，同时只能有一个进程在写文件，发生竞争时读者优先。为了实现第一类读写锁，需要在文件头中加入两个锁(读锁和写锁)，以及记录当前读者数目的变量`read_cnt`。之后在`OpenFile`的`Read`和`Write`函数中添加相应操作即可实现第一类读写锁。

在文件头的`Allocate`函数中添加相关变量的初始化：

~~~cpp
    readerCount = 0 ;
    readLock = new Lock("read Lock");
    writeLock = new Lock("write Lock");
~~~

修改`NumDirect`的宏定义：

~~~cpp
#define NumDirect 	( (SectorSize - 4*sizeof(int)- 3*sizeof(time_t) -2*sizeof(Lock*)) / sizeof(int) )
~~~

修改`read`函数，读之前先将获取读锁，将读者数目加一，若恰好为一则获得写锁，释放读锁，完成读操作后，再次获得读锁，将读者数目减一，若恰好为0则释放写锁，释放读锁，返回。

~~~cpp
int
OpenFile::Read(char *into, int numBytes)
{
   hdr->FetchFrom(hdrSector);
   hdr->readLock->Acquire();
   hdr->readerCount++;
   if (hdr->readerCount == 1){
       hdr->writeLock->Acquire();
   }
   hdr->readLock->Release();

   int result = ReadAt(into, numBytes, seekPosition);
   seekPosition += result;
   time_t currentTime = time(NULL);
   hdr->lastAccessTime = currentTime;
  // printf("read at time: %s\n",asctime(localtime(&currentTime)));
   hdr->readLock->Acquire();
   hdr->readerCount--;
   if (hdr->readerCount == 0){
       hdr->writeLock->Release();
   }
   hdr->readLock->Release();

   hdr->WriteBack(hdrSector);
   return result;
}
~~~

`Write`的操作相对简单，只需要获取和释放写锁即可：

~~~cpp
int
OpenFile::Write(char *into, int numBytes)
{
   hdr->FetchFrom(hdrSector);
   hdr->writeLock->Acquire();
   int result = WriteAt(into, numBytes, seekPosition);
   seekPosition += result;
    time_t currentTime = time(NULL);
   hdr->lastAccessTime = currentTime;
   hdr->lastWriteTime = currentTime;
   hdr->writeLock->Release();
   hdr->WriteBack(hdrSector);

  // printf("write at time: %s\n",asctime(localtime(&currentTime)));
   return result;
}
~~~

> 当某一线程欲删除一个文件，而另外一些线程正在访问该文件时，需保证所有线程关闭了这个文件，该文件才被删除。也就是说，只要还有一个线程打开了这个文件， 该文件就不能真正地被删除。

为实现此功能，只需在文件头中添加一个打开计数信号量`UserCount`即可，初始值为0，在`OpenFile`的构造函数中对其加一，析构函数中将其减一；为保证互斥访问用读者锁保护。`FileSystem`的`Removw`函数首先判断打开计数是否为0,如不为0，则提示删除失败， 直接返回。

在`FileHeader`的`Allocate`函数中添加初始化：

~~~cpp
usetCount = new int;
*userCount =  0;
~~~

再次修改`NumDirect`的宏定义：

~~~cpp
#define NumDirect 	( (SectorSize - 4*sizeof(int)- sizeof(int*)-3*sizeof(time_t) -2*sizeof(Lock*)) / sizeof(int) )
~~~

修改`OpenFile`的构造函数和析构函数：

~~~cpp
OpenFile::OpenFile(int sector)
{ 
    //printf("Num Direct %d\n",NumDirect);
    //printf("Sizeof FHR:%d\n",sizeof(FileHeader));
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    hdr->readLock->Acquire();
    *(hdr->userCount) ++;
    hdr->readLock->Release();
    seekPosition = 0;
    this->hdrSector = sector;
}
OpenFile::~OpenFile()
{
   // hdr->Print();
    hdr->readLock->Acquire();
    *(hdr->userCount) ++;
    hdr->WriteBack(hdrSector);
    delete hdr;
}
~~~

在`Remove`函数中添加对引用计数的判断，实现删除保护机制：

~~~cpp

    if(*(fileHdr->userCount) != 0){ 
        printf("File %s is busy now,failed to remove\n",name);
        return FALSE;
    }
~~~

### 第三部分 挑战题

#### Challenge 1 性能优化

> 例如，为了优化寻道时间和旋转延迟时间，可以将同一文件的数据块放置在磁盘同一磁道上

在初始版本中，文件头初始化时，为数据区分配每个数据块时都调用`freeMap->Find()`，由于碎片的存在，同一文件的数据块很可能不连续，分布在磁盘的不同磁道上，导致磁盘访问性能的下降。为了尽量避免这一问题，可以修改`Allocate`函数，先调用`freeMap`的`FindArea`函数试图获取一块连续的区域，如没有足够长的连续区域再使用原来的逐个`find`的机制,如此就可以将同一文件的数据块尽量放在同一磁道上。`FindArea`的代码如下：

~~~cpp
int BitMap::FindArea(int size){
    int s = 0 , e = 0 ;
    while (e < numBits) {
        while(Test(s) && s < numBits) s++,e++;
        while(Test(e) && e < numBits) e++;
        if (e-s >= size){
            for(int i = 0 ; i < size ; ++i){
                Mark(s+i);
            }
            return s;
        }
    }
    return -1;
}
~~~

> 使用cache机制减少磁盘访问次数，例如延迟写和预读取

在磁盘中加入Cache,使用`LRU`替换策略，为简便起见采取`Write Through`,靠预读取提升磁盘读取的性能。在加入Cache前，运行测试程序`./nahos -t`的磁盘读写次数如下图：

![image-20200427162922113](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200427162922113.png)

Cache表项的定义与构造函数、析构函数如下：

~~~cpp
class CacheLine{
  public:
    CacheLine();
    ~CacheLine();
    bool valid;
    bool dirty;
    int sector;
    int  lru;
    char* data; 
}
~~~

~~~cpp
CacheLine::CacheLine(){ 
    valid = false;
    dirty = false;
    sector = -1;
    lru = -1;
    data = new char[SectorSize];
}

CacheLine::~CacheLine(){
    if (data != NULL) delete data;
}
~~~

定义`CacheSize`宏为8,在`SynchDisk`的构造函数中初始化缓存：

~~~c
 Cache = new CacheLine[CacheSize];
~~~

在`SynchDisk`的`ReadSector`和`WriteSector`函数中添加缓存机制：

~~~cpp

void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    int pos = -1;
    for(int i = 0 ; i < CacheSize; i++){
        if(Cache[i].valid && Cache[i].sector == sectorNumber){
            pos = i;
            break;
        }
    }

    if (pos != -1) { // Cache Hit 
        Cache[pos].lru = stats->totalTicks;
        bcopy(Cache[pos].data, data,SectorSize);
        disk->HandleInterrupt();
        return;
    }
    
    int swap = -1 ; // Choose Cache Line To Write
    for(int i = 0 ; i < CacheSize ; i++){
        if (!Cache[i].valid) { // Check if there is an empty cache line
            swap = i ; 
            break;
        }
    }
    if (swap != -1){
        swap = 0 ;
        int LastAccessTime = Cache[0].lru ; // if cache is full,choose a line to replace according to LRU
        for(int i = 1 ; i < CacheSize ; i++){
            if (Cache[i].lru < LastAccessTime){
                LastAccessTime = Cache[i].lru ;
                swap = i;
            }
        }
    }

    lock->Acquire();			// only one disk I/O at a time
    disk->ReadRequest(sectorNumber, data);
    // Write On Cache 
    Cache[swap].valid = TRUE;
    Cache[swap].sector = sectorNumber;
    Cache[swap].lru = stats->totalTicks; // update lru
    Cache[swap].dirty = FALSE;
    bcopy(data,Cache[pos].data,SectorSize); // update data
    semaphore->P();			// wait for interrupt
    lock->Release();
}
~~~

重新运行测试程序，磁盘读写次数减少，性能得到提升。文件内容没有错误：

![image-20200427172622692](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200427172622692.png)

#### Challenge 2 实现pipe机制

> 重定向openfile的输入输出方式，使得前一进程从控制台读入数据并输出至管道，后一进程从管道读入数据并输出至控制台。

使用文件模拟pipe机制，为简便起见，只支持文件系统中的单个pipe，即用一个文件作为缓冲区实现进程间通信。在文件系统初始化时创建一个文件`PipeFile`模拟`pipe`,需要向管道写或从管道读数据时指明数据源/目的和字节数即可。

~~~cpp
int FileSystem::ReadPipe(char* data,int numBytes){

    OpenFile * pipeFile = new OpenFile(PipeSector);
    int size = pipeFile->Read(data,numBytes);
    delete pipeFile;
    printf("Get %s bytes of data from pipe:%s\n",size,data);
    return size;
}

int FileSystem::WritePipe(char* data,int numBytes){

    OpenFile * pipeFile = new OpenFile(PipeSector);
    int size = pipeFile->Write(data,numBytes);
    delete pipeFile;
    printf("Get %s bytes of data from pipe:%s\n",size,data);
    return size;
}
~~~

编写测试函数，主线程先从控制台读入数据写入管道，Fork一个子线程从管道中读数据并输出：

~~~cpp
void getDataFromPipe(int dummy){ 
    printf("In thread B: reading from pipe to console:\n");
    char* output_data = new char[11];
    printf("\033[1;31m");
    printf("Output:");
    printf("\033[0m");

    fileSystem->ReadPipe(output_data,10);
    output_data[10] = '\0';
    printf("\033[1;33m");
    printf("%s\n",output_data);
    printf("\033[0m");
}

void PipeTest(){ 
    printf("Starting pipe performance test:\n");
    printf("In thread A: reading from console to pipe:\n");
    char* input_data = new char[10];
    printf("\033[1;31m");
    printf("Input:");
    printf("\033[0m");
    scanf("%s", input_data);
    fileSystem->WritePipe(input_data,strlen(input_data));
    Thread* t2 = new Thread("Thread B");
    t2->Fork(getDataFromPipe,0);

}
~~~

重新编译，在`filesys`文件夹下输入`./nachos -pt`进行测试，输入输出处用颜色标出，可见模拟的Pipe机制可以正确实现进程间通信。

![image-20200427202929692](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200427202929692.png)

### 编译选项

开始时在`filesys`下执行`./nachos -t` ，实际执行的还是线程实验的测试函数，阅读makefile发现是`filesys`下的`makefile`中`-DTHREAD`选项导致的，删除该选项后重新编译，即可正常执行文件系统的测试。

### 实现间接索引时引发段错误

开始测试Exercise 3(间接索引)时一直产生段错误，检查发现是遍历链表时覆盖了上一项，将代码改为每次new一个`FileHeader`去读取下一项后解决此问题。(对C的指针还是要特别小心)

## 内容四 收获与感想

这次Lab的内容非常多，编码和测试的过程比较繁琐，通过动手实践，我对操作系统文件系统的理解大大加深了，原来日常使用的文件系统下隐藏着如此多精细的设计，包括文件头的信息记录、磁盘块索引的组织、多级目录结构、文件的互斥访问等等，这些机制都被操作系统封装起来，日常只需要输入简单的命令就可以高效管理文件，深刻体现了操作系统的虚拟性特征。另外，这次修改代码涉及到大量指针的使用，修改过程中非常容易引发段错误，使用`gdb`和`valgrind`来debug非常有助于检查内存泄漏的情况。

## 内容五 对课程的意见与建议

这种内容非常繁琐、周期比较长的Lab，建议考虑拆分成几个小的Lab让同学们每周做一部分，有助于同学们把握节奏、循序渐进。另外很多同学是和操作系统课一起上实习课的，正课的进度比实习课慢，会造成一些知识点上的不足，建议今后考虑适当统一一下教学进度。

## 内容六 参考文献

无。