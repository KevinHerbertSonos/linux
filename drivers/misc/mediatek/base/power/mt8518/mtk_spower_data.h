/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MT_SPOWER_CPU_H
#define MT_SPOWER_CPU_H

#define VSIZE 6
#define TSIZE 18

/***************************/
/* "(WAT -8.6%)	   */
/* Leakage Power"	   */
/***************************/
#define CA53_TABLE_0                                                           \
	/**/ {900, 950, 1000, 1050, 1100, 1150,  \
		20, 41, 53, 68, 88, 113, 144,  \
		25, 49, 63, 80, 103, 133, 171,  \
		30, 57, 74, 95, 122, 157, 203,  \
		35, 68, 87, 111, 143, 185, 240,  \
		40, 83, 106, 135, 172, 219, 279, \
		45, 98, 124, 159, 202, 258, 331,  \
		50, 115, 147, 186, 237, 305, 392,  \
		55, 136, 173, 219, 279, 359, 465,  \
		60, 162, 205, 260, 331, 425, 558,  \
		65, 191, 240, 306, 389, 501, 661,    \
		70, 225, 284, 360, 458, 591, 784,  \
		75, 266, 334, 423, 539, 697, 929,  \
		80, 298, 377, 480, 618, 816, 1108,  \
		85, 351, 444, 565, 727, 962, 1313,  \
		90, 414, 523, 664, 855, 1135, 1557,  \
		95, 488, 616, 782, 1006, 1339, 1845,  \
		100, 576, 726, 919, 1183, 1579, 2186,  \
		105, 679, 854, 1082, 1392, 1862, 2592}

/******************/
/* "(WAT -3%)	  */
/* Leakage Power" */
/******************/
#define CA53_TABLE_1                                                           \
	/**/ {900, 950, 1000, 1050, 1100, 1150,  \
		20, 10, 13, 16, 21, 27, 35,  \
		25, 12, 15, 20, 25, 32, 41,  \
		30, 15, 19, 24, 30, 38, 49,  \
		35, 19, 23, 29, 36, 45, 58,  \
		40, 22, 28, 35, 44, 56, 70,  \
		45, 27, 34, 42, 53, 67, 84,  \
		50, 34,  41, 51, 64, 79, 99,  \
		55, 42, 51, 62, 77, 95, 118,  \
		60, 47, 59, 74, 92, 114, 142,  \
		65, 58, 72, 89, 110, 136, 169,  \
		70, 72, 88, 108, 132, 162, 200,  \
		75, 89, 107, 130, 158, 193, 234,  \
		80, 129, 141, 160, 187, 222, 267,  \
		85, 159, 172, 193, 225, 265, 317,  \
		90, 197, 209, 234, 270, 317, 376,  \
		95, 243, 256, 283, 324, 378, 446,  \
		100, 300, 312, 343, 389, 451, 529,  \
		105, 371, 381, 415, 467, 539, 628}

/******************/
/* "(WAT -2.5%)	  */
/* Leakage Power" */
/******************/
#define CA53_TABLE_2                                                           \
	/**/ {900, 950, 1000, 1050, 1100, 1150,  \
		20, 2, 3, 4, 6, 8, 11,  \
		25, 3, 4, 5, 6, 9, 12,  \
		30, 4, 4, 6, 8, 10, 14,  \
		35, 4, 5, 7, 9, 12, 16,  \
		40, 6, 8, 9, 12, 16, 21,  \
		45, 7, 9, 11, 14, 18, 24,  \
		50, 9, 11, 13, 17, 21, 27,  \
		55, 11, 13, 16, 20, 25, 31,  \
		60, 14, 16, 20, 25, 31, 39,  \
		65, 16,  20, 24, 29, 36, 45,  \
		70, 20, 24, 28, 34, 42, 52,  \
		75, 24, 29, 34, 40, 49, 60,  \
		80, 25, 28, 32, 38, 45, 55,  \
		85, 31, 33, 38, 44, 53, 64,  \
		90, 37, 40, 45, 52, 61, 73,  \
		95, 45, 48, 54, 61, 71, 84,  \
		100, 55, 58, 64, 72, 83, 97,  \
		105, 67, 70, 76, 85, 97, 111}

int ca53_data[][VSIZE * TSIZE + VSIZE + TSIZE] = {
	CA53_TABLE_0, CA53_TABLE_1, CA53_TABLE_2,
};

struct spower_raw_t {
	int vsize;
	int tsize;
	int table_size;
	int *table[];
};

struct spower_raw_t ca53_spower_raw = {
	.vsize = VSIZE,
	.tsize = TSIZE,
	.table_size = 3,
	.table = {(int *)&ca53_data[0], (int *)&ca53_data[1],
		  (int *)&ca53_data[2]},
};

struct vrow_t {
	int mV[VSIZE];
};

struct trow_t {
	int deg;
	int mA[VSIZE];
};


struct sptbl_t {
	int vsize;
	int tsize;
	/* array[VSIZE + TSIZE + (VSIZE*TSIZE)]; */
	int *data;
	/* pointer to voltage row of data */
	struct vrow_t *vrow;
	/* pointer to temperature row of data */
	struct trow_t *trow;
};

#define trow(tab, ti)		((tab)->trow[ti])
#define mA(tab, vi, ti)	((tab)->trow[ti].mA[vi])
#define mV(tab, vi)		((tab)->vrow[0].mV[vi])
#define deg(tab, ti)		((tab)->trow[ti].deg)
#define vsize(tab)		((tab)->vsize)
#define tsize(tab)		((tab)->tsize)
#define tab_validate(tab)	((tab)->data != NULL)

static inline void spower_tab_construct
	(struct sptbl_t (*tab)[], struct spower_raw_t *raw)
{
	int i;
	struct sptbl_t *ptab = (struct sptbl_t *)tab;

	for (i = 0; i < raw->table_size; i++) {
		ptab->vsize = raw->vsize;
		ptab->tsize = raw->tsize;
		ptab->data = raw->table[i];
		ptab->vrow = (struct vrow_t *)ptab->data;
		ptab->trow = (struct trow_t *)(ptab->data + ptab->vsize);
		ptab++;
	}
}

#define MAX_TABLE_SIZE 5

/**
 * @argument
 * dev: the enum of MT_SPOWER_xxx
 * voltage: the operating voltage
 * degree: the Tj
 * @return
 *  -1, means sptab is not yet ready.
 *  other value: the mW of leakage value.
 **/
extern int mt_spower_get_leakage(int dev, int voltage, int degree);

#endif


