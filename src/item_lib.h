/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_ITEM_LIB_H
#define CFENGINE_ITEM_LIB_H

int PrintItemList(char *buffer, int bufsize, Item *list);
void PrependFullItem(Item **liststart, char *itemstring, char *classes, int counter, time_t t);
void PurgeItemList(Item **list, char *name);
Item *ReturnItemIn(Item *list, const char *item);
Item *ReturnItemInClass(Item *list, char *item, char *classes);
Item *ReturnItemAtIndex(Item *list, int index);
int GetItemIndex(Item *list, char *item);
Item *EndOfList(Item *start);
int IsItemInRegion(char *item, Item *begin, Item *end, Attributes a, Promise *pp);
void PrependItemList(Item **liststart, const char *itemstring);
int SelectItemMatching(Item *s, char *regex, Item *begin, Item *end, Item **match, Item **prev, char *fl);
int SelectNextItemMatching(char *regexp, Item *begin, Item *end, Item **match, Item **prev);
int SelectLastItemMatching(char *regexp, Item *begin, Item *end, Item **match, Item **prev);
void InsertAfter(Item **filestart, Item *ptr, char *string);
int NeighbourItemMatches(Item *start, Item *location, char *string, enum cfeditorder pos, Attributes a, Promise *pp);
int RawSaveItemList(Item *liststart, char *file);
Item *SplitStringAsItemList(char *string, char sep);
Item *SplitString(const char *string, char sep);
int DeleteItemGeneral(Item **filestart, const char *string, enum matchtypes type);
int DeleteItemLiteral(Item **filestart, const char *string);
int DeleteItemStarting(Item **list, char *string);
int DeleteItemNotStarting(Item **list, char *string);
int DeleteItemMatching(Item **list, char *string);
int DeleteItemNotMatching(Item **list, char *string);
int DeleteItemContaining(Item **list, char *string);
int DeleteItemNotContaining(Item **list, char *string);
int CompareToFile(Item *liststart, char *file, Attributes a, Promise *pp);
Item *String2List(char *string);
int ListLen(Item *list);
int ByteSizeList(const Item *list);
bool IsItemIn(Item *list, const char *item);
int IsMatchItemIn(Item *list, char *item);
Item *ConcatLists(Item *list1, Item *list2);
void CopyList(Item **dest, Item *source);
void IdempItemCount(Item **liststart, const char *itemstring, const char *classes);
Item *IdempPrependItem(Item **liststart, const char *itemstring, const char *classes);
Item *IdempPrependItemClass(Item **liststart, char *itemstring, char *classes);
Item *PrependItem(Item **liststart, const char *itemstring, const char *classes);
void AppendItem(Item **liststart, const char *itemstring, const char *classes);
void DeleteItemList(Item *item);
void DeleteItem(Item **liststart, Item *item);
void DebugListItemList(Item *liststart);
Item *SplitStringAsItemList(char *string, char sep);
void IncrementItemListCounter(Item *ptr, char *string);
void SetItemListCounter(Item *ptr, char *string, int value);
char *ItemList2CSV(Item *list);
int ItemListSize(Item *list);
int MatchRegion(char *chunk, Item *location, Item *begin, Item *end);
Item *DeleteRegion(Item **liststart, Item *begin, Item *end);

#endif
