#ifndef DISTRO_H
#define DISTRO_H

#define READ    0
#define WRITE   1

/* uniform [0,1) */
double unirand();

/* 50/50 read and writes */
int getnext_50rw();

/* 95/5 read/write mix */
int getnext_95rw();

uint64_t getnext_seq(uint64_t max);

#endif /* end of include guard: DISTRO_H */
