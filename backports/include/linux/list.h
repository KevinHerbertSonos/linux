#ifndef __BACKPORT_LIST_H
#define __BACKPORT_LIST_H
#include_next <linux/list.h>

#ifndef list_last_entry
/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)
#endif

#endif /* __BACKPORT_LIST_H */
