
#include "diskio.h"		/* Declarations of disk I/O functions */
#include "ff.h"			/* Declarations of API */
#if CFG_USE_SDCARD_HOST

#if _FATFS != 108	/* Revision ID */
#error Wrong include file (ff.h).
#endif

#if _VOLUMES >= 1 || _VOLUMES <= 10
static
FATFS *FatFs[_VOLUMES];	/* Pointer to the file system objects (logical drives) */
#else
#error Number of volumes must be 1 to 10.
#endif

static
uint16 Fsid;				/* File system mount ID */

#define	ExFatContiguousClus	0x03
/*-----------------------------------------------------------------------*/
/* String functions                                                      */
/*-----------------------------------------------------------------------*/

/* Copy memory to memory */
//static
void mem_cpy (void* dst, const void* src, uint32 cnt) {
    uint8 *d = (uint8*)dst;
    const uint8 *s = (const uint8*)src;
    while (cnt--)
        *d++ = *s++;
}

/* Fill memory */
void mem_set (void* dst, int val, uint32 cnt) {
    uint8 *d = (uint8*)dst;
    while (cnt--)
        *d++ = (uint8)val;
}

/*-----------------------------------------------------------------------*/
/* Change window offset                                                  */
/*-----------------------------------------------------------------------*/
FRESULT move_window (
	FATFS *fs,		/* File system object */
	uint32 sector	/* Sector number to make apperance in the fs->win[] */
)					/* Move to zero only writes back dirty window */
{
    uint32 wsect;
    wsect = fs->winsect;
    if (wsect != sector) {	/* Changed current window */
        if (sector) {
            if (disk_read(fs->drive, fs->win, sector, 1) != RES_OK)
                return FR_DISK_ERR;
            fs->winsect = sector;
        }
    }
    return FR_OK;
}


/*-----------------------------------------------------------------------*/
/* Get sector# from cluster#                                             */
/*-----------------------------------------------------------------------*/
static uint32 clust2sect (	FATFS* fs,uint32 clst)
{
	clst -= 2;
	if (clst >= (fs->max_clust - 2)) 
		return 0;
	return (clst * fs->csize + fs->database);
}


/*-----------------------------------------------------------------------*/
/* FAT access - Read value of a FAT entry                                */
/*-----------------------------------------------------------------------*/
uint32 get_fat (	/* 0xFFFFFFFF:Disk error, 1:Internal error, Else:Cluster status */
	FATFS* fs,	/* File system object */
	uint32 clst	/* Cluster# to get the link information */
)
{
	uint32 wc, bc;
	uint8 *p;

	if (clst < 2 || clst >= fs->max_clust)	/* Check range */
		return 1;

	switch (fs->fs_type) 
	{
	case FS_FAT12 :
		bc = (uint32)clst; bc += bc / 2;
		if (move_window(fs, fs->fatbase + (bc / SS(fs)))) break;
		wc = fs->win[bc % SS(fs)]; bc++;
		if (move_window(fs, fs->fatbase + (bc / SS(fs)))) break;
		wc |= fs->win[bc % SS(fs)] << 8;
		return clst & 1 ? wc >> 4 : (wc & 0xFFF);

	case FS_FAT16 :
		if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 2)))) break;
		p = &fs->win[clst * 2 % SS(fs)];
		return LD_WORD(p);
	case FS_ExFAT :
	case FS_FAT32 :	
		if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4)))) break;
		p = &fs->win[clst * 4 % SS(fs)];
		return LD_DWORD(p) & 0x0FFFFFFF;

	default:
		return 1;
	}

	return 0xFFFFFFFF;	/* An error occurred at the disk I/O layer */
}

/*-----------------------------------------------------------------------*/
/* Check if the file/directory object is valid or not                    */
/*-----------------------------------------------------------------------*/
static
FRESULT validate (FATFS * fs)
{
	if(!fs || !fs->fs_type)
		return FR_INVALID_OBJECT;

	return FR_OK;
}

/*-----------------------------------------------------------------------*/
/* Seek File R/W Pointer                                                 */
/*-----------------------------------------------------------------------*/
FRESULT f_lseek (
	FIL *fp,		/* Pointer to the file object */
	uint32 ofs		/* File pointer from top of file */
)
{
	FRESULT res;
	uint32 clst, bcs, nsect, ifptr;


	res = validate(fp->fs);		/* Check validity of the object */
	if (res != FR_OK) 
		return res;

	if (ofs > fp->fsize	) 
		ofs = fp->fsize;

	ifptr = fp->fptr;
	fp->fptr = nsect = 0; 
	fp->csect = 255;
	
	if (ofs > 0) 
	{
		bcs = (uint32)fp->fs->csize * SS(fp->fs);	/* Cluster size (byte) */
		if (ifptr > 0 &&(ofs - 1) / bcs >= (ifptr - 1) / bcs) 
		{	/* When seek to same or following cluster, */
			fp->fptr = (ifptr - 1) & ~(bcs - 1);	/* start from the current cluster */
			ofs -= fp->fptr;
			clst = fp->curr_clust;
		} 
		else 
		{	/* When seek to back cluster, */
			clst = fp->org_clust;/* start from the first cluster */
			fp->curr_clust = clst;
		}
		if (clst != 0) 
		{
			while (ofs > bcs) 
			{	/* Cluster following loop */
				if((fp->fs->fs_type == FS_ExFAT)&&(fp->ExNoChainFlag == ExFatContiguousClus))
					clst++;
				else
					clst = get_fat(fp->fs, clst);
					
				if (clst == 0xFFFFFFFF) 
					return FR_DISK_ERR;
				if (clst <= 1 || clst >= fp->fs->max_clust) 
					return FR_INT_ERR;
				fp->curr_clust = clst;
				fp->fptr += bcs;
				ofs -= bcs;
			}
			fp->fptr += ofs;
			fp->csect = (uint8)(ofs / SS(fp->fs));	/* Sector offset in the cluster */
			if (ofs % SS(fp->fs)) 
			{
				nsect = clust2sect(fp->fs, clst);	/* Current sector */
				if (!nsect) 
					return FR_INT_ERR;
				nsect += fp->csect;
				fp->csect++;
			}
		}
	}
	
	if (fp->fptr % SS(fp->fs) && nsect != fp->dsect) 
	{
		if (disk_read(fp->fs->drive, fp->buf, nsect, 1) != RES_OK)
			return FR_DISK_ERR;
		fp->dsect = nsect;
	}
	return res;
}

/*-----------------------------------------------------------------------*/
/* Directory handling - Seek directory index                             */
/*-----------------------------------------------------------------------*/
//static
FRESULT dir_seek (
	DIR *dj,		/* Pointer to directory object */
	uint16 idx		/* Directory index number */
)
{
	uint32 clst;
	uint16 ic;
	
	dj->index = idx;

	if((dj->fs->fs_type == FS_ExFAT) && (dj->ExNoChainFlag == ExFatContiguousClus))
	{
		ic = SS(dj->fs) / 32 * dj->fs->csize;	/* Entries per cluster */
		clst = dj->sclust;
		if (clst >= dj->fs->max_clust)
			return FR_INT_ERR;
		while(idx >= ic)
		{
			clst++;
			idx -= ic;
		}
		dj->clust=clst;
		dj->sect = clust2sect(dj->fs, dj->clust)+ idx / (SS(dj->fs) / 32);
	}
	else
	{
		clst = dj->sclust;
		if (clst == 1 || clst >= dj->fs->max_clust)
			return FR_INT_ERR;
		if (!clst && ((dj->fs->fs_type == FS_FAT32)||(dj->fs->fs_type== FS_ExFAT)))
			clst = dj->fs->dirbase;
		if (clst == 0) 
		{
			dj->clust = clst;
			if (idx >= dj->fs->n_rootdir)
				return FR_INT_ERR;
			dj->sect = dj->fs->dirbase + idx / (SS(dj->fs) / 32);
		}
		else 
		{
			ic = SS(dj->fs) / 32 * dj->fs->csize;	/* Entries per cluster */
			while (idx >= ic) 
			{
				clst = get_fat(dj->fs, clst);				/* Get next cluster */
				if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	
				if (clst < 2 || clst >= dj->fs->max_clust)	/* Reached to end of table or int error */
					return FR_INT_ERR;
				idx -= ic;
			}
			dj->clust = clst;
			dj->sect = clust2sect(dj->fs, clst) + idx / (SS(dj->fs) / 32);
		}
	}
	
	dj->dir = dj->fs->win + (idx % (SS(dj->fs) / 32)) * 32;	/* Ptr to the entry in the sector */
	return FR_OK;
}

/*-----------------------------------------------------------------------*/
/* Load a sector and check if it is an (Ex)FAT boot sector                   */
/* output: 0-FAT boot sector,(foud fat16 or fat32)
           1-Exfat,
           2-Not boot sector,
   		   3-Valid boot record but not fat/exfat,
   		   4-Disk error			*/
/*-----------------------------------------------------------------------*/
static
uint8 check_fs (FATFS* fs,uint32 sect)
{
	if (disk_read(fs->drive, fs->win, sect, 1) != RES_OK)
		return 4;
	
	if (LD_WORD(&fs->win[BS_55AA]) != 0xAA55)
		return 2;
	if(LD_DWORD(&fs->win[BPB_ExFatName]) == 0x54414658 )/*check "XFAT" string*/
		return 1;
	if ((LD_DWORD(&fs->win[BS_FilSysType]) & 0xFFFFFF) == 0x544146)	/* Check "FAT" string */
		return 0;
	if ((LD_DWORD(&fs->win[BS_FilSysType32]) & 0xFFFFFF) == 0x544146)/* Check "FAT" string */
		return 0;
	return 3;
}

/*-----------------------------------------------------------------------*/
/* Read File                                                             */
/*-----------------------------------------------------------------------*/
FRESULT f_read (
	FIL *fp, 		/* Pointer to the file object */
	void *buff,		/* Pointer to data buffer */
	uint32 btr,		/* Number of bytes to read */
	uint32 *br		/* Pointer to number of bytes read */
)
{
	FRESULT res;
	uint32 clst, sect, remain;
	uint32 rcnt = 0, cc;
	uint8 *rbuff = buff;

	*br = 0;	/* Initialize bytes read */
	res = validate(fp->fs);	/* Check validity of the object */
	if (res != FR_OK) 
		return res;
	if (!(fp->flag & FA_READ)) 	/* Check access mode */
		return FR_DENIED;
	
	remain = fp->fsize - fp->fptr;
	if (btr > remain) 
		btr = (uint32)remain;/* Truncate btr by remaining bytes */

	for ( ;btr;rbuff += rcnt, fp->fptr += rcnt, *br += rcnt, btr -= rcnt) 
	{
		if ((fp->fptr % SS(fp->fs)) == 0) 
		{	/* On the sector boundary? */
			if (fp->csect >= fp->fs->csize) 
			{	/* On the cluster boundary? */
				if((fp->fs->fs_type==FS_ExFAT)&&(fp->ExNoChainFlag==ExFatContiguousClus))
				{
					clst=fp->org_clust + ((fp->fptr + _MAX_SS-1)/_MAX_SS + fp->fs->csize - 1)/(fp->fs->csize);
				}
				else
					clst = (fp->fptr == 0)?fp->org_clust : get_fat(fp->fs, fp->curr_clust);
				if (clst <= 1) 
					return FR_INT_ERR;
				if (clst == 0xFFFFFFFF) 
					return FR_DISK_ERR;
				fp->curr_clust = clst;	/* Update current cluster */
				fp->csect = 0;			/* Reset sector offset in the cluster */
			}
			sect = clust2sect(fp->fs, fp->curr_clust);	/* Get current sector */
			if (!sect)
				return FR_INT_ERR;
			sect += fp->csect;
			cc = btr / SS(fp->fs);	/* When remaining bytes >= sector size, */
			if (cc) 
			{	/* Read maximum contiguous sectors directly */
				if (fp->csect + cc > fp->fs->csize)	/* Clip at cluster boundary */
					cc = fp->fs->csize - fp->csect;
				if (disk_read(fp->fs->drive, rbuff, sect, (uint8)cc) != RES_OK)
					return FR_DISK_ERR;
				fp->csect += (uint8)cc;	/* Next sector address in the cluster */
				rcnt = SS(fp->fs) * cc;	/* Number of bytes transferred */
				continue;
			}
			if (fp->dsect != sect) 
			{/* Fill sector buffer with file data */
				if (disk_read(fp->fs->drive, fp->buf, sect, 1) != RES_OK)
					return FR_DISK_ERR;
			}
			fp->dsect = sect;
			fp->csect++;/* Next sector address in the cluster */
		}
		rcnt = SS(fp->fs) - (fp->fptr % SS(fp->fs));/* Get partial sector data from sector buffer */
		if (rcnt > btr) 
			rcnt = btr;
		mem_cpy(rbuff, &fp->buf[fp->fptr % SS(fp->fs)], rcnt);	/* Pick partial sector */
	}
	return FR_OK;
}

/*-----------------------------------------------------------------------
Close File.                                                            
input: fp -- Pointer to the file object to be closed 
------------------------------------------------------------------------*/
FRESULT f_close ( FIL *fp)
{
	FRESULT res;
	res = validate(fp->fs);
	if (res == FR_OK) 
		fp->fs = NULL;
	return res;
}

/* FR_OK(0): successful, !=0: any error occured */
FRESULT chk_mounted_con (FATFS *rfs, uint8 type)
{
	uint8 fmt, *tbl;
	DSTATUS stat;
	uint32 bsect, fsize, tsect, mclst;
	FATFS *fs;

	fs = rfs;
	if (!fs)
		return FR_NOT_ENABLED;	/* Is the file system object available? */
	if (fs->fs_type)
	{
		stat = disk_status(fs->drive);
		if (!(stat & STA_NOINIT)) 
			return FR_OK;	/* The file system object is valid */
	}

	fs->fs_type = 0;
	fs->drive = type;
	stat = disk_initialize(fs->drive);	/* Initialize low level disk I/O layer */
	FAT_PRT("newfat 1\r\n");
	if (stat & STA_NOINIT)
		return FR_NOT_READY;

	fmt = check_fs(fs, bsect = 0);	
	FAT_PRT("newfat 2:%d\r\n",fmt);

	if (fmt == 3)
	{	/* Not an FAT boot record, it may be patitioned */
			tbl = &fs->win[MBR_Table + LD2PT(vol) * 16];

		if (tbl[4])
		{
			bsect = LD_DWORD(&tbl[8]);
			fmt = check_fs(fs, bsect);
		}
	}
	
	if (fmt == 4)
		return	FR_NO_FILESYSTEM; 
	if((fmt==2)||(fmt==3))
		return FR_DISK_ERR;
	
	if(fmt==1)/*exfat*/
	{
		if((1 << (fs->win[BPB_ExBytePerSecSft])) != SS(fs))
			return FR_NO_FILESYSTEM;
		/* Initialize the file system object */
		fs->sects_fat = LD_DWORD(fs->win+BPB_ExFatLength);
		fs->n_fats	= fs->win[BPB_ExNumofFATs];
		fs->fatbase = bsect + LD_DWORD(fs->win+BPB_ExFatOffset);////
		fs->csize = 1 << (fs->win[BPB_ExSecPerClusSft]);
		fs->max_clust= LD_DWORD(fs->win+BPB_ExClusterCnt);
		fs->dirbase = LD_DWORD(fs->win+BPB_ExRootDirBase);
		fs->database = bsect + LD_DWORD(fs->win+BPB_ExClusHPOffset);
		fs->n_rootdir = 0;
		fs->fs_type = FS_ExFAT;
		fs->winsect = 0;
	}
	else
	{
	    /*  BPS段的处理， BIOS Parameter Block */
        // 扇区字节数 512byte常见
		if (fmt || LD_WORD(fs->win+BPB_BytsPerSec) != SS(fs))
			return FR_NO_FILESYSTEM;
		/* Initialize the file system object */
		fsize = LD_WORD(fs->win+BPB_FATSz16);
		if (!fsize) // 每FAT所占的扇区数， 只被FAT12/16使用，FAT32必须为0 
            fsize = LD_DWORD(fs->win+BPB_FATSz32);
		fs->sects_fat= fsize;
		fs->n_fats = fs->win[BPB_NumFATs];// FAT数，分区上FAT的副本数，一般为2	 				
		fsize *= fs->n_fats;

        // 保留扇区数
		fs->fatbase = bsect + LD_WORD(fs->win+BPB_RsvdSecCnt); 
		fs->csize = fs->win[BPB_SecPerClus]; // 每簇扇区数
		// 根目录可容纳的最多目录项数，只被FAT12/16使用，FAT32必须为0 
		fs->n_rootdir = LD_WORD(fs->win+BPB_RootEntCnt);
        // 小扇区数(小于32MB存于此，大于32MB的存放在0x20-23)，
        // 只被FAT12/16使用，FAT32必须为0 
		tsect = LD_WORD(fs->win+BPB_TotSec16);
		if (!tsect) // 总扇区数 FAT32分区中总的扇区数
            tsect = LD_DWORD(fs->win+BPB_TotSec32);
		fs->max_clust= mclst = (tsect						
			- LD_WORD(fs->win+BPB_RsvdSecCnt) - fsize - fs->n_rootdir / (SS(fs)/32)
			) / fs->csize + 2;

		fmt = FS_FAT12;									
		if (mclst >= 0xFF7) 
          fmt = FS_FAT16;	
		if (mclst >= 0xFFF7) 
          fmt = FS_FAT32;

        // 根目录簇号 FAT32 一般是 2
		if (fmt == FS_FAT32)
			fs->dirbase = LD_DWORD(fs->win+BPB_RootClus);
		else
			fs->dirbase = fs->fatbase + fsize;
		fs->database = fs->fatbase + fsize + fs->n_rootdir / (SS(fs)/32);
		fs->fs_type = fmt;
		fs->winsect = 0;
	}
	
	fs->id = ++Fsid;
	FAT_PRT("FAT INIT OK!!!\r\n");
	return FR_OK;
}

FRESULT f_EOF(FIL *fp )
{
	if(fp->fsize <= fp->fptr)
		return FR_FILE_END;
	else
		return FR_OK;
		
}

/*-----------------------------------------------------------------------*/
/* Mount/Unmount a Logical Drive                                         */
/*-----------------------------------------------------------------------*/
FRESULT f_mount (
	uint8 vol,    /* Logical drive number to be mounted/unmounted */ 
	FATFS* fs	/* Pointer to the file system object (NULL:unmount)*/
)
{
    FATFS *cfs;

    if (vol >= _VOLUMES) 
        return FR_INVALID_DRIVE;

    cfs = FatFs[vol];					/* Pointer to fs object */

    if (cfs) 
    cfs->fs_type = 0;				/* Clear old fs object */

    if (fs) 
    fs->fs_type = 0;				/* Clear new fs object */

    FatFs[vol] = fs;					/* Register new fs object */

    return FR_OK;
}

/* Pointer to new file system object (NULL for unmount)*/
FRESULT f_unmount (FATFS *fs)
{
	mem_set(fs,0, sizeof(FATFS));
	return FR_OK;
}

#endif

#if CFG_USE_FTPD_UPGRADE

FRESULT f_close (
	FIL* fp		/* Pointer to the file object to be closed */
)
{
	return FR_OK;
}

FRESULT f_getcwd (
	TCHAR* buff,	/* Pointer to the directory path */
	uint32 len		/* Size of path */
)
{
	return FR_OK;
}

FRESULT f_open (
	FIL* fp,			/* Pointer to the blank file object */
	const TCHAR* path,	/* Pointer to the file name */
	uint8 mode			/* Access mode and file open mode flags */
)
{
	return FR_OK;
}

FRESULT f_opendir (
	DIR* dp,			/* Pointer to directory object to create */
	const TCHAR* path	/* Pointer to the directory path */
)
{
	return FR_OK;
}

FRESULT f_read (
	FIL* fp, 	/* Pointer to the file object */
	void* buff,	/* Pointer to data buffer */
	uint32 btr,	/* Number of bytes to read */
	uint32* br	/* Pointer to number of bytes read */
)
{
	return FR_OK;
}

FRESULT f_readdir (
	DIR* dp,			/* Pointer to the open directory object */
	FILINFO* fno		/* Pointer to file information to return */
)
{
	return FR_OK;
}

FRESULT f_stat (
	const TCHAR* path,	/* Pointer to the file path */
	FILINFO* fno		/* Pointer to file information to return */
)
{
	return FR_OK;
}

FRESULT f_write (
	FIL* fp,			/* Pointer to the file object */
	const void* buff,	/* Pointer to the data to be written */
	uint32 btw,			/* Number of bytes to write */
	uint32* bw			/* Pointer to number of bytes written */
)
{
	return FR_OK;
}

FRESULT f_chdir (
	const TCHAR* path	/* Pointer to the directory path */
)
{
	return FR_OK;
}

FRESULT f_mkdir (
	const TCHAR* path		/* Pointer to the directory path */
)
{
	return FR_OK;
}

FRESULT f_rename (
	const TCHAR* path_old,	/* Pointer to the object name to be renamed */
	const TCHAR* path_new	/* Pointer to the new name */
)
{
	return FR_OK;
}

FRESULT f_unlink (
	const TCHAR* path		/* Pointer to the file or directory path */
)
{
	return FR_OK;
}

FRESULT f_eof(FIL *fp )
{
	if(fp->fsize <= fp->fptr)
		return FR_FILE_END;
	else
		return FR_OK;
		
}
#endif

