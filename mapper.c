/*
 * mapper.c -- simple file that mmap()s a file region and prints it
 *
 * Copyright (C) 1998,2000,2001 Alessandro Rubini
 * 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <stropts.h>
#include <asm-generic/ioctl.h>
#include <time.h>

/***** DEFINES ******/
#define DMA_MAGIC		0xdd
#define DMA_PREPARE		_IOW(DMA_MAGIC, 0, struct DmaControlBlock *)
#define DMA_KICK		_IOW(DMA_MAGIC, 1, struct DmaControlBlock *)
#define DMA_WAIT_ONE	_IOW(DMA_MAGIC, 2, struct DmaControlBlock *)
#define DMA_WAIT_ALL	_IO(DMA_MAGIC, 3)

struct DmaControlBlock
{
	unsigned int m_transferInfo;
	void *m_pSourceAddr;
	void *m_pDestAddr;
	unsigned int m_xferLen;
	unsigned int m_tdStride;
	struct DmaControlBlock *m_pNext;
	unsigned int m_blank1, m_blank2;
};

#define MY_ASSERT(x) if (!(x)) { *(int *)0 = 0; }

inline void CopyLinear(struct DmaControlBlock *pCB,
		void *pDestAddr, void *pSourceAddr, unsigned int length, unsigned int srcInc)
{
	MY_ASSERT(pCB);
	MY_ASSERT(pDestAddr);
	MY_ASSERT(pSourceAddr);
	MY_ASSERT(length > 0 && length <= 0x3fffffff);
	MY_ASSERT(srcInc == 0 || srcInc == 1);

	if (srcInc)
	{
		unsigned long source_start = (unsigned long)pSourceAddr >> 12;
		unsigned long source_end = (unsigned long)(pSourceAddr + length - 1) >> 12;

		if (source_start != source_end)
		{
			fprintf(stderr, "linear source range straddles page boundary %p->%p, %lx->%lx\n",
					pSourceAddr, pSourceAddr + length, source_start, source_end);

			if (source_end - source_start > 1)
				fprintf(stderr, "\tstraddles %ld pages\n", source_end - source_start);
		}
	}

	unsigned long dest_start = (unsigned long)pDestAddr >> 12;
	unsigned long dest_end = (unsigned long)(pDestAddr + length - 1) >> 12;

	if (dest_start != dest_end)
	{
		fprintf(stderr, "linear dest range straddles page boundary %p->%p, %lx->%lx\n",
				pDestAddr, pDestAddr + length, dest_start, dest_end);

		if (dest_end - dest_start > 1)
				fprintf(stderr, "\tstraddles %ld pages\n", dest_end - dest_start);
	}

	pCB->m_transferInfo = (srcInc << 8) | (1 << 4) | (5 << 12) | (1 << 9) | (1 << 5);
	pCB->m_pSourceAddr = pSourceAddr;
	pCB->m_pDestAddr = pDestAddr;
	pCB->m_xferLen = length;
	pCB->m_tdStride = 0xffffffff;
	pCB->m_pNext = 0;
	pCB->m_blank1 = pCB->m_blank2 = 0;
}

int main(int argc, char **argv)
{
    char *fname;
    FILE *f;
    void *address;
    int err;
    
    const unsigned int transfer_size = 75 * 1024 * 1024;
    
    srand(time(0));

    

    fname=argv[1];

    if (!(f=fopen(fname,"r+b"))) {
        fprintf(stderr, "%s: %s: %s\n", argv[0], fname, strerror(errno));
        exit(1);
    }

    address=mmap(0, transfer_size * 2 + 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(f), 0);

    if (address == (void *)-1) {
        fprintf(stderr,"%s: mmap(): %s\n",argv[0],strerror(errno));
        exit(1);
    }

   memset(address, 0xcd, transfer_size * 2 + 4096);
        
   unsigned char *pSrc = (unsigned char *)address + 4096;
   unsigned char *pDst = pSrc + transfer_size;
   int count;
   
  //if (0)
  /*fprintf(stderr, "0\n");
   for (count = 0; count < 16; count++)
   {
      pSrc[count] = rand();
      pDst[count] = rand();
      
      fprintf(stderr, "%d: %d %d\n", count, pSrc[count], pDst[count]);
   }
   fprintf(stderr, "4096\n");
   for (count = 4096; count < 4096+16; count++)
   {
      pSrc[count] = rand();
      pDst[count] = rand();
      
      fprintf(stderr, "%d: %d %d\n", count, pSrc[count], pDst[count]);
   }*/
   
   struct DmaControlBlock *pHead = (struct DmaControlBlock *)address;
            
   for (count = 0; count < transfer_size / 4096; count++)
   {
      CopyLinear(&pHead[count], &pDst[count * 4096], &pSrc[count * 4096], 4096, 1);
      pHead[count].m_pNext = &pHead[count + 1];
   }
   pHead[count - 1].m_pNext = 0;

    time_t start = clock();
    err = ioctl(fileno(f), DMA_PREPARE, address);
    if (err == -1)
    	fprintf(stderr, "dma prepare err %d\n", errno);
    time_t mid = clock();
    
    err = ioctl(fileno(f), DMA_KICK, address);
    if (err == -1)
    	fprintf(stderr, "dma kick err %d\n", errno);

    time_t end = clock();

    fprintf(stderr, "prepare took %.3f ms, kick took %.3f ms\n",
	(float)(end - mid) / CLOCKS_PER_SEC * 1000.0f,
	(float)(mid - start) / CLOCKS_PER_SEC * 1000.0f);
    
 /*   fprintf(stderr, "0\n");
   for (count = 0; count < 16; count++)
   {
      fprintf(stderr, "%d: %d %d\n", count, pSrc[count], pDst[count]);
   }
   fprintf(stderr, "4096\n");
   for (count = 4096; count < 4096+16; count++)
   {
      fprintf(stderr, "%d: %d %d\n", count, pSrc[count], pDst[count]);
   }*/

    
    fclose(f);
    return 0;
}
        
