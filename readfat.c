#include <stdio.h>
#include <sys/stat.h> 
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

// only support VFAT16 /*FIXME*/

/*#define MBR_FS_TYPE_OFFSET */
#define MBR_SECTOR_SIZE_OFFSET 0x0B
#define MBR_FAT_OFFSET 0x0E
#define MBR_FAT_CNT_OFFSET 0x10
#define MBR_SECTOR_PER_FAT 0x16
#define MBR_SECTORS_PER_CLUSTER 0x0D

#define ENTRY_NAME_OFFSET 0x0
#define ENTRY_CLUSTER_OFFSET 0x1A
#define ENTRY_FSIZE_OFFSET 0x1C
#define ENTRY_SIZE 32

unsigned short mbr_sector_size(char *baseaddr)
{
    unsigned short ret;
    ret = *((unsigned short *)(baseaddr + MBR_SECTOR_SIZE_OFFSET));
    return ret;
}

char *mbr_fat(char* baseaddr)
{
    char *ret;
    ret = baseaddr + mbr_sector_size(baseaddr) * (*((unsigned short*)(baseaddr + MBR_FAT_OFFSET)));
    return ret;
}

int mbr_fat_cnt(char *baseaddr)
{
    int ret;
    ret =   (char)(*(baseaddr + MBR_FAT_CNT_OFFSET));
    return ret;
}
unsigned short mbr_sectors_peer_fat(char *baseaddr)
{
    unsigned short ret;
    ret = *((unsigned short*)(baseaddr + MBR_SECTOR_PER_FAT));
    return  ret;
}
unsigned char mbr_sectors_peer_cluster(char *baseaddr)
{
    unsigned char ret;
    ret = *((unsigned char*)(baseaddr + MBR_SECTORS_PER_CLUSTER));
    return ret;
}
char *mbr_entrys(char * baseaddr)
{
    char * ret;
#ifdef FAT32
    return (unsigned int)(*(baseaddr + 0xC));
#else
    ret =  mbr_fat(baseaddr) + mbr_fat_cnt(baseaddr) * mbr_sectors_peer_fat(baseaddr) * mbr_sector_size(baseaddr);
    return ret;
#endif
}
int entry_is_null(char *entry)
{
    int ret;
    ret = *entry == 0? 1:0;
    printf("            [entry_is_null] entry[0]=%x, ret=%d\n", *entry, ret);
    return  ret;
}

char *entry_name(char *entry)
{
    char *name;
    name =  (char*)(entry + ENTRY_NAME_OFFSET);
    printf("            [entry_name] name=%s\n", name);
    return name;
}
unsigned short entry_cluster(char *entry)
{
    unsigned short ret;
    ret = *((unsigned short*)(entry + ENTRY_CLUSTER_OFFSET));
    printf("            [entry_cluster] cluster=%u\n", ret);
    return ret;
}
unsigned int entry_fsize(char *entry)
{
    unsigned int ret;
    ret = *((unsigned int*)(entry + ENTRY_FSIZE_OFFSET));
    printf("            [entry_fsize] fsize=%u\n", ret);
    return ret;
}
char *entry_next_entry(char *entry)
{
    return entry + ENTRY_SIZE;
}

int is_endofile(unsigned int cluster)
{
    printf("        [is_endofile] cluster=%u\n", cluster);
    if (cluster == 0xffff || cluster <2) {
        return 1;
    }
    return 0;
}

int read_content(void *dst, void *src, size_t cnt, unsigned int cluster)
{
    int clustersize;
    int offset;
    clustersize = mbr_sectors_peer_cluster(src) * mbr_sector_size(src);
    offset = clustersize * cluster;
    printf("[read_content] cluster=%d,clustersize=%d, cnt=%d, offset=%d\n", cluster, clustersize, cnt, offset);
    memcpy(dst, src + offset, cnt);
    return cnt;
}

unsigned short next_cluster(char* fat_base, int cluster)
{
    unsigned short ret;
    ret = *((unsigned short*)((unsigned short*)fat_base + cluster));
    printf("    [next_cluster] @fat_base=%p, cluster=%d, next=%d\n", fat_base, cluster, ret);
    return ret;
}

int min(int a, int b)
{
    return a>b?b:a;
}


char * do_job(int *fsize, char *inbuf)
{
    /*int fs_type;*/
    char *fat_base;
    char *entrys, *entry;
    int cluster;
    int filesize ;
    char *name;
    char *fbuf;
    int cnt, ret;
    char *pret;
    int cluster_size;

    // debug
    ret = mbr_sector_size(inbuf);
    printf("  [mbr_sector_size] = %u\n", ret);
    pret = mbr_fat(inbuf);
    printf("  [mbr_fat] base=0x%x, fat=0x%x, offset=%d\n", (unsigned int)inbuf, (unsigned int)pret, pret - inbuf);
    ret = mbr_fat_cnt(inbuf);
    printf("  [mbr_fat_cnt] ret=%u\n", (unsigned)ret);
    ret = mbr_sectors_peer_fat(inbuf);
    printf("  [mbr_sectors_peer_fat] ret=%u\n", (unsigned)ret);
    ret = mbr_sectors_peer_cluster(inbuf);
    printf("  [mbr_sectors_peer_cluster] = %u\n", (unsigned)ret);
    pret = mbr_entrys(inbuf);
    printf("    [mbr_entrys] base=0x%x, entrys=0x%x, offset=%d\n", (unsigned int)inbuf, (unsigned int)pret, pret - inbuf);

    // debug end

    // get fat,entry 's address
    fat_base = mbr_fat(inbuf);
    printf("fat_base=%d\n", (unsigned)(fat_base - inbuf));

    entrys = mbr_entrys(inbuf);
    printf("entrys=%d\n", (unsigned)(entrys - inbuf));

    // value init
    cluster = -1;
    filesize = 0;

    // find uboot entry
    entry = entrys;
    while ( entry_is_null(entry) != 1) {
        name = entry_name(entry);
        if (strncmp(name, "UBOOT", 5) == 0) {
            printf("-------------get uboot------------\n");
            cluster = entry_cluster(entry);
            filesize = entry_fsize(entry);
            break;
        }
        entry = entry_next_entry(entry);
        printf("    entry =%d\n", (entry - entry));
    }
    
    if (cluster == -1) {
        printf("can't find file: UBOOT\n");
        return NULL;
    }

    printf("begin read file\n");
    // read uboot
    fbuf = malloc(filesize);
    cnt = 0;
    while (is_endofile(cluster) != 1) {
        cluster_size = mbr_sectors_peer_cluster(inbuf) * mbr_sector_size(inbuf);
        read_content(fbuf + cnt, inbuf, min(filesize - cnt, cluster_size), cluster);
        cluster = next_cluster(fat_base, cluster);
    }
    *fsize = filesize;
    return fbuf;
}

int main(int argc, char const* argv[])
{
    struct stat sb;
    int ubootsize;
    char *uboot;
    int fd, ofd;
    char *buf;
    int bytes;

    printf("    open file : %s \n", argv[1]);
    if ((fd = open(argv[1],  O_RDONLY, (mode_t)(0))) == -1){
        perror("Cannot open filen");
    }
    if (fstat(fd, &sb) == -1) { /* To obtain file size */
        return -1;
    }

    if((buf = malloc(sb.st_size)) == NULL){
        perror("malloc error!");
    }
    
    printf("    read file  \n");
    bytes = read(fd, buf, sb.st_size);
    if ( bytes != sb.st_size){
        perror("partial read or error!");
    }

    printf("    do job  \n");
    uboot = do_job(&ubootsize, buf);

    printf("save uboot to uboot.bin\n");
    if ((ofd = open("uboot.bin",  O_WRONLY | O_CREAT | O_TRUNC, (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) == -1){
        perror("Cannot open filen");
    }
    
    bytes = write(ofd, uboot, ubootsize);
    if (bytes != ubootsize) {
        perror("can't save uboot.bin");
    }
    return 0;
}
