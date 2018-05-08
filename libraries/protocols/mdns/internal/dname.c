/**
 ******************************************************************************
 * @file    dname.c
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   DNS Name helper functions
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include <stdint.h>
#include <mdns.h>
#include <mdns_port.h>

#include "mdns_private.h"
#include "mdns_message.h"
#include <ctype.h>

/* write the normal c string "label" to "dst" with appropriate dns length and
 * null termination.  Return a pointer to the byte after the last one written,
 * or 0 if the label was too long.
 */
uint8_t *dname_put_label(uint8_t * dst, const char *label)
{
	int len;
	uint8_t *p = dst;

	len = (uint8_t) strlen(label);
	*p++ = len;
	strcpy((char *)p, label);
	return p + len;
}

/* Find the size of a dname.  Note that it may contain pointers.  Note that the
 * size does include the number of bytes taken up by a pointer, but not the
 * size of the data it points to.  It also includes the byte taken up by the
 * terminating 0, if any.  return -1 for invalid names.
 */
int dname_size(uint8_t *dname)
{
	uint8_t *start = dname;
	while (*dname != 0x00) {
		if (IS_POINTER(*dname)) {	/* pointer */
			dname++;
			break;
		}

		if (*dname > MDNS_MAX_LABEL_LEN)
			return -1;

		/* we've found a valid label length */
		dname += *dname + 1;
	}
	return dname - start + 1;
}

/* compare l1 to l2.  return <0, 0, >0 like strcasecmp */
int dname_label_cmp(uint8_t *p1, uint8_t *l1, uint8_t *p2, uint8_t *l2)
{
	int i, min;
	uint8_t *c1, *c2;

	while (IS_POINTER(*l1))
		l1 = p1 + POINTER(l1);

	while (IS_POINTER(*l2))
		l2 = p2 + POINTER(l2);

	min = *l1 > *l2 ? *l2 : *l1;
	c1 = l1 + 1;
	c2 = l2 + 1;

	for (i = 0; i < min; i++) {
		/*Compare ignoring case*/
		if (tolower(*c1) > tolower(*c2))
			return 1;
		if (tolower(*c2) > tolower(*c1))
			return -1;
		c1++;
		c2++;
	}
	if (*l1 > *l2)
		return 1;
	if (*l2 > *l1)
		return -1;
	return 0;
}

/* return a pointer to the next label in name or NULL if this is the end */
uint8_t *dname_label_next(uint8_t * p, uint8_t * n)
{
	while (IS_POINTER(*n))
		n = p + POINTER(n);
	n += *n + 1;

	return *n == 0 ? NULL : n;
}

/* compare the dname name n1 from the raw packet p1 to the dname n2 from the
 * raw packet p2.  Because the dnames may contain pointers, we need the
 * pointers to the raw packets.  Return 0 if the dnames are identical, >0 if n1
 * is lexicographically greater than n2, or <0 if n2 is greater than n1.  If
 * you are absolutely certain that a name doesn't contain pointers, you can
 * pass NULL as it's raw packet.
 */
int dname_cmp(uint8_t *p1, uint8_t *n1, uint8_t *p2, uint8_t *n2)
{
	int ret;

	while (1) {
		/* advance both names to the next valid pointers */
		while (IS_POINTER(*n1))
			n1 = p1 + POINTER(n1);
		while (IS_POINTER(*n2))
			n2 = p2 + POINTER(n2);

		if (*n1 == 0 && *n2 == 0)
			break;

		if (*n1 != 0 && *n2 != 0) {
			/* both n1 and n2 are pointing to labels by now */
			ret = dname_label_cmp(p1, n1, p2, n2);
			if (ret != 0)
				return ret;
		}

		if (*n1 != 0 && *n2 == 0)
			/* we're done with n2 and n1 still has chars */
			return 1;

		if (*n1 == 0 && *n2 != 0)
			/* we're done with n2 and n1 still has chars */
			return -1;

		if (*n1 == 0 && *n2 == 0)
			/* we arrived at the end of both strings and they're still the
			 * same
			 */
			return 0;

		/* no conclusion yet.  advance to the next label */
		n1 += *n1 + 1;
		n2 += *n2 + 1;
	}
	return 0;
}

/* Change the dname foo.local to foo-2.local.  If it already says foo-X.local,
 * make it foo-(X+1).local.  If it says foo-9.local, just leave it alone and
 * return -1.  name must be a valid dns name and must contain at least 2 extra
 * bytes.
 */
int dname_increment(uint8_t *name)
{
	int len = name[0], newlen;

	if (name[len - 1] == '-' && name[len] >= '2' && name[len] < '9') {
		name[len] += 1;
		return 0;
	} else if ((name[len - 2] == '-' || name[len - 3] == '-')
		&& name[len] >= '0' && name[len] < '9') {
		/* Update unit's place: 11 -> 12 */
		name[len] += 1;
		return -1;
	} else if ((name[len - 2] == '-' || name[len - 3] == '-')
		&& name[len] == '9' && name[len - 1] != '9') {
		/* Update 10th place: 19 -> 20 */
		name[len - 1] += 1;
		name[len] = '0';
		return -1;
	} else if (name[len - 2] == '-' && name[len] == '9'
		&& name[len - 1] == '9') {
		/* Change 99 to 100 */
		name[0] = len + 1;
		/* Move entire string after '9' to one position left in order
		 * to accommodate additional '1'. */
		memmove(&name[len + 1], &name[len],
			strlen((char *)name) - len);
		name[len - 1] = '1';
		name[len] = '0';
		name[len + 1] = '0';
		return -1;
	} else if (name[len - 3] == '-' && name[len] == '9'
		&& name[len - 1] == '9') {
		/* Update 100th place: 199 -> 200 */
		name[len - 2] += 1;
		name[len - 1] = '0';
		name[len] = '0';
		return -1;
	} else if (name[len - 1] == '-' && name[len] == '9') {
		/* Change 9 to 10 */
		name[0] = len + 1;
		/* Move entire string after '9' to one position left in order
		 * to accommodate additional '1'. */
		memmove(&name[len + 1], &name[len],
			strlen((char *)name) - len);
		name[len] = '1';
		name[len + 1] = '0';
		return -1;
	}

	newlen = len + 2;
	if (newlen > MDNS_MAX_LABEL_LEN) {
		name[MDNS_MAX_LABEL_LEN - 1] = '-';
		name[MDNS_MAX_LABEL_LEN] = '2';
	} else {
		name[0] = newlen;
		/* don't forget to move trailing 0 */
		memmove(&name[len + 3], &name[len + 1],
			strlen((char *)name) - len);
		name[len + 1] = '-';
		name[len + 2] = '2';
	}
	MDNS_DBG("Derived new name: ");
	debug_print_name(NULL, name);
	MDNS_DBG("\r\n");
	return 0;
}

/* convert the ascii name from a list of labels separated by "sep".  Return the
 * length of the modified name, or -1 if there was an error.  name is not
 * allowed to begin with a separator.  dnames can't conceivable be bigger than
 * UINT16_MAX, if they are, this is an error.  If dest is NULL, the operation
 * is done in-place, otherwise the result is written to dest.
 */
int dnameify(char *name, uint16_t kvlen, uint8_t sep, uint8_t * dest)
{
	char *src, *start, *tail;
	int len, labellen;

	/* now change all of the colons to lengths starting from the last
	 * char
	 */
	len = kvlen - 1;
	if (dest == NULL) {
	    tail = name + kvlen;
		if (name[len] == '\0') {
			start = (char *)name;
			dest = (uint8_t *) name + len;
			src = (char *)dest - 1;
		} else
			return kvlen;
	} else {
	    tail = (char *)dest + kvlen;
		start = (char *)dest;
		dest += len;
		src = name + len - 1;
	}
	len = 0;
	labellen = 0;

	while (1) {
	    /* Single separator, skip it, example key1=1.key2=2..key3=3.*/
		if ((*src == sep && *(src-1) != '/') && labellen == 0){
            memcpy(src, src + 1, tail - (src + 1));
            dest--;
            src--;
		}

		if (dest == (uint8_t *) start || (*src == sep && *(src-1) != '/') ){
			/* This is the beginning of a label.  Update its length and start
			 * looking at the next one.
			 */
			*dest = labellen;

			if (UINT16_MAX - len < labellen)
				return -1;

			len += labellen + 1;
			labellen = 0;

			if (dest == (uint8_t *) start)
				break;
			dest--;
			src--;
			continue;
		}

		/* Alternate separator has a "/", remove it .*/
        if ( *src == sep && *(src - 1) == '/' ) {
            memcpy(src - 1, src, tail - src);
            dest--;
            src--;
        }
		/* move the char down */
		*dest-- = *src--;
		labellen++;
	}
	return len;
}

int dname_copy(uint8_t *dst, uint8_t *p, uint8_t *src)
{
	int len = 0;

	while (*src != 0) {
		/* advance to the next valid data */
		while (IS_POINTER(*src))
			src = p + POINTER(src);
		len += *src + 1;
		if (len >= MDNS_MAX_NAME_LEN) {
			*dst = 0;
			return -1;
		}
		memcpy(dst, src, *src + 1);
		dst += *src + 1;
		src += *src + 1;
	}
	*dst = 0;
	return 0;
}

/* convert the label pointed to by src (which may contain pointers within p) to
 * a c string and write the result to dst.  If keepuscores is 1, leading
 * underscores will be preserved.  Otherwise, leading underscore will not be
 * copied to dst.  Return a pointer to the next label that needs parsing.
 */
uint8_t *dname_label_to_c(char *dst, uint8_t *p,
			  uint8_t *src, int keepuscores)
{
	int len = 0, i;

	while (IS_POINTER(*src))
		src = p + POINTER(src);

	len = *src++;
	if (len >= MDNS_MAX_LABEL_LEN)
		return NULL;

	for (i = 0; i < len; i++) {
		if (keepuscores == 0 && *src == '_') {
			src++;
			continue;
		}
		*dst++ = (char)*src++;
	}
	*dst = 0;
	return src;
}

/* copy the data portion of a txt record pointed to by "txt" with length tlen
 * to dst with dlen and null terminate it.  Add a : between each string of the
 * txt record.  copy at most dlen bytes, including the trailing null.
 */
void txt_to_c_ncpy(char *dst, int dlen, uint8_t * txt, int tlen)
{
	int remaining = dlen - 1;

	while (1) {
		/* any more txt to copy? */
		if (*txt == 0 || tlen == 0) {
			*dst = 0;
			break;
		}

		/* if the next kv pair won't fit, skip it */
		if (*txt > remaining) {
			txt += *txt + 1;
			continue;
		}

		memcpy(dst, txt + 1, *txt);
		remaining -= *txt;
		tlen -= (*txt + 1);
		dst += *txt;
		txt += *txt + 1;
		if (remaining == 0 || remaining == 1 || tlen == 0) {
			*dst = 0;
			break;
		}
		*dst++ = ':';
	}
}

/* would parsing the dname n from the raw packet p overrun the end of the
 * packet e?
 */
int dname_overrun(uint8_t *p, uint8_t *e, uint8_t *n)
{
	while (1) {
		if (n > e)
			return 1;
		if (n <= e && *n == 0)
			return 0;

		while (IS_POINTER(*n)) {
			n = p + POINTER(n);
			if (n > e)
				return 1;
		}
		n += *n + 1;
	}

}

#ifdef MDNS_TESTS
static int c_to_dname(uint8_t *dst, char *src)
{
	uint8_t *label = dst;
	int len = 0;

	dst++;
	while (*src != 0x00) {
		if (len > 63)
			/* label is too long */
			return -1;

		if (*src == '.') {
			*label = len;
			len = 0;
			label = dst;
			dst++;
			src++;
			continue;
		}
		*dst++ = (uint8_t) *src++;
		len++;
	}
	*dst = 0;
	*label = len;
	return 0;
}

/* dname_length tests and test data */
uint8_t p0[] = { 3, 'f', 'o', 'o', 5, 'l', 'o', 'c', 'a', 'l', 0 };

/* p1 contains the name foo.local in a query and a pointer to that name at
 * offset 27.
 */
uint8_t p1[] = { 0xBD, 0x38, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
	0x00,
	0x00, 0x03, 0x66, 0x6F, 0x6F, 0x05, 0x6C, 0x6F, 0x63, 0x61, 0x6C,
	0x00, 0x00, 0x01, 0x00, 0x01, 0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0xFF, 0x00, 0x04, 0xC0, 0xA8, 0x01, 0x51,
};

uint8_t p3[] = { 5, 'f', 'o', 'o', '-', '2', 5, 'l', 'o', 'c', 'a', 'l', 0 };

uint8_t p4[] = { 5, 'k', '1', '=', 'v', '1', 5, 'k', '2', '=', 'k', '3', 0 };

uint8_t p5[] = { 5, 'k', '1', '=', 'v', '1', 5, 'k', '2', '=', 'k', '2', 0 };

uint8_t p6[] = { 5, 'l', 'a', 'b', 'e', 'l', 4, 'w', 'i', 't', 'h', 1, 'a',
	3, 'b', 'a', 'd', 0xC1, 0xFF,
	'p', 'o', 'i', 'n', 't', 'e', 'r', 0
};

void dname_size_tests(void)
{
	int ret;
	ret = dname_size(p0);
	if (ret != 11)
		goto ERROR;	/* Failed to find length of foo.local   */

	ret = dname_size(&p1[27]);
	if (ret != 2)
		goto ERROR;	/* Failed to find length of name with pointer   */

	goto SUCCESS;

ERROR:
	printf("Error");
SUCCESS:
	printf("Success");
}

void dname_cmp_tests(void)
{
	int ret;
	ret = dname_cmp(NULL, p0, NULL, p0);
	if (ret != 0)
		goto ERROR;	/* Failed to cmp a dname to itself      */

	ret = dname_cmp(NULL, p0, p1, &p1[27]);
	if (ret != 0)
		goto ERROR;	/* Failed to cmp identical dnames, one direct one indirect.     */

	ret = dname_cmp(p1, &p1[27], p1, &p1[27]);
	if (ret != 0)
		goto ERROR;	/* Failed to cmp identical indirect dnames.             */

	ret = dname_cmp(p1, &p1[27], NULL, p0);
	if (ret != 0)
		goto ERROR;	/* Failed to cmp identical dnames, one indirect one direct. */

	ret = dname_cmp(p1, &p1[27], NULL, p3);
	if (ret != -1)
		goto ERROR;	/* foo-2.local should be greater than foo.local */

	ret = dname_cmp(NULL, p3, p1, &p1[27]);
	if (ret != 1)
		goto ERROR;	/*      foo-2.local should be greater than foo.local */

	ret = dname_cmp(NULL, p4, NULL, p5);
	if (ret != 1)
		goto ERROR;	/*      k1=v1.k2=v3 should be greater than k1=v1.k2=v2 */

	ret = dname_cmp(NULL, p5, NULL, p4);
	if (ret != -1)
		goto ERROR;	/* k1=v1.k2=v3 should be less than k1=v1.k2=v2  */

	goto SUCCESS;

ERROR:
	printf("Error");
SUCCESS:
	printf("Success");
}

void increment_name_tests(void)
{
	int ret, i;
	uint8_t n1[MDNS_MAX_NAME_LEN];
	uint8_t n2[MDNS_MAX_NAME_LEN];

	/* simple test case */
	ret = c_to_dname(n1, "foo.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert name to mdns name  */

	ret = dname_increment(n1);
	if (ret != 0)
		goto ERROR;	/* Failed to increment foo to foo-2     */

	ret = c_to_dname(n2, "foo-2.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert name to mdns name  */

	ret = strcmp((char *)n1, (char *)n2);
	if (ret != 0)
		goto ERROR;	/* Failed to increment foo to foo-2     */

	/* maximum name */
	ret =
	    c_to_dname(n1,
		       "thisnameisthemaxlabellength000000000000000000000000000000000000.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert longest possible name to mdns name */

	ret = dname_increment(n1);
	if (ret != 0)
		goto ERROR;	/* Failed to increment longest possible name    */

	ret =
	    c_to_dname(n2,
		       "thisnameisthemaxlabellength0000000000000000000000000000000000-2.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert longest possible name to mdns name */

	ret = strcmp((char *)n1, (char *)n2);
	if (ret != 0)
		goto ERROR;	/* Failed to increment longest possible name    */

	/* several increments */
	ret = c_to_dname(n1, "myname.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert name to mdns name  */

	for (i = 0; i < 6; i++)
		dname_increment(n1);
	ret = c_to_dname(n2, "myname-7.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert name to mdns name  */

	ret = strcmp((char *)n1, (char *)n2);
	if (ret != 0)
		goto ERROR;	/* Failed to increment foo to foo-6     */

	/* max increments */
	for (i = 0; i < 2; i++)
		dname_increment(n1);
	ret = c_to_dname(n2, "myname-9.local");
	if (ret != 0)
		goto ERROR;	/* Failed to convert name to mdns name  */

	ret = strcmp((char *)n1, (char *)n2);
	if (ret != 0)
		goto ERROR;	/* Failed to increment foo to foo-9     */

	goto SUCCESS;

ERROR:
	printf("Error");
SUCCESS:
	printf("Success");
}

void txt_to_c_ncpy_tests(void)
{
	uint8_t txt0[] = { 1, 0 };
	uint8_t txt1[] = { 5, 'k', '1', '=', 'v', '1' };
	uint8_t txt2[] = { 5, 'k', '1', '=', 'v', '1', 5, 'k', '2', '=', 'v',
	   '2', 5, 'k', '3', '=', 'v', '3',
	};
	char result[MDNS_MAX_NAME_LEN + 1];
	uint8_t maxtxt[MDNS_MAX_NAME_LEN + 1];
	int i;

	txt_to_c_ncpy(result, sizeof(result), txt0, sizeof(txt0));
	if (strcmp(result, "") != 0)
		goto ERROR;	/* Failed to copy empty TXT record      */

	txt_to_c_ncpy(result, sizeof(result), txt1, sizeof(txt1));
	if (strcmp(result, "k1=v1") != 0)
		goto ERROR;	/* Failed to copy simple TXT record     */

	txt_to_c_ncpy(result, sizeof(result), txt2, sizeof(txt2));
	if (strcmp(result, "k1=v1:k2=v2:k3=v3") != 0)
		goto ERROR;	/* Failed to copy multiple kvs  */

	maxtxt[0] = sizeof(maxtxt) - 1;
	maxtxt[1] = 'k';
	maxtxt[1] = '=';
	for (i = 2; i < sizeof(maxtxt); i++)
		maxtxt[i] = 'v';
	txt_to_c_ncpy(result, sizeof(result), maxtxt, sizeof(maxtxt));
	if (memcmp(result, maxtxt + 1, sizeof(maxtxt) - 1) != 0)
		goto ERROR;	/* Failed to copy max txt       */

	if (result[sizeof(result) - 1] != 0)
		goto ERROR;	/* Failed to NULL terminate result      */

	goto SUCCESS;

ERROR:
	printf("Error");
SUCCESS:
	printf("Success");
}

void dname_tests(void)
{
	dname_size_tests();
	dname_cmp_tests();
	increment_name_tests();
	txt_to_c_ncpy_tests();
}
#endif
