/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_ITEM_LIB_H
#define CFENGINE_ITEM_LIB_H

#include <cf3.defs.h>
#include <writer.h>
#include <file_lib.h>

struct Item_
{
    char *name;
    char *classes;
    int counter;
    time_t time;
    Item *next;
};

typedef enum
{
    ITEM_MATCH_TYPE_LITERAL_START,
    ITEM_MATCH_TYPE_LITERAL_COMPLETE,
    ITEM_MATCH_TYPE_LITERAL_SOMEWHERE,
    ITEM_MATCH_TYPE_REGEX_COMPLETE,
    ITEM_MATCH_TYPE_LITERAL_START_NOT,
    ITEM_MATCH_TYPE_LITERAL_COMPLETE_NOT,
    ITEM_MATCH_TYPE_LITERAL_SOMEWHERE_NOT,
    ITEM_MATCH_TYPE_REGEX_COMPLETE_NOT
} ItemMatchType;

void PrintItemList(const Item *list, Writer *w);
void PrependFullItem(Item **liststart, const char *itemstring, const char *classes, int counter, time_t t);
Item *ReturnItemIn(Item *list, const char *item);
Item *ReturnItemInClass(Item *list, const char *item, const char *classes);
Item *ReturnItemAtIndex(Item *list, int index);
Item *EndOfList(Item *start);
void PrependItemList(Item **liststart, const char *itemstring);
void InsertAfter(Item **filestart, Item *ptr, const char *string);
bool RawSaveItemList(const Item *liststart, const char *filename, NewLineMode new_line_mode);
Item *RawLoadItemList(const char *filename);
Item *SplitStringAsItemList(const char *string, char sep);
Item *SplitString(const char *string, char sep);
int DeleteItemGeneral(Item **filestart, const char *string, ItemMatchType type);
int DeleteItemLiteral(Item **filestart, const char *string);
int DeleteItemStarting(Item **list, const char *string);
int DeleteItemNotStarting(Item **list, const char *string);
int DeleteItemMatching(Item **list, const char *string);
int DeleteItemNotMatching(Item **list, const char *string);
int DeleteItemContaining(Item **list, const char *string);
int DeleteItemNotContaining(Item **list, const char *string);
int ListLen(const Item *list);
bool IsItemIn(const Item *list, const char *item);
bool ListsCompare(const Item *list1, const Item *list2);
bool ListSubsetOfList(const Item *list1, const Item *list2);
int IsMatchItemIn(const Item *list, const char *item);
Item *ConcatLists(Item *list1, Item *list2);
void CopyList(Item **dest, const Item *source);
void IdempItemCount(Item **liststart, const char *itemstring, const char *classes);
Item *IdempPrependItem(Item **liststart, const char *itemstring, const char *classes);
Item *IdempPrependItemClass(Item **liststart, const char *itemstring, const char *classes);
Item *ReverseItemList(Item *list); /* Eats list, spits it out reversed. */
Item *PrependItem(Item **liststart, const char *itemstring, const char *classes);
/* Warning: AppendItem()'s cost is proportional to list length; it is
 * usually cheaper to build a list using PrependItem, then reverse it;
 * building it with AppendItem() is quadratic in length. */
void AppendItem(Item **liststart, const char *itemstring, const char *classes);
void DeleteItemList(Item *item);
void DeleteItem(Item **liststart, Item *item);
void IncrementItemListCounter(Item *ptr, const char *string);
void SetItemListCounter(Item *ptr, const char *string, int value);
char *ItemList2CSV(const Item *list);
size_t ItemList2CSV_bound(const Item *list, char *buf, size_t buf_size, char separator);
int ItemListSize(const Item *list);
bool IsInterfaceAddress(const Item *ip_addresses, const char *adr);

#endif
