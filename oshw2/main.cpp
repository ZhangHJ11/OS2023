//
// Created by jim_h on 2023/4/9.
//
#include <iostream>
#include <cstring>
#include <vector>
#include <stack>
#include <string>
#include <stdio.h>

using namespace std;

#define IMAGE_SIZE 1440 * 1024
#define NORMAL_FILE 0x20
#define DIR_FILE 0x10

extern "C"
{
    void my_puts(const char *);
}

// sizeOf == 32 根目录中每一个文件对应一个fileentry
struct FileEntry
{
    char DIR_Name[11];          // 文件名字8字节 扩展名字3字节
    unsigned char DIR_Attr;     // 文件属性
    unsigned char reserve[10];  // 保留位
    unsigned short DIR_WrtTime; // 最后一次写入时间
    unsigned short DIR_WrtDate; // 最后一次写入日期
    unsigned short DIR_FstClus; // 对应的第一个簇
    unsigned int DIR_FileSize;  // 文件大小
};
// 字符分割函数
vector<string> split(string str, char op);
// 路径分割函数
void splitPath(const char *const path, vector<string> &paths);
// 获取文件名称函数
string getFilename(const char *filename);
// 读取char转换成int 注意是小端模式
int char2int(const unsigned char *little, int start, int length)
{
    int tmp = 0;
    for (int i = start + length - 1; i >= start; i--)
        // 一个一个字节读取
        tmp = tmp * 256 + little[i];
    return tmp;
}
// 读取int转换成int 注意是小端模式
int int2int(const unsigned int source)
{
    // Todo
    unsigned char intermediate[4];
    intermediate[0] = source >> 24 & 0xFF;
    intermediate[1] = source >> 16 & 0xFF;
    intermediate[2] = source >> 8 & 0xFF;
    intermediate[3] = source & 0xFF;
    return source;
}
// 读取short转换成int 注意是小端模式
int short2int(const unsigned short source)
{
    unsigned char s_h = source & 0b0000000011111111;
    unsigned char s_l = source >> 8;
    return s_l * 256 + s_h;
}
// 加载数据 注意是小端模式
void loadData(unsigned char *from, unsigned char *to, int start, int size)
{
    for (int i = 0; i < size; i++)
    {
        to[i] = from[start + i];
    }
}

// 读取文件系统
class FAT12Reader
{
private:
    unsigned char data[1440 * 1024];
    int BPB_BytesPerSec;    // 每扇区最大字节数
    int BPB_RootEntCnt;     // 根目录文件最大数量
    int Fat1StartSector;    // fat1起始扇区
    int Fat2StartSector;    // fat2起始扇区
    int RootDirStartSector; // 根目录起始扇区
    int RootDirSectors;     // 根目录大小
    int DataStartSector;    // 数据区起始扇区

public:
    vector<FileEntry *> fileEntry; // 根目录遍历得到所有文件的fileentry
    FAT12Reader(const char *Path); // 构造函数 完成boot扇区中数据的获得和entry
    int getFileEntries(const char *path, vector<FileEntry *> *fileEntries);
    string readfile(const FileEntry *fileEntry);
    int readfat(int clusnum); // 已知当前簇号，去读取fat表中对应簇号的内容
    void mytest();
} fat12Reader("./b.img");

FAT12Reader::FAT12Reader(const char *Path)
{
    FILE *file = fopen(Path, "r");
    fread(data, 1440 * 1280, 1, file);
    BPB_BytesPerSec = char2int(data, 11, 2); // 0x200 = 512
    BPB_RootEntCnt = char2int(data, 17, 2);  // 0xE0 = 11100000B = 224
    Fat1StartSector = 1;                     // fat1起始扇区
    Fat2StartSector = 10;                    // fat2起始扇区
    RootDirStartSector = 19;                 // 根目录起始扇区
    RootDirSectors = (BPB_RootEntCnt * sizeof(FileEntry) + BPB_BytesPerSec - 1) / BPB_BytesPerSec;
    DataStartSector = RootDirStartSector + RootDirSectors;
    // cout << BPB_BytesPerSec << endl;
    // cout << BPB_RootEntCnt << endl;
    // cout << DataStartSector << endl;
    // bfs遍历根目录区
    for (int i = 0; i < BPB_RootEntCnt; i++)
    {
        struct FileEntry *fE = new FileEntry;
        int base = BPB_BytesPerSec * RootDirStartSector;
        int offset = i * sizeof(FileEntry);
        unsigned char tmp[sizeof(FileEntry)];
        // 从data中读出，转化格式
        loadData(data, tmp, base + offset, sizeof(FileEntry));
        memcpy(fE, tmp, sizeof(FileEntry));
        if (fE->DIR_Name[0] <= 0 || (fE->DIR_Attr != NORMAL_FILE && fE->DIR_Attr != DIR_FILE))
        {
            // 错误的文件
            delete fE;
            continue;
        }
        fileEntry.push_back(fE);
    }
    // cout << fileEntry.size() << endl;
    // cout << getFilename(fileEntry[1]->DIR_Name) << endl;
    // cout << fileEntry[1]->DIR_Attr << endl;
    // cout << short2int(fileEntry[1]->DIR_FstClus) << endl;
    // cout << fileEntry[1]->DIR_FstClus << endl;
}

int FAT12Reader::getFileEntries(const char *path, vector<FileEntry *> *fileEntries)
{
    // 从路径得到entries
    fileEntries->clear();
    for (FileEntry *fe : fileEntry)
        fileEntries->push_back(fe);
    // 分割路径
    vector<string> paths;
    splitPath(path, paths);
    // 根目录
    if (string(path) == "/")
        return DIR_FILE;
    // 根据名字寻找路径
    for (int i = 0; i < paths.size(); i++)
    {
        FileEntry *entry = nullptr;
        for (FileEntry *t : *fileEntries)
        {
            // cout << getFilename(t->DIR_Name) << ':' << paths[i] << endl;
            if (getFilename(t->DIR_Name) == paths[i]) // 找到了
            {
                entry = t;
                break;
            }
        }

        fileEntries->clear();
        if (entry == nullptr)
            return 0; // 没找到出错了
        else if (i + 1 == paths.size() && entry->DIR_Attr == NORMAL_FILE)
        {
            fileEntries->push_back(entry);
            return NORMAL_FILE; // 是最后一个且是普通文件
        }
        else if (entry->DIR_Attr = DIR_FILE)
        {
            // TODO: 文件夹目录
        }
        else
        {
            return 0;
        }
    }
    return 0;
}

string FAT12Reader::readfile(const FileEntry *fileEntry)
{
    unsigned char bytes[BPB_BytesPerSec];
    int fileSize = int2int(fileEntry->DIR_FileSize);
    int nextClus = short2int(fileEntry->DIR_FstClus);
    char a[fileSize + 1];
    if (fileSize < BPB_BytesPerSec)
    {
        // 这一行读取一个完整的扇区,然后把文件大小的内容存储起来
        // DataStartSector + nextClus - 2 从data中的起始地址读一个
        loadData(data, bytes, (DataStartSector + nextClus - 2) * BPB_BytesPerSec, BPB_BytesPerSec);
        memcpy(a, bytes, fileSize);
        a[fileSize] = '\0'; // 有乱码
        return string(a);
    }
    else
    {
        // TODO：文件大小大于一个扇区的大小
        int times = 0;
        while (nextClus != 0)
        {
            loadData(data, bytes, (DataStartSector + nextClus - 2) * BPB_BytesPerSec, BPB_BytesPerSec);
            nextClus = readfat(nextClus);
            if (nextClus != 0)
            {
                memcpy(a + times * BPB_BytesPerSec, bytes, BPB_BytesPerSec);
                times++;
            }
            else
            {
                memcpy(a + times * BPB_BytesPerSec, bytes, fileSize % BPB_BytesPerSec);
            }
        }
        // 不断读取 （读 -> readfat -> 读）
        a[fileSize] = '\0'; // 有乱码
        return string(a);
    }
    return string(a);
}

int FAT12Reader::readfat(int clusnum)
{
    // 起始位置
    int start = Fat1StartSector * BPB_BytesPerSec + (clusnum * 3) / 2;
    unsigned char tmp[2];
    // 去读两个字节
    loadData(data, tmp, start, 2);
    // 分奇数偶数计算
    int nextclusnum;
    // 807060 0:8070->080 1:7060->607
    if (clusnum % 2 == 0)
        // 以char存储, 读出来是倒着读的
        nextclusnum = tmp[1] % 16 * 256 + tmp[0];
    else if (clusnum % 2 == 1)
        nextclusnum = tmp[1] * 16 + tmp[0] / 16;
    // error
    if (nextclusnum >= 0xff7 || nextclusnum <= 1)
    {
        return 0;
    }
    return nextclusnum;
}

void FAT12Reader::mytest()
{
}

int main()
{
    string cmd;
    string keyWord;
    bool isProper;
    vector<string> args;

    while (1)
    {
        my_puts("> ");
        if (!getline(cin, cmd))
            break;
        vector<string> command = split(cmd, ' ');
        isProper = true;
        args.clear();
        keyWord = command[0];
        command.erase(command.begin());
        if (keyWord == "exit")
        {
            if (command.empty())
                break;
            else
                // cout << "exit do not have parameters" << endl;
                my_puts("exit do not have parameters\n");
        }
        else if (keyWord == "cat")
        {
            // step one: 处理命令
            for (int i = 0; i < command.size(); i++)
            {
                if (command[i][0] == '-')
                {
                    my_puts(("option is not supported by \'cat\': " + command[i] + "\n").c_str());
                    isProper = false;
                    break;
                }
                else
                {
                    if (args.size() == 0)
                        args.push_back(command[i]);
                    else
                    {
                        my_puts("multiple options are not supported by \'cat\' \n");
                        isProper = false;
                        break;
                    }
                }
            }
            if (!isProper)
                continue;

            // step two: 输出
            vector<FileEntry *> fileEntries_cat;
            int type = fat12Reader.getFileEntries(args[0].data(), &fileEntries_cat);
            // cout << args[0] << endl;
            // cout << fileEntries_cat.size() << endl;
            // cout << type << endl;
            if (type != 32)
                my_puts("this file type is not supported by \'cat\'");
            else
            {
                string content = fat12Reader.readfile(fileEntries_cat[0]);
                my_puts(content.c_str());
            }
        }
        else if (keyWord == "ls")
        {
            for (string i : command)
            {
                if (i[0] == '-')
                {
                    if (i.length() == 1)
                    {
                        my_puts("lack option\n");
                        isProper = false;
                        break;
                    }
                    for (char t : i.substr(1))
                    {
                        if (t != 'l')
                        {
                            my_puts(("this option is not supported: " + i + "\n").c_str());
                            isProper = false;
                            break;
                        }
                    }
                }
                else if (i[0] == '/')
                {
                    // args(paths)
                    if (args.size() == 0)
                    {
                        args.push_back(i);
                    }
                    else
                    {
                        my_puts("only support one path\n");
                        isProper = false;
                        break;
                    }
                }
                else
                {
                    my_puts("path should start with \'\\' \n");
                    isProper = false;
                    break;
                }
            }
            if (!isProper)
                continue;
        }
        else
        {
            // cout << "unsupported cmd" << endl;
            my_puts("command not found\n");
        }
    }
    return 0;
}

vector<string> split(string str, char op)
{
    //    ls -l
    vector<string> tmp;
    int index = 0;
    for (int i = 0; i < str.length(); i++)
    {
        if (str[i] == op)
        {
            tmp.push_back(str.substr(index, i - index));
            index = i + 1;
        }
    }
    tmp.push_back(str.substr(index, str.length() - index));
    return tmp;
}

string getFilename(const char *filename)
{
    char tmp[12];
    tmp[11] = '\0';
    memcpy(tmp, filename, 11);

    string s = (string)tmp;
    string normalname = s.substr(0, 8);
    string extensionname = s.substr(8, 3);
    // 没有填满字节数，则会用空格填充
    for (int i = 0; i < normalname.length(); i++)
    {
        if (normalname[i] == 32)
        {
            normalname = normalname.substr(0, i);
            break;
        }
    }
    for (int i = 0; i < extensionname.length(); i++)
    {
        if (extensionname[i] == 32)
        {
            extensionname = extensionname.substr(0, i);
            break;
        }
    }
    return (extensionname.size() > 0) ? normalname + '.' + extensionname : normalname;
}

void splitPath(const char *const path, vector<string> &paths)
{
    string tmp = "";
    vector<string> segs = split(path, '/');
    for (string seg : segs)
        if (seg.size() > 0)
            paths.push_back(seg);
}