/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#ifndef CFENGINE_XML_WRITER_H
#define CFENGINE_XML_WRITER_H

#include <platform.h>
#include <writer.h>

typedef struct
{
    const char *name;
    const char *value;
} XmlAttribute;

void XmlComment(Writer *writer, const char *comment);

/*
   INSERT START XML ELEMENT -> <tag_name XmlAttribute.name="XmlAttribute.value" ...>
*/
/* TAKE PARAM attr_cnt, STRUCT XmlAttribute, STRUCT XmlAttribute ...  */
void XmlStartTag(Writer *writer, const char *tag_name, int attr_cnt, ...);

void XmlEndTag(Writer *writer, const char *tag_name);

/*
   INSERT XML TAG -> <tag_name XmlAttribute.name="XmlAttribute.value" ...>value</tag_name>
*/
/* TAKE PARAM attr_cnt, STRUCT XmlAttribute, STRUCT XmgAttribute ...  */
void XmlTag(Writer *writer, const char *tag_name, const char *value, int attr_cnt, ...);

/* String content, properly escaped */
void XmlContent(Writer *writer, const char *value);

#endif
