#ifndef SLAB_H
#define SLAB_H

#include <stdio.h>

struct slab_dir;
struct slab_entry;

/* init the slab subsystem */
void slab_init();

/* allocate a slab_dir */
struct slab_dir* slab_dir_init(unsigned int cid, size_t *laddr);

/* allocate and free persistent memory */
void *slab_palloc(unsigned int cid, unsigned int size);
void slab_pfree(unsigned int cid, void *maddr);

/* checkpoint/commit changes in current transaction */
void slab_cpoint(unsigned int cid, int type);

/* restore (mmap) the entire slab_dir */
struct slab_dir* slab_map(unsigned int cid, size_t laddr, int type);

/* set/get root of the container */
size_t slab_setroot(unsigned int cid, void *maddr);
void *slab_getroot(unsigned int cid);

/* fix the target addresses of all persistent pointers */
void slab_fixptrs(unsigned int cid);

/* make all data pages read-only */
void slab_mprotect_datapgs(unsigned int cid, int prot);

/* store metadata for the persistent pointer located at ptr_loc */
void slab_insert_pointer(unsigned int cid, void **ptr_loc);

#endif /* end of include guard: SLAB_H */
