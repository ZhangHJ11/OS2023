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
// 把所有路径都规约成最后含有/符号的路径
string adjustPath(const string path, const string content)
{
    if (path[path.size() - 1] == '/')
    {
        return (content.size() == 0) ? path : path + content + "/";
    }
    else
    {
        return (content.size() == 0) ? path + "/" : path + "/" + content + "/";
    }
}
// 输出路径时去掉..和.，以免ls /NJU/..时显示/NJU/../A: 使用了栈的结构
string adjustoutput(const string path)
{
    vector<string> paths;
    splitPath(path.c_str(), paths);
    vector<string> ans;
    string tmp = "/";
    for (string p : paths)
    {
        if (p == ".")
            continue;
        if (p == "..")
            if (ans.size() == 0)
                continue;
            else
                ans.pop_back();
        else
            ans.push_back(p);
    }
    for (string a : ans)
    {
        tmp += a;
        tmp += "/";
    }
    return tmp;
}
string adjustpoint(const string path)
{
    vector<string> paths;
    splitPath(path.c_str(), paths);
    vector<string> ans;
    string tmp = "/";
    for (string p : paths)
    {
        if (p == ".")
            continue;
        else
            ans.push_back(p);
    }
    for (string a : ans)
    {
        tmp += a;
        tmp += "/";
    }
    return tmp;
}
// 红色字体
void print_red_start();
void print_red_end();
// 输出文件大小
void print_int(int x)
{
    if (x == 0)
        my_puts("0");
    else
    {
        int x1 = x;
        char tmp[1000];
        int len = 0;
        while (x)
        {
            len++;
            x /= 10;
        }
        int c = len - 1;
        *(tmp + len) = 0;
        while (c >= 0)
        {
            tmp[c] = '0' + x1 % 10;
            c--;
            x1 /= 10;
        }
        my_puts(tmp);
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
    string readfile(const FileEntry *fileEntry); // 读文件中的文本内容
    int readfat(int clusnum);                    // 已知当前簇号，去读取fat表中对应簇号的内容
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
}

int FAT12Reader::getFileEntries(const char *path, vector<FileEntry *> *fileEntries)
{
    // 从路径得到entries，即返回文件类型和path的所有entries，如果是文件就返回文件的entry
    fileEntries->clear();
    for (FileEntry *fe : fileEntry)
        fileEntries->push_back(fe);
    // 分割路径
    vector<string> paths;
    splitPath(path, paths);
    // 根目录
    if (string(path) == "/")
    {
        return DIR_FILE;
    }

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
        // 没找到出错 对于根目录需要另外判断
        if (entry == nullptr && paths[i] != "..")
            return 0;
        if (entry == nullptr && paths[i] == "..")
        {
            for (FileEntry *fe : fileEntry)
                fileEntries->push_back(fe);
            continue;
        }
        // 是普通文件且是最后一个
        if (i + 1 == paths.size() && entry->DIR_Attr == NORMAL_FILE)
        {
            fileEntries->push_back(entry);
            return NORMAL_FILE;
        }
        // 是普通文件但不是最后一个
        if (i + 1 != paths.size() && entry->DIR_Attr == NORMAL_FILE)
        {
            return -1;
        }
        if (entry->DIR_Attr = DIR_FILE)
        {
            // 文件夹目录 遍历所有簇中，通过名字判断是否相同，如果文件夹比较大，那么他对应的fat项就不是FFF
            int nextClus = short2int(entry->DIR_FstClus);
            if (nextClus == 0)
            {
                // 回到根目录的方式和回到其他目录不同，都需要特判
                fileEntries->clear();
                for (FileEntry *fe : fileEntry)
                    fileEntries->push_back(fe);
                continue;
            }
            unsigned char buf[BPB_BytesPerSec];
            while (nextClus > 0)
            {
                loadData(data, buf, (DataStartSector + nextClus - 2) * BPB_BytesPerSec, BPB_BytesPerSec);
                nextClus = readfat(nextClus);
                for (unsigned int i = 0; i < BPB_BytesPerSec / sizeof(FileEntry); i++)
                {
                    FileEntry *entry = new FileEntry;
                    memcpy(entry, buf + i * sizeof(FileEntry), sizeof(FileEntry));
                    if (entry->DIR_Name[0] <= 0 ||
                        (entry->DIR_Attr != NORMAL_FILE && entry->DIR_Attr != DIR_FILE))
                    {
                        delete entry;
                        continue;
                    }
                    fileEntries->push_back(entry);
                }
            }
        }
        else
        {
            // 通过返回值来判断出错类型
            return 0;
        }
    }
    return DIR_FILE;
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

// ls _ dfs
void dfs(const char *path, bool islong)
{
    vector<FileEntry *> fileEntries;
    vector<string> subDirPaths;
    // 根据当前路径，找到fileentry
    int CODE = fat12Reader.getFileEntries(path, &fileEntries);
    if (CODE == NORMAL_FILE)
        return;

    // root
    my_puts(adjustoutput((adjustPath(path, ""))).c_str());
    if (islong)
    {
        int filenum = 0;
        int dirnum = 0;
        for (FileEntry *fe1 : fileEntries)
        {
            if ((fe1->DIR_Attr) == NORMAL_FILE)
                filenum++;
            if ((fe1->DIR_Attr) == DIR_FILE && fe1->DIR_Name[0] != '.')
                dirnum++;
        }
        my_puts(" ");
        print_int(dirnum);
        my_puts(" ");
        print_int(filenum);
    }
    my_puts(":\n");

    for (int i = 0; i < fileEntries.size(); i++)
    {
        FileEntry *fe = fileEntries[i];
        if (fe->DIR_Attr == DIR_FILE)
        {
            if (islong)
            {
                vector<FileEntry *> subFEs;
                fat12Reader.getFileEntries(adjustPath(path, getFilename(fe->DIR_Name)).data(), &subFEs);
                int filenum = 0;
                int dirnum = 0;
                for (FileEntry *fe1 : subFEs)
                {
                    if ((fe1->DIR_Attr) == NORMAL_FILE)
                        filenum++;
                    if ((fe1->DIR_Attr) == DIR_FILE && fe1->DIR_Name[0] != '.')
                        dirnum++;
                }
                print_red_start();
                my_puts((getFilename(fe->DIR_Name) + " ").c_str());
                print_red_end();

                if (fe->DIR_Name[0] != '.')
                {
                    print_int(dirnum);
                    my_puts(" ");
                    print_int(filenum);
                    my_puts("\n");
                }
                else
                    my_puts("\n");
            }
            else
            {
                print_red_start();
                my_puts((getFilename(fe->DIR_Name) + "  ").c_str());
                print_red_end();
            }
            if (fe->DIR_Name[0] != '.')
            {
                subDirPaths.push_back(adjustPath(path, getFilename(fe->DIR_Name)));
            }
        }
        else if (fe->DIR_Attr == NORMAL_FILE)
        {
            if (islong)
            {
                my_puts((getFilename(fe->DIR_Name) + " ").c_str());
                print_int(int2int(fe->DIR_FileSize));
                my_puts("\n");
            }
            else
                my_puts((getFilename(fe->DIR_Name) + "  ").c_str());
        }
    }

    if (!islong)
        my_puts("\n");

    // print sub
    for (string subPath : subDirPaths)
        dfs(subPath.data(), islong);
}

int main()
{
    string cmd;
    string keyWord;
    bool isProper;
    bool isLong; // ls -l
    vector<string> args;

    while (1)
    {
        my_puts("> ");
        if (!getline(cin, cmd))
            break;
        vector<string> command = split(cmd, ' ');
        isProper = true;
        isLong = false;
        args.clear();
        keyWord = command[0];
        command.erase(command.begin());
        if (keyWord == "exit")
        {
            if (command.empty())
                break;
            else
                my_puts("exit do not have parameters\n");
        }
        else if (keyWord == "cat")
        {
            // step one: 处理命令 没有参数 错误选项
            if (command.size() == 0)
            {
                my_puts("lack path\n");
                continue;
            }
            for (int i = 0; i < command.size(); i++)
            {
                if (command[i][0] == '-')
                {
                    my_puts(("this option is not supported by \'cat\': " + command[i] + "\n").c_str());
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
            if (type == 0)
                my_puts("not found \n");
            else if (type == -1)
            {
                my_puts("invalid path \n");
            }
            else if (type != 32)
            {
                my_puts("not a file \n");
            }
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
                            my_puts(("this option is not supported by \'ls\': : " + i + "\n").c_str());
                            isProper = false;
                            break;
                        }
                    }
                    isLong = true;
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
                    my_puts("path should start with \'/\' \n");
                    isProper = false;
                    break;
                }
            }
            if (!isProper)
                continue;

            string path = args.size() > 0 ? args[0] : "/";
            path = adjustpoint(path);
            vector<FileEntry *> fileEntries_ls;
            int type = fat12Reader.getFileEntries(path.data(), &fileEntries_ls);
            if (type == NORMAL_FILE)
            {
                my_puts("not a directory\n");
            }
            else if (type == DIR_FILE)
            {
                dfs(path.data(), isLong);
            }
            else if (type == -1)
            {
                my_puts("invalid path \n");
            }
            else
            {
                my_puts("not found\n");
            }
        }
        else
        {
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

void print_red_start()
{
    my_puts("\033[31m");
}
void print_red_end()
{
    my_puts("\033[0m");
}